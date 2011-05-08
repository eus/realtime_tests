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

#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include "utility_cpu.h"
#include "utility_log.h"
#include "utility_file.h"

const char prog_name[] = "utility_cpu_test";
FILE *log_stream;

static volatile int stop_infinite_loop = 0;
static void signal_handler(int signo)
{
  stop_infinite_loop = 1;
}

int main(int argc, char **argv, char **envp)
{
  log_stream = stderr;

  /* Testcase 1: process migration is prevented */
  pid_t child_pid = fork();
  if (child_pid == 0) {

    assert(enter_UP_mode() == 0); /* Enter UP mode ASAP to prevent any
				     migration statistics to be larger
				     than 0 */

    struct sigaction signal_handler_data = {
      .sa_handler = signal_handler,
    };

    if (sigaction(SIGINT, &signal_handler_data, NULL) != 0) {
      fatal_syserror("Cannot register SIGINT handler");
    }

    while (!stop_infinite_loop);

    return EXIT_SUCCESS;

  } else if (child_pid == -1) {
    fatal_syserror("[Testcase 1] Cannot do fork()");
  } else {

    sleep(10); /* Wait for some statistics to build up */

    char linux_process_info_path[1024];
    snprintf(linux_process_info_path, sizeof(linux_process_info_path),
	     "/proc/%u/sched", (unsigned int) child_pid);
    FILE *linux_process_info
      = utility_file_open_for_reading(linux_process_info_path);

    char *buffer = NULL;
    size_t buffer_len = 0;
    int rc = 0;
    const char keyword1[] = "se.statistics.nr_failed_migrations_affine";
    const size_t keyword1_len = strlen(keyword1);
    int keyword1_count = -1;
    const char keyword2[] = "se.statistics.nr_failed_migrations_running";
    const size_t keyword2_len = strlen(keyword2);
    int keyword2_count = -1;
    const char keyword3[] = "se.statistics.nr_forced_migrations";
    const size_t keyword3_len = strlen(keyword3);
    int keyword3_count = -1;
    unsigned int keyword_hit_mask = 0;
    while ((rc = utility_file_readln(linux_process_info, &buffer, &buffer_len,
				     1024)) == 0) {
#define get_keyword_count(keyword_no) do {				\
	if (strncmp(buffer, keyword ## keyword_no,			\
		    keyword ## keyword_no ## _len) == 0			\
	    && (isspace(buffer[keyword ## keyword_no ## _len])		\
		|| buffer[keyword ## keyword_no ## _len] == ':')) {	\
	  char *ptr = buffer + keyword ## keyword_no ## _len;		\
	  while (*ptr != ':' && *ptr != '\0') {				\
	    ptr++;							\
	  }								\
	  if (*ptr != ':') {						\
	    break; /* the format is unexpected */			\
	  }								\
	  ptr++;							\
	  while (isspace(*ptr)) {					\
	    ptr++;							\
	  }								\
	  keyword ## keyword_no ## _count = atoi(ptr);			\
	  keyword_hit_mask |= 1 << (keyword_no - 1);			\
	}								\
      } while (0)

      get_keyword_count(1);
      get_keyword_count(2);
      get_keyword_count(3);

      if (keyword_hit_mask == 0x7) {
	break;
      }

#undef get_keyword_count
    }
    if (buffer != NULL) {
      free(buffer);
    }
    utility_file_close(linux_process_info, linux_process_info_path);
    if (kill(child_pid, SIGINT) != 0) {
      fatal_syserror("Cannot stop child process with pid %u", child_pid);
    }

    assert(rc != -2);
    assert(keyword_hit_mask == 0x7);
    assert(keyword1_count != 0);
    assert(keyword2_count == 0);
    assert(keyword3_count == 0);
  }

  /* Testcase 2: check that the last CPU ID is correct */
  FILE *linux_cpuinfo = fopen("/proc/cpuinfo", "r");
  assert(linux_cpuinfo != NULL);
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
  assert(fclose(linux_cpuinfo) == 0);
  assert(get_last_cpu() == (cpu_count - 1));

  /* Testcase 3: setting Linux governor of CPU0 */
  const char linux_governor_file_path[]
    = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor";

  /* Read current governor */
  FILE *linux_governor_file
    = utility_file_open_for_reading(linux_governor_file_path);
  assert(linux_governor_file != NULL);
  char *buffer1 = NULL;
  size_t buffer1_len = 0;
  assert(utility_file_readln(linux_governor_file, &buffer1, &buffer1_len, 32)
	 == 0);
  utility_file_close(linux_governor_file, linux_governor_file_path);

  /* Use the library function to keep the current governor */
  cpu_freq_governor *used_gov = cpu_freq_get_governor(0);
  assert(used_gov != NULL);

  /* Change the governor */
  linux_governor_file = utility_file_open_for_writing(linux_governor_file_path);
  assert(linux_governor_file != NULL);
  fprintf(linux_governor_file, "performance");
  assert(utility_file_close(linux_governor_file, linux_governor_file_path)
	 == 0);

  /* Use the library function to restore the governor */
  assert(cpu_freq_restore_governor(used_gov) == 0);

  /* Check that the governor is indeed restored */
  linux_governor_file = utility_file_open_for_reading(linux_governor_file_path);
  assert(linux_governor_file != NULL);
  char *buffer2 = NULL;
  size_t buffer2_len = 0;
  assert(utility_file_readln(linux_governor_file, &buffer2, &buffer2_len, 32)
	 == 0);
  utility_file_close(linux_governor_file, linux_governor_file_path);
  assert(strcmp(buffer1, buffer2) == 0);

  /* Clean up */
  free(buffer1);
  free(buffer2);

  /* Testcase 4: check cpu_freq_available() */
  /* Open the frequency file */
  const char linux_freq_file_path[]
    = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies";
  FILE *linux_freq_file = utility_file_open_for_reading(linux_freq_file_path);
  assert(linux_freq_file != NULL);

  /* Read in all frequencies */
  buffer1 = NULL;
  buffer1_len = 0;
  assert(utility_file_readln(linux_freq_file, &buffer1, &buffer1_len, 32) == 0);
  assert(utility_file_close(linux_freq_file, linux_freq_file_path) == 0);

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
  assert(freq_count == expected_freq_count);
  assert(freqs != NULL);
  assert(freqs[0] == (strtoull(buffer1, NULL, 10) * 1000));

  /* Check descending order */
  int i;
  for (i = 1; i < freq_count; i++) {
    assert(freqs[i - 1] >= freqs[i]);
  }

  /* Clean-up */
  free(buffer1);
  free(freqs);

  /* Testcase 5: check cpu_freq_set() and cpu_freq_get() */
  used_gov = cpu_freq_get_governor(0);

  freqs = cpu_freq_available(0, &freq_count);
  cpu_freq_set(0, freqs[0]);
  sleep(10); /* Allow for visual inspection through GNOME applet, for example */

  const char linux_curr_freq_file_path[]
    = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq";
  FILE *linux_curr_freq_file
    = utility_file_open_for_reading(linux_curr_freq_file_path);
  assert(linux_curr_freq_file != NULL);
  buffer1 = NULL;
  buffer1_len = 0;
  assert(utility_file_readln(linux_curr_freq_file, &buffer1, &buffer1_len, 32)
	 == 0);
  utility_file_close(linux_curr_freq_file, linux_curr_freq_file_path);
  assert (cpu_freq_get(0) == strtoull(buffer1, NULL, 10) * 1000);
  
  cpu_freq_restore_governor(used_gov);

  free(buffer1);
  free(freqs);

  return EXIT_SUCCESS;
}
