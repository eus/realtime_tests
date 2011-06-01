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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "../utility_experimentation.h"
#include "../utility_log.h"
#include "../utility_sched_fifo.h"
#include "../utility_cpu.h"
#include "../utility_time.h"

struct thread_params
{
  int which_cpu;
  pthread_barrier_t *green_light;
  struct timespec t_begin;
  struct timespec t_end;
  cpu_busyloop *busyloop;
  int rc;
};
static void *thread_fn(void *args)
{
  struct thread_params *prms = args;
  prms->rc = EXIT_FAILURE;

  if (lock_me_to_cpu(prms->which_cpu) != 0) {
    log_error("Cannot set affinity to CPU%d", prms->which_cpu);
    goto error;
  }

  int rc = sched_fifo_enter_max(NULL);
  if (rc != 0) {
    log_error("Cannot enter SCHED_FIFO with max prio%s",
              rc == -1 ? " (insufficient privilege)" : "");
    goto error;
  }

  pthread_barrier_wait(prms->green_light);

  rc += clock_gettime(CLOCK_MONOTONIC, &prms->t_begin);
  keep_cpu_busy(prms->busyloop);
  rc += clock_gettime(CLOCK_MONOTONIC, &prms->t_end);  

  prms->rc = EXIT_SUCCESS;

 error:
  return &prms->rc;
}

