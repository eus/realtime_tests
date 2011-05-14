/*****************************************************************************
 * Copyright (C) 2011  Tadeus Prastowo (eus@member.fsf.org)                  *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation, either version 3 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/

#define _GNU_SOURCE /* sched_getcpu(), pthread_getaffinity_np() */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include "utility_testcase.h"
#include "utility_cpu.h"
#include "utility_log.h"
#include "utility_file.h"
#include "utility_time.h"
#include "utility_sched_fifo.h"

static cpu_freq_governor *used_gov = NULL;
static int used_gov_in_use = 0;
static pid_t child_pid = 0;
static void cleanup(void)
{
  if (used_gov_in_use) {
    int rc;
    if ((rc = cpu_freq_restore_governor(used_gov)) != 0) {
      log_error("You must restore the CPU freq governor yourself");
    }
  }
  if (child_pid != 0) {
    if (kill(child_pid, SIGINT) != 0 && errno != ESRCH) {
      log_syserror("You must kill child %d yourself", child_pid);
    }
  }
}

static volatile int stop_infinite_loop = 0;
static void signal_handler(int signo)
{
  stop_infinite_loop = 1;
}

MAIN_UNIT_TEST_BEGIN("utility_cpu_test", "stderr", NULL, cleanup)
{
  if (argc != 2) {
    fprintf(stderr,
	    "Usage: %s UNDER_VALGRIND\n"
	    "Set UNDER_VALGRIND to 1 if this is run under Valgrind.\n"
	    "Otherwise, set it to 0. This is because some testcases\n"
	    "are expected to fail under Valgrind but not when run\n"
	    "normally.\n", prog_name);
    return EXIT_FAILURE;
  }

#define sample_linux_process_info()                                     \
  do {                                                                  \
    sleep(1); /* Sample the statistics after locking has happened */    \
    char linux_process_info_path[1024];                                 \
    snprintf(linux_process_info_path, sizeof(linux_process_info_path),  \
             "/proc/%d/sched", (int) child_pid);                        \
    FILE *linux_process_info                                            \
      = utility_file_open_for_reading(linux_process_info_path);         \
    unsigned keyword_hit_count = 0;                                     \
    unsigned keyword_hit_count_expected = (sizeof(keyword_value)        \
                                           / sizeof(*keyword_value));   \
                                                                        \
    int sample_migration_statistics(const char *line, void *args) {     \
      const char **keyword = keywords;                                  \
      while (*keyword != NULL) {                                        \
        size_t keyword_len = strlen(*keyword);                          \
                                                                        \
        if (strncmp(line, *keyword, keyword_len) == 0                   \
            && (isspace(line[keyword_len])                              \
                || line[keyword_len] == ':')) {                         \
          char *ptr = strchr(line + keyword_len, ':');                  \
          gracious_assert(ptr != NULL);                                 \
          ptr++;                                                        \
          while (isspace(*ptr)) {                                       \
            ptr++;                                                      \
          }                                                             \
          keyword_value[keyword - keywords] = atoi(ptr);                \
          keyword_hit_count++;                                          \
        }                                                               \
                                                                        \
        keyword++;                                                      \
      }                                                                 \
                                                                        \
      return 0;                                                         \
    }                                                                   \
    gracious_assert(utility_file_read(linux_process_info, 1024,         \
                                      sample_migration_statistics,      \
                                      NULL) == 0);                      \
    utility_file_close(linux_process_info, linux_process_info_path);    \
    gracious_assert(keyword_hit_count == keyword_hit_count_expected);   \
                                                                        \
    sleep(9); /* Wait for some statistics to build up */                \
    linux_process_info                                                  \
      = utility_file_open_for_reading(linux_process_info_path);         \
    keyword_hit_count = 0;                                              \
    int ensure_locked_to_cpu(const char *line, void *args) {            \
      const char **keyword = keywords;                                  \
      while (*keyword != NULL) {                                        \
        size_t keyword_len = strlen(*keyword);                          \
                                                                        \
        if (strncmp(line, *keyword, keyword_len) == 0                   \
            && (isspace(line[keyword_len])                              \
                || line[keyword_len] == ':')) {                         \
          char *ptr = strchr(line + keyword_len, ':');                  \
          gracious_assert(ptr != NULL);                                 \
          ptr++;                                                        \
          while (isspace(*ptr)) {                                       \
            ptr++;                                                      \
          }                                                             \
                                                                        \
          unsigned expected_value = keyword_value[keyword - keywords];  \
          gracious_assert_msg(atoi(ptr) == expected_value,              \
                              "%s: %d != %d",                           \
                              *keyword, atoi(ptr), expected_value);     \
                                                                        \
          keyword_hit_count++;                                          \
        }                                                               \
                                                                        \
        keyword++;                                                      \
      }                                                                 \
                                                                        \
      return 0;                                                         \
    }                                                                   \
    gracious_assert(utility_file_read(linux_process_info, 1024,         \
                                      ensure_locked_to_cpu, NULL)       \
                    == 0);                                              \
    utility_file_close(linux_process_info, linux_process_info_path);    \
    gracious_assert(keyword_hit_count == keyword_hit_count_expected);   \
  } while (0)

#define check_that_locking_indeed_happens()             \
  do {                                                  \
    int keyword_value[] = {-1, -1, -1};                 \
    const char *keywords[] = {                          \
      "se.statistics.nr_failed_migrations_running",     \
      "se.statistics.nr_failed_migrations_hot",         \
      "se.statistics.nr_forced_migrations",             \
      NULL                                              \
    };                                                  \
    sample_linux_process_info();                        \
  } while (0)

#define check_that_unlocking_indeed_happens()           \
  do {                                                  \
    int keyword_value[] = {-1};                         \
    const char *keywords[] = {                          \
      "se.statistics.nr_failed_migrations_affine",      \
      NULL                                              \
    };                                                  \
    sample_linux_process_info();                        \
  } while (0)

  /* Testcase 1: process migration is prevented */
  child_pid = fork();
  if (child_pid == 0) {

    gracious_assert(enter_UP_mode() == 0); /* Enter UP mode ASAP to prevent any
					      migration statistics to be larger
					      than 0 */

    struct sigaction signal_handler_data = {
      .sa_handler = signal_handler,
    };

    if (sigaction(SIGINT, &signal_handler_data, NULL) != 0) {
      fatal_syserror("Cannot register SIGINT handler");
    }

    while (!stop_infinite_loop) {
      gracious_assert(sched_getcpu() == 0);
    }

    return EXIT_SUCCESS;
  } else {
    gracious_assert(child_pid != -1);

    check_that_locking_indeed_happens();

    if (kill(child_pid, SIGINT) != 0) {
      fatal_syserror("Cannot stop child process with pid %u", child_pid);
    }
    child_pid = 0;
    check_subprocess_exit_status(EXIT_SUCCESS);
  }

  /* Testcase 2: check that the last CPU ID is correct */
  FILE *linux_cpuinfo = fopen("/proc/cpuinfo", "r");
  gracious_assert(linux_cpuinfo != NULL);
  char buffer[1024];
  const char *keyword = "processor";
  int cpu_count = 0;
  while (fgets(buffer, sizeof(buffer), linux_cpuinfo) != NULL) {
    if (strncmp(buffer, keyword, strlen(keyword)) == 0
	&& (isspace(buffer[strlen(keyword)])
	    || buffer[strlen(keyword)] == ':')) {
      cpu_count++;
    }
  }
  gracious_assert(fclose(linux_cpuinfo) == 0);
  gracious_assert(get_last_cpu() == (cpu_count - 1));

  /* Testcase 3: setting Linux governor of CPU0 */
  const char linux_governor_file_path[]
    = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor";

  /* Read current governor */
  FILE *linux_governor_file
    = utility_file_open_for_reading(linux_governor_file_path);
  gracious_assert(linux_governor_file != NULL);
  char *buffer1 = NULL;
  size_t buffer1_len = 0;
  gracious_assert(utility_file_readln(linux_governor_file,
				      &buffer1, &buffer1_len, 32) == 0);
  utility_file_close(linux_governor_file, linux_governor_file_path);

  /* Use the library function to keep the current governor */
  used_gov = cpu_freq_get_governor(0);
  gracious_assert(used_gov != NULL);

  /* Change the governor */
  linux_governor_file = utility_file_open_for_writing(linux_governor_file_path);
  gracious_assert(linux_governor_file != NULL);
  used_gov_in_use = 1;
  fprintf(linux_governor_file, "performance");
  gracious_assert(utility_file_close(linux_governor_file,
				     linux_governor_file_path) == 0);

  /* Use the library function to restore the governor */
  gracious_assert(cpu_freq_restore_governor(used_gov) == 0);
  used_gov_in_use = 0;

  /* Check that the governor is indeed restored */
  linux_governor_file = utility_file_open_for_reading(linux_governor_file_path);
  gracious_assert(linux_governor_file != NULL);
  char *buffer2 = NULL;
  size_t buffer2_len = 0;
  gracious_assert(utility_file_readln(linux_governor_file,
				      &buffer2, &buffer2_len, 32) == 0);
  utility_file_close(linux_governor_file, linux_governor_file_path);
  gracious_assert(strcmp(buffer1, buffer2) == 0);

  /* Clean up */
  free(buffer1);
  free(buffer2);

  /* Testcase 4: check cpu_freq_available() */
  /* Open the frequency file */
  const char linux_freq_file_path[]
    = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies";
  FILE *linux_freq_file = utility_file_open_for_reading(linux_freq_file_path);
  gracious_assert(linux_freq_file != NULL);

  /* Read in all frequencies */
  buffer1 = NULL;
  buffer1_len = 0;
  gracious_assert(utility_file_readln(linux_freq_file,
				      &buffer1, &buffer1_len, 32) == 0);
  gracious_assert(utility_file_close(linux_freq_file, linux_freq_file_path)
		  == 0);

  /* Count the frequencies */
  const char delim[] = " ";
  size_t expected_freq_count = 0;
  buffer2 = strtok(buffer1, delim);
  while (buffer2 != NULL) {
    expected_freq_count++;
    if (strtoull(buffer1, NULL, 10) < strtoull(buffer2, NULL, 10))  {
      buffer1 = buffer2; /* Track the greatest frequency */
    }
    buffer2 = strtok(NULL, delim);
  }

  /* Use cpu_freq_available() */
  ssize_t freq_count = -1;
  unsigned long long *freqs = cpu_freq_available(0, &freq_count);
  gracious_assert(freq_count == expected_freq_count);
  gracious_assert(freqs != NULL);
  gracious_assert(freqs[0] == (strtoull(buffer1, NULL, 10) * 1000));

  /* Check descending order */
  int i;
  for (i = 1; i < freq_count; i++) {
    gracious_assert(freqs[i - 1] >= freqs[i]);
  }

  /* Clean-up */
  free(buffer1);
  free(freqs);

  /* Testcase 5: check cpu_freq_set() and cpu_freq_get() */
  /* Save current governor */
  used_gov = cpu_freq_get_governor(0);
  gracious_assert(used_gov != NULL);

  /* Set the CPU frequency */
  freqs = cpu_freq_available(0, &freq_count);
  gracious_assert(freq_count >= 1);

  /* Set the frequency to the lowest one so that if the frequency
     gets raised by another computational intensive thread that
     interferes this thread, the failure of cpu_freq_set() will be
     obvious. */
  gracious_assert(cpu_freq_set(0, freqs[freq_count - 1]) == 0);
  used_gov_in_use = 1;

  /* Manually read the current frequency */
  const char linux_curr_freq_file_path[]
    = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq";
  FILE *linux_curr_freq_file
    = utility_file_open_for_reading(linux_curr_freq_file_path);
  gracious_assert(linux_curr_freq_file != NULL);
  buffer1 = NULL;
  buffer1_len = 0;
  gracious_assert(utility_file_readln(linux_curr_freq_file,
				      &buffer1, &buffer1_len, 32) == 0);
  utility_file_close(linux_curr_freq_file, linux_curr_freq_file_path);

  /* Check that cpu_freq_get() agrees with the manually read frequency */
  gracious_assert(cpu_freq_get(0) == freqs[freq_count - 1]);
  gracious_assert(cpu_freq_get(0) == strtoull(buffer1, NULL, 10) * 1000);
  
  /* Restore the saved governor */
  gracious_assert(cpu_freq_restore_governor(used_gov) == 0);
  used_gov_in_use = 0;

  /* Clean-up */
  free(buffer1);
  free(freqs);

  /* Testcase 6: check the accuracy of the busy loop
   * [Upon FAILURE] If this testcase fails, you need to adjust
   * the following parameters of the CPU busyloop object to be
   * created:
   */
  child_pid = fork();
  if (child_pid == 0) {
    utility_time duration;
    utility_time_init(&duration);
    to_utility_time(1, s, &duration);
    utility_time search_tolerance;
    utility_time_init(&search_tolerance);
    to_utility_time(1, us, &search_tolerance);
    unsigned search_max_passes = 10;

    /* Save current governor */
    gracious_assert(enter_UP_mode() == 0);
    used_gov = cpu_freq_get_governor(0);
    gracious_assert(used_gov != NULL);

    /* Set the CPU frequency */
    freqs = cpu_freq_available(0, &freq_count);
    gracious_assert(freq_count >= 1);
    gracious_assert(cpu_freq_set(0, freqs[0]) == 0);
    used_gov_in_use = 1;

    /* Create CPU busyloop object */
    cpu_busyloop *busyloop_obj = NULL;
    gracious_assert(create_cpu_busyloop(0, &duration, &search_tolerance,
					search_max_passes, &busyloop_obj) == 0);
    gracious_assert(busyloop_obj != NULL);

    /* Check busyloop_obj properties */
    gracious_assert(cpu_busyloop_id(busyloop_obj) == 0);
    gracious_assert(cpu_busyloop_frequency(busyloop_obj) == freqs[0]);
    gracious_assert(utility_time_eq_gc(cpu_busyloop_duration(busyloop_obj),
				       to_utility_time_dyn(1, s)));

    /* Prevent interference on the following time-sensitive section */
    struct scheduler default_scheduler;
    gracious_assert(sched_fifo_enter_max(&default_scheduler) == 0);

    struct timespec t_begin, t_end;
    /* Stabilizing the cache */
    int rc_t_begin = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_begin);
    int rc_t_end = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_end);
    /* End of stabilizing the cache */

    /* Time the measurement duration */
