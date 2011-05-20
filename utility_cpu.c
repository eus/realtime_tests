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

#define _GNU_SOURCE /* pthread_setaffinity_np(), CPU_*, etc. */

#include "utility_cpu.h"

int lock_me_to_cpu(int which_cpu)
{
  cpu_set_t affinity_mask;

  CPU_ZERO(&affinity_mask);
  CPU_SET(which_cpu, &affinity_mask);

  if (pthread_setaffinity_np(pthread_self(),
                             sizeof(affinity_mask), &affinity_mask) != 0) {
    log_syserror("Cannot set CPU affinity");
    return -1;
  }

  return 0;
}

int unlock_me(void)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  int last_cpu_id = get_last_cpu();
  if (last_cpu_id == -1) {
    log_error("Fail to obtain the ID of the last CPU");
    return -1;
  }

  int i;
  for(i = 0; i <= last_cpu_id; i++) {
    CPU_SET(i, &cpuset);
  }

  if ((errno = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset))
      != 0) {
    log_syserror("Fail to set affinity to all CPUs");
    return -1;
  }

  return 0;
}

int get_last_cpu(void)
{
  const char linux_cpuinfo_path[] = "/proc/cpuinfo";
  FILE *linux_cpuinfo = utility_file_open_for_reading(linux_cpuinfo_path);
  if (linux_cpuinfo == NULL) {
    log_error("Cannot open %s for reading", linux_cpuinfo_path);
    return -1;
  }

  int rc = 0;
  char *buffer = NULL;
  size_t buffer_len = 0;
  int proc_count = 0;
  const char keyword[] = "processor";
  const size_t keyword_len = strlen(keyword);

  while ((rc = utility_file_readln(linux_cpuinfo, &buffer, &buffer_len, 1024))
         == 0) {
    if (strncmp(buffer, keyword, keyword_len) == 0
        && (isspace(buffer[keyword_len]) || buffer[keyword_len] == ':')) {
      proc_count++;
    }
  }

  if (buffer != NULL) {
    free(buffer);
  }

  utility_file_close(linux_cpuinfo, linux_cpuinfo_path);

  if (rc == -2) {
    log_error("Error while reading %s", linux_cpuinfo_path);
    return -1;
  }

  --proc_count;
  if (proc_count < 0) {
    log_error("/proc/cpuinfo contains no 'processor' key");
  }

  return proc_count;
}

#define LINUX_GOVERNOR_FILE_PATH_FORMAT                         \
  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor"

cpu_freq_governor *cpu_freq_get_governor(int which_cpu)
{
  cpu_freq_governor *result = malloc(sizeof (cpu_freq_governor));
  if (result == NULL) {
    log_error("Insufficient memory to create cpu_freq_governor object");
    return NULL;
  }
  result->which_cpu = which_cpu;

  char linux_governor_file_path[1024];
  snprintf(linux_governor_file_path, sizeof(linux_governor_file_path),
           LINUX_GOVERNOR_FILE_PATH_FORMAT, which_cpu);
  FILE *linux_governor_file
    = utility_file_open_for_reading(linux_governor_file_path);
  if (linux_governor_file == NULL) {
    log_error("Cannot open '%s' for reading", linux_governor_file_path);
    free(result);
    return NULL;
  }

  char *buffer = NULL;
  size_t buffer_len = 0;
  int rc = utility_file_readln(linux_governor_file, &buffer, &buffer_len, 32);
  utility_file_close(linux_governor_file, linux_governor_file_path);

  if (rc == 0) {
    result->linux_cpu_governor_name = buffer;
    return result;
  } else if (rc == -1) {
    log_error("No governor exists");
  } else {
    log_error("Fail to read '%s'", linux_governor_file_path);
  }

  if (buffer != NULL) {
    free(buffer);
  }
  free(result);
  return NULL;
}