MAIN_BEGIN("multicore_means_parallel_threads", "stderr", NULL)
{
  /* Tunable parameters */
  relative_time tolerance;
  utility_time_init(&tolerance);
  to_utility_time(1, ms, &tolerance);

  relative_time busyloop_duration;
  utility_time_init(&busyloop_duration);
  to_utility_time(1500, ms, &busyloop_duration);

  relative_time busyloop_tolerance;
  utility_time_init(&busyloop_tolerance);
  to_utility_time(100, us, &busyloop_tolerance);

  int busyloop_passes = 10;
  /* End of tunable parameters */

  int rc = 0;
  int exit_code = EXIT_FAILURE;

  pthread_barrier_t green_light;
  int green_light_initialized = 0;

  cpu_freq_governor *gov_cpu0 = NULL;
  cpu_freq_governor *gov_cpu1 = NULL;

  cpu_busyloop *busyloop_cpu0 = NULL;
  cpu_busyloop *busyloop_cpu1 = NULL;


  if (get_last_cpu() < 1) {
    log_error("This experimentation unit needs at least two CPUs");
    goto error;
  }


  if ((gov_cpu0 = cpu_freq_get_governor(0)) == NULL) {
    log_error("Cannot save the frequency governor of CPU0");
    goto error;
  }
  if ((gov_cpu1 = cpu_freq_get_governor(1)) == NULL) {
    log_error("Cannot save the frequency governor of CPU1");
    goto error;
  }


  if ((rc = cpu_freq_set_max(0)) != 0) {
    log_error("Cannot set CPU0 frequency to max%s",
              rc == -2 ? " (insufficient privilege)" : "");
    goto error;
  }
  if ((rc = cpu_freq_set_max(1)) != 0) {
    log_error("Cannot set CPU1 frequency to max%s",
              rc == -2 ? " (insufficient privilege)" : "");
    goto error;
  }


  sleep(1); /* Wait for the CPU frequencies to stabilize */


  rc = create_cpu_busyloop(0, &busyloop_duration,
                           &busyloop_tolerance, busyloop_passes,
                           &busyloop_cpu0);
  if (rc != 0) {
    log_error("Cannot create busyloop on CPU0%s",
              rc == -1 ? " (insufficient privilege)" :
              rc == -2 ? " (duration too short)" :
              rc == -4 ? " (duration too long)" : "");
    goto error;
  }
  rc = create_cpu_busyloop(1, &busyloop_duration,
                           &busyloop_tolerance, busyloop_passes,
                           &busyloop_cpu1);
  if (rc != 0) {
    log_error("Cannot create busyloop on CPU1%s",
              rc == -1 ? " (insufficient privilege)" :
              rc == -2 ? " (duration too short)" :
              rc == -4 ? " (duration too long)" : "");
    goto error;
  }


  if ((errno = pthread_barrier_init(&green_light, NULL, 3)) != 0) {
    log_syserror("Cannot initialize barrier green_light");
    goto error;
  }
  green_light_initialized = 1;


  pthread_t thread_0;
  struct thread_params prms0 = {
    .which_cpu = 0,
    .green_light = &green_light,
    .busyloop = busyloop_cpu0,
  };
  pthread_t thread_1;
  struct thread_params prms1 = {
    .which_cpu = 1,
    .green_light = &green_light,
    .busyloop = busyloop_cpu1,
  };
  if ((errno = pthread_create(&thread_0, NULL, thread_fn, &prms0)) != 0) {
    log_syserror("Cannot create test thread for CPU0");
    goto error;
  }
  if ((errno = pthread_create(&thread_1, NULL, thread_fn, &prms1)) != 0) {
    log_syserror("Cannot create test thread for CPU1");
    goto error;
  }


  pthread_barrier_wait(&green_light);


  if ((errno = pthread_join(thread_0, NULL)) != 0) {
    log_syserror("Cannot join test thread of CPU0");
  }
  if ((errno = pthread_join(thread_1, NULL)) != 0) {
    log_syserror("Cannot join test thread of CPU1");
  }
  if (prms0.rc != EXIT_SUCCESS) {
    log_error("Test thread of CPU0 fails");
    goto error;
  }
  if (prms1.rc != EXIT_SUCCESS) {
    log_error("Test thread of CPU1 fails");
    goto error;
  }


  absolute_time *cpu0_begin = timespec_to_utility_time_dyn(&prms0.t_begin);
  absolute_time *cpu0_end = timespec_to_utility_time_dyn(&prms0.t_end);
  relative_time *cpu0_duration = utility_time_sub_dyn(cpu0_end, cpu0_begin);
  {
    char *str_t = to_string_dyn(cpu0_duration);
    log_verbose("CPU0_duration is %s s\n", str_t);
    free(str_t);
  }
  absolute_time *cpu1_begin = timespec_to_utility_time_dyn(&prms1.t_begin);
  absolute_time *cpu1_end = timespec_to_utility_time_dyn(&prms1.t_end);
  relative_time *cpu1_duration = utility_time_sub_dyn(cpu1_end, cpu1_begin);
  {
    char *str_t = to_string_dyn(cpu1_duration);
    log_verbose("CPU1_duration is %s s\n", str_t);
    free(str_t);
  }
  relative_time *delta_begin, *delta_end, *delta_duration;

  if (utility_time_ge(cpu0_begin, cpu1_begin)) {
    delta_begin = utility_time_sub_dyn(cpu0_begin, cpu1_begin);
  } else {
    delta_begin = utility_time_sub_dyn(cpu1_begin, cpu0_begin);
  }
  if (utility_time_ge(cpu0_duration, cpu1_duration)) {
    delta_duration = utility_time_sub_dyn(cpu0_duration, cpu1_duration);
  } else {
    delta_duration = utility_time_sub_dyn(cpu1_duration, cpu0_duration);
  }
  if (utility_time_ge(cpu0_end, cpu1_end)) {
    delta_end = utility_time_sub_dyn(cpu0_end, cpu1_end);
  } else {
    delta_end = utility_time_sub_dyn(cpu1_end, cpu0_end);
  }

  rc = 0;
  if (utility_time_gt(delta_begin, &tolerance)) {
    char *str_t = to_string_dyn(delta_begin);
    log_error("CPU0_begin and CPU1_begin differs by %s s", str_t);
    free(str_t);
    rc--;
  }
  if (utility_time_gt(delta_duration, &tolerance)) {
    char *str_t = to_string_dyn(delta_duration);
    log_error("CPU0_duration and CPU1_duration differs by %s s", str_t);
    free(str_t);
    rc--;
  }
  if (utility_time_gt(delta_end, &tolerance)) {
    char *str_t = to_string_dyn(delta_end);
    log_error("CPU0_end and CPU1_end differs by %s s", str_t);
    free(str_t);
    rc--;
  }

  utility_time_gc(cpu0_begin);
  utility_time_gc(cpu0_end);
  utility_time_gc(cpu0_duration);
  utility_time_gc(cpu1_begin);
  utility_time_gc(cpu1_end);
  utility_time_gc(cpu1_duration);
  utility_time_gc(delta_begin);
  utility_time_gc(delta_end);
  utility_time_gc(delta_duration);

  if (rc != 0)
    goto error;

  exit_code = EXIT_SUCCESS;

  /* Clean-up */
 error:
  if (busyloop_cpu1 != NULL)
    destroy_cpu_busyloop(busyloop_cpu1);
  if (busyloop_cpu0 != NULL)
    destroy_cpu_busyloop(busyloop_cpu0);
  if (gov_cpu1 != NULL)
    if (cpu_freq_restore_governor(gov_cpu1) != 0)
      log_error("You have to restore the frequency governor of CPU1 yourself");
  if (gov_cpu0 != NULL)
    if (cpu_freq_restore_governor(gov_cpu0) != 0)
      log_error("You have to restore the frequency governor of CPU0 yourself");
  if (green_light_initialized)
    if ((errno = pthread_barrier_destroy(&green_light)) != 0)
      log_syserror("Cannot destroy barrier green_light");

  return exit_code;

} MAIN_END