#define to_ns(t) (t.tv_sec * 1000000000ULL + t.tv_nsec)
    rc_t_begin += clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_begin);
    rc_t_end += clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_end);
    unsigned long long measurement_duration = to_ns(t_end) - to_ns(t_begin);
#undef to_ns
    /* End of timing the measurement duration */

    /* Time keep_cpu_busy() */
    rc_t_begin += clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_begin);
    keep_cpu_busy(busyloop_obj);
    rc_t_end += clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_end);
    /* End of timing keep_cpu_busy() */

    gracious_assert(sched_fifo_leave(&default_scheduler) == 0);
    /* End of time-sensitive section */

    /* Check the accuracy of keep_cpu_busy() */
    gracious_assert(rc_t_begin == 0);
    gracious_assert(rc_t_end == 0);
    utility_time duration_actual;
    utility_time_init(&duration_actual);
    utility_time_sub_gc(timespec_to_utility_time_dyn(&t_end),
			timespec_to_utility_time_dyn(&t_begin),
			&duration_actual);
    utility_time_sub_gc(&duration_actual,
			to_utility_time_dyn(measurement_duration, ns),
			&duration_actual);

    utility_time *lower_bound = utility_time_sub_dyn(&duration,
						     &search_tolerance);
    utility_time *upper_bound = utility_time_add_dyn(&duration,
						     &search_tolerance);
    if (utility_time_lt_gc_t2(&duration_actual, lower_bound)
	|| utility_time_gt_gc_t2(&duration_actual, upper_bound)) {
      /* If this is run under Valgrind, this is expected.  If Valgrind
	 reports no leak, run this again without Valgrind. If this
	 messages persists, this testcase really fails. */

      gracious_assert(strcmp(argv[1], "1") == 0);
    }

    /* Clean-up */
    free(freqs);
    destroy_cpu_busyloop(busyloop_obj);
    gracious_assert(cpu_freq_restore_governor(used_gov) == 0);
    used_gov_in_use = 0;

    return EXIT_SUCCESS;
  } else {
    gracious_assert(child_pid != -1);
    check_subprocess_exit_status(EXIT_SUCCESS);
    child_pid = 0;
  }

  /* Testcase 7: check host_byte_order()*/
  int byte_order_test = 0xFEEDBEEF;
  char byte_order_test_probe = ((char *) &byte_order_test)[0];
  switch (host_byte_order()) {
  case CPU_LITTLE_ENDIAN:
    gracious_assert(byte_order_test_probe == (char) 0xEF);
    break;
  case CPU_BIG_ENDIAN:
    gracious_assert(byte_order_test_probe == (char) 0xFE);
    break;
  default:
    gracious_assert_msg(0, "Unknown endianness");
  }

  /* Testcase 8: check enter_UP_mode_freq_max() */
  child_pid = fork();
  if (child_pid == 0) {

    gracious_assert(enter_UP_mode_freq_max(&used_gov) == 0);
    used_gov_in_use = 1;

    struct sigaction signal_handler_data = {
      .sa_handler = signal_handler,
    };
    gracious_assert (sigaction(SIGINT, &signal_handler_data, NULL) == 0);

    ssize_t freqs_len = 0;
    unsigned long long *freqs = cpu_freq_available(0, &freqs_len);
    gracious_assert(freqs_len >= 1);
    unsigned long long max_freq = freqs[0];
    free(freqs);

    /* This checking of max_freq is not robust because if there is
       another cpu-bound thread that elevates the frequency of the
       CPU, there is no way to know whether enter_UP_mode_freq_max()
       really sets the frequency to the maximum one. Provided that the
       elevation of CPU frequency is only determined by the volume of
       instructions executed, this problem can be solved by having a
       kernel patch that pegs the status of this thread to RUNNING for
       a certain duration of time. In this way, this thread will not
       execute any instruction while preventing another cpu-bound
       thread for taking the CPU and elevating the frequency. */
    gracious_assert(cpu_freq_get(0) == max_freq);

    while (!stop_infinite_loop) {
      gracious_assert(sched_getcpu() == 0);
    }
    
    gracious_assert(cpu_freq_restore_governor(used_gov) == 0);
    used_gov_in_use = 0;

    return EXIT_SUCCESS;
  } else {
    gracious_assert(child_pid != -1);

    check_that_locking_indeed_happens();

    gracious_assert(kill(child_pid, SIGINT) == 0);
    child_pid = 0;
    check_subprocess_exit_status(EXIT_SUCCESS);
  }

  /* Testcase 9: check unlock_me() */
  child_pid = fork();
  if (child_pid == 0) {
    struct sigaction signal_handler_data = {
      .sa_handler = signal_handler,
    };
    gracious_assert (sigaction(SIGINT, &signal_handler_data, NULL) == 0);

    int last_cpu_id = get_last_cpu();
    gracious_assert(last_cpu_id != -1);

    gracious_assert(lock_me_to_cpu(last_cpu_id) == 0);
    while (!stop_infinite_loop) {
      gracious_assert(sched_getcpu() == last_cpu_id);
    }

    gracious_assert(unlock_me() == 0);
    while (!stop_infinite_loop);

    return EXIT_SUCCESS;
  } else {
    gracious_assert(child_pid != -1);

    check_that_locking_indeed_happens();
    gracious_assert(kill(child_pid, SIGINT) == 0);

    check_that_unlocking_indeed_happens();
    gracious_assert(kill(child_pid, SIGINT) == 0);

    child_pid = 0;
    check_subprocess_exit_status(EXIT_SUCCESS);
  }

  return EXIT_SUCCESS;

} MAIN_UNIT_TEST_END