int cpu_freq_restore_governor(cpu_freq_governor *governor)
{
  char linux_governor_file_path[1024];
  snprintf(linux_governor_file_path, sizeof(linux_governor_file_path),
           LINUX_GOVERNOR_FILE_PATH_FORMAT, governor->which_cpu);

  FILE *linux_governor_file = fopen(linux_governor_file_path, "w");
  if (linux_governor_file == NULL) {
    log_syserror("Cannot open '%s' for writing", linux_governor_file_path);
    if (errno == EACCES) {
      return -2;
    } else {
      return -1;
    }
  }

  fprintf(linux_governor_file, "%s", governor->linux_cpu_governor_name);

  if (utility_file_close(linux_governor_file, linux_governor_file_path) != 0) {
    log_error("Cannot close '%s' after writing", linux_governor_file_path);
    return -1;
  }

  destroy_cpu_freq_governor(governor);
  return 0;
}

void destroy_cpu_freq_governor(cpu_freq_governor *governor)
{
  free(governor->linux_cpu_governor_name);
  free(governor);
}

#define LINUX_FREQUENCIES_FILE_PATH_FORMAT                              \
  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_available_frequencies"

unsigned long long *cpu_freq_available(int which_cpu, ssize_t *list_len)
{
  *list_len = -1;

  /* Set the correct file path */
  char linux_frequencies_file_path[1024];
  snprintf(linux_frequencies_file_path, sizeof(linux_frequencies_file_path),
           LINUX_FREQUENCIES_FILE_PATH_FORMAT, which_cpu);
  /* End of setting the correct file path */

  /* Open the frequency file */
  FILE *linux_frequencies_file
    = utility_file_open_for_reading(linux_frequencies_file_path);
  if (linux_frequencies_file == NULL) {
    log_error("Cannot open '%s' for reading", linux_frequencies_file_path);
    return NULL;
  }
  /* End of opening the frequency file */

  /* Read the available frequencies */
  char *buffer = NULL;
  size_t buffer_len = 0;
  int rc = utility_file_readln(linux_frequencies_file,
                               &buffer, &buffer_len, 256);
  if (rc == -2) {
    log_error("Cannot read '%s'", linux_frequencies_file_path);
    goto error;
  } else if (rc == -1) {
    *list_len = 0;
    goto error;
  } else {
    utility_file_close(linux_frequencies_file, linux_frequencies_file_path);
  }
  /* End of reading the available frequencies */

  /* Count the number of available frequencies */
  size_t frequency_count = 0;
  char *ptr = buffer;
  while (*ptr != '\0') {
    if (isspace(*ptr)) {
      frequency_count++;
    }
    ptr++;
  }
  /* End of counting the number of available frequencies */

  /* Create the result */
  *list_len = frequency_count;
  unsigned long long *result = NULL;
  if (frequency_count != 0) {
    result = malloc(sizeof(unsigned long long) * frequency_count);
    int result_idx = 0;
    char *ptr = buffer;
    char *ptr_start = buffer;
    while (*ptr != '\0') {
      if (isspace(*ptr)) {
        *ptr = '\0';
        result[result_idx++] = strtoull(ptr_start, NULL, 10) * 1000;
        ptr++;
        ptr_start = ptr;
      } else {
        ptr++;
      }
    }
  }
  /* End of creating the result */

  if (buffer != NULL) {
    free(buffer);
  }

  return result;

 error:
  utility_file_close(linux_frequencies_file, linux_frequencies_file_path);
  if (buffer != NULL) {
    free(buffer);
  }
  return NULL;
}

#define LINUX_SET_FREQUENCY_FILE_PATH_FORMAT                    \
  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed"

int cpu_freq_set(int which_cpu, unsigned long long new_freq)
{
  /* Open the necessary files first to ensure caller has enough privilege */
  char linux_governor_file_path[1024];
  snprintf(linux_governor_file_path, sizeof(linux_governor_file_path),
           LINUX_GOVERNOR_FILE_PATH_FORMAT, which_cpu);
  FILE *linux_governor_file = fopen(linux_governor_file_path, "w");
  if (linux_governor_file == NULL) {
    if (errno == EACCES) {
      return -2;
    }
    log_syserror("Cannot open '%s' for writing", linux_governor_file_path);
    return -1;
  }
  char linux_set_frequency_file_path[1024];
  snprintf(linux_set_frequency_file_path, sizeof(linux_set_frequency_file_path),
           LINUX_SET_FREQUENCY_FILE_PATH_FORMAT, which_cpu);
  FILE *linux_set_frequency_file = fopen(linux_set_frequency_file_path, "w");
  if (linux_set_frequency_file == NULL) {
    if (errno == EACCES) {
      return -2;
    }
    log_syserror("Cannot open '%s' for writing", linux_set_frequency_file_path);
    return -1;
  }
  /* End of opening necessary files */

  /* Select the right governor first */
  fprintf(linux_governor_file, "userspace");
  utility_file_close(linux_governor_file, linux_governor_file_path);
  /* End of selecting the right governor */

  /* Set the frequency */
  fprintf(linux_set_frequency_file, "%llu", new_freq / 1000);
  utility_file_close(linux_set_frequency_file, linux_set_frequency_file_path);
  /* End of setting the frequency*/

  return 0;
}

#define LINUX_CUR_FREQUENCY_FILE_PATH_FORMAT                    \
  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq"

unsigned long long cpu_freq_get(int which_cpu)
{
  char linux_cur_frequency_file_path[1024];
  snprintf(linux_cur_frequency_file_path, sizeof(linux_cur_frequency_file_path),
           LINUX_CUR_FREQUENCY_FILE_PATH_FORMAT, which_cpu);
  FILE *linux_cur_frequency_file
    = utility_file_open_for_reading(linux_cur_frequency_file_path);
  if (linux_cur_frequency_file == NULL) {
    log_syserror("Cannot open '%s' for reading", linux_cur_frequency_file_path);
    return 0;
  }

  char *buffer = NULL;
  size_t buffer_len = 0;
  int rc = utility_file_readln(linux_cur_frequency_file,
                               &buffer, &buffer_len, 32);
  if (rc == -1) {
    log_error("'%s' is empty", linux_cur_frequency_file_path);
    goto error;
  } else if (rc == -2) {
    log_error("Fail to read '%s'", linux_cur_frequency_file_path);
    goto error;
  }

  utility_file_close(linux_cur_frequency_file, linux_cur_frequency_file_path);

  unsigned long long result = strtoull(buffer, NULL, 10) * 1000;
  free(buffer);
  return result;

 error:
  if (buffer != NULL) {
    free(buffer);
  }
  return 0;
}

int cpu_busyloop_id(const cpu_busyloop *obj)
{
  return obj->which_cpu;
}

unsigned long long cpu_busyloop_frequency(const cpu_busyloop *obj)
{
  return obj->frequency;
}

utility_time *cpu_busyloop_duration(const cpu_busyloop *obj)
{
  return utility_time_to_utility_time_dyn(&obj->duration);
}

void destroy_cpu_busyloop(cpu_busyloop *arg)
{
  utility_time_gc(&arg->duration);
  free(arg);
}

#define BILLION 1000000000.0
static double timespec_to_sec(const struct timespec *t)
{
  return t->tv_sec + t->tv_nsec / BILLION;
}
static unsigned long long duration_to_loop_count(unsigned long long frequency,
                                                 double duration)
{
  return (unsigned long long) (frequency * duration);
}

/* This function is sensitive to timing. All of its data should be in
   the data cache, all of its instructions should be in the
   instruction cache, and the instructions should be with the most
   minimal number of branching possible. */
static __attribute__((optimize(0)))
double busyloop_measurement(unsigned long long loop_count)
{
  int rc = 0;
  struct timespec t_begin, t_end;

  /* The use of CLOCK_THREAD_CPUTIME_ID is justified because this
     measurement is expected to be done in one thread without any
     preemption. */
#define CLOCK_TYPE CLOCK_THREAD_CPUTIME_ID

  /* Calculate actual duration adjustment */
  rc += clock_gettime(CLOCK_TYPE, &t_begin);
  /*    ^    *
   *    |    *
   *  delta  * This is used to stabilize the cache first.
   *    |    *
   *    V    */
  rc += clock_gettime(CLOCK_TYPE, &t_end);
  rc += clock_gettime(CLOCK_TYPE, &t_begin);
  /*    ^    *
   *    |    *
   *  delta  * This is the one used for adjustment.
   *    |    *
   *    V    */
  rc += clock_gettime(CLOCK_TYPE, &t_end);
  double adjustment = timespec_to_sec(&t_end) - timespec_to_sec(&t_begin);
  /* End of calculating actual duration adjustment */

  rc += clock_gettime(CLOCK_TYPE, &t_begin);
  busyloop(loop_count);
  rc += clock_gettime(CLOCK_TYPE, &t_end);
  double actual_duration = timespec_to_sec(&t_end) - timespec_to_sec(&t_begin);

  return (rc == 0 ? (actual_duration - adjustment) : -1.0);

#undef CLOCK_TYPE
}

static int busyloop_search(double duration, double search_tolerance,
                           unsigned search_max_passes,
                           unsigned long long curr_freq,
                           unsigned long long *loop_count)
{
  int nth_pass;
  for (nth_pass = 1; nth_pass <= search_max_passes; nth_pass++) {

    double actual_duration = busyloop_measurement(*loop_count);
    if (actual_duration < 0.0) {
      log_error("Not getting t_begin or t_end when measuring actual duration");
      return -3;
    }

    log_verbose("Pass %d of %d: %llu loops -> %.9f s [%.9f s, %.9f s]\n",
                nth_pass, search_max_passes, *loop_count, actual_duration,
                duration - search_tolerance, duration + search_tolerance);

    if (nth_pass == search_max_passes) {
      /* Do not modify loop_count */
      break;
    }

    double delta = actual_duration - duration;
    if (delta < 0) {
      if (-search_tolerance <= delta) {
        break;
      }

      unsigned long long loop_count_delta = duration_to_loop_count(curr_freq,
                                                                   -delta);
      if (loop_count_delta > (*loop_count / 10)) {
        log_error("busyloop_measurement deviates too much;"
                  " it is 90%% faster than the predicted duration"
                  " (try to use cpu_set_freq()"
                  " or to do clean compilation of the codebase)");
        return -3;
      }
      *loop_count += loop_count_delta;
    } else {
      if (delta <= search_tolerance) {
        break;
      }

      unsigned long long loop_count_delta = duration_to_loop_count(curr_freq,
                                                                   delta);
      if (*loop_count < loop_count_delta) {
        return -2;
      }
      *loop_count -= loop_count_delta;
    }
  }

  return 0;
}

struct busyloop_search_parameters
{
  unsigned long long loop_count;
  unsigned long long curr_freq;
  double duration;
  double search_tolerance;
  unsigned search_max_passes;
  int which_cpu;
  int exit_status;
};
static void *busyloop_search_thread(void *args)
{
  struct busyloop_search_parameters *params = args;
  params->exit_status = -3;

  /* Lock to the CPU to be measured */
  if (lock_me_to_cpu(params->which_cpu) != 0) {
    log_error("Cannot lock myself to CPU #%d", params->which_cpu);
    goto out;
  }

  /* Use RT scheduler without preemption with the maximum priority possible */
  switch (sched_fifo_enter_max(NULL)) {
  case 0:
    break;
  case -1:
    params->exit_status = -1;
    goto out;
  default: /* Anticipate further addition of exit status */
    log_error("Cannot change scheduler to SCHED_FIFO with max priority");
    goto out;
  }

  /* Run the search algorithm */
  params->exit_status = busyloop_search(params->duration,
                                        params->search_tolerance,
                                        params->search_max_passes,
                                        params->curr_freq,
                                        &params->loop_count);

 out:
  return &params->exit_status;
}

int create_cpu_busyloop(int which_cpu, const utility_time *duration,
                        const utility_time *search_tolerance,
                        unsigned search_max_passes,
                        cpu_busyloop **result)
{
  unsigned long long curr_freq = cpu_freq_get(which_cpu);

  struct timespec timespec_duration;
  to_timespec(duration, &timespec_duration);

  unsigned long long loop_count
    = duration_to_loop_count(curr_freq, timespec_to_sec(&timespec_duration));

  /* Refine loop_count if requested */
  if (search_max_passes > 0) {
    struct timespec timespec_search_tolerance;
    to_timespec(search_tolerance, &timespec_search_tolerance);

    pthread_t busyloop_search_tid;
    struct busyloop_search_parameters busyloop_search_params = {
      .which_cpu = which_cpu,
      .duration = timespec_to_sec(&timespec_duration),
      .search_tolerance = timespec_to_sec(&timespec_search_tolerance),
      .search_max_passes = search_max_passes,
      .loop_count = loop_count,
      .curr_freq = curr_freq,
      .exit_status = -3,
    };
    if (pthread_create(&busyloop_search_tid, NULL, busyloop_search_thread,
                       &busyloop_search_params) != 0) {
      log_syserror("Cannot create busyloop search thread");
      *result = NULL;
      return -3;
    }
    if (pthread_join(busyloop_search_tid, NULL) != 0) {
      log_syserror("Cannot join busyloop search thread");
      *result = NULL;
      return -3;
    }
    if (busyloop_search_params.exit_status != 0) {
      *result = NULL;
      return busyloop_search_params.exit_status;
    }
    loop_count = busyloop_search_params.loop_count;
  }
  /* End of refining loop_count */

  /* Create the result */
  cpu_busyloop *obj = malloc(sizeof(cpu_busyloop));
  if (obj == NULL) {
    log_error("Insufficient memory to create cpu_busyloop object");
    *result = NULL;
    return -3;
  }

  obj->which_cpu = which_cpu;
  obj->frequency = curr_freq;
  obj->loop_count = loop_count;
  utility_time_init(&obj->duration);
  utility_time_to_utility_time_gc(duration, &obj->duration);
  /* End of creating the result */

  utility_time_gc_auto(search_tolerance);

  *result = obj;
  return 0;
}

int enter_UP_mode_freq_max(cpu_freq_governor **default_gov)
{
  if (enter_UP_mode() == -1) {
    log_error("Cannot enter UP mode");
    *default_gov = NULL;
    return -2;
  }

  unsigned long long *freqs;
  ssize_t freqs_len;
  freqs = cpu_freq_available(0, &freqs_len);
  if (freqs_len <= 0) {
    log_error("CPU 0 has no available frequency for selection");
    *default_gov = NULL;
    return -2;
  }
  unsigned long long max_freq = freqs[0];
  free(freqs);

  *default_gov = cpu_freq_get_governor(0);
  if (*default_gov == NULL) {
    log_error("Cannot obtain the current governor of CPU 0");
    *default_gov = NULL;
    return -2;
  }

  int rc = -2;
  switch (cpu_freq_set(0, max_freq)) {
  case -2:
    rc = -1;
    /* No break since this must jump to label error */
  case -1:
    goto error;
  }
  
  return 0;

 error:
  if (rc != -1 && cpu_freq_restore_governor(*default_gov) != 0) {
    log_error("You have to restore the governor of CPU 0 yourself");
    *default_gov = NULL;
    return -2;
  }
  *default_gov = NULL;
  return rc;
}
