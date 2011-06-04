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
#include <pthread.h>
#include <time.h>
#include "../utility_experimentation.h"
#include "../task.h"
#include "../utility_log.h"
#include "../utility_time.h"
#include "../utility_sched_fifo.h"

MAIN_BEGIN("rate_monotonic", "stderr", NULL)
{
  /* Tuneable parameters */
  relative_time *tau_1_wcet = to_utility_time_dyn(2, ms);
  relative_time *tau_1_period = to_utility_time_dyn(10, ms);

  relative_time *tau_2_wcet = to_utility_time_dyn(3, ms);
  relative_time *tau_2_period = to_utility_time_dyn(15, ms);

  relative_time *tau_3_wcet = to_utility_time_dyn(6, ms);
  relative_time *tau_3_period = to_utility_time_dyn(26, ms);

  relative_time *tau_4_wcet = to_utility_time_dyn(6, ms);
  relative_time *tau_4_period = to_utility_time_dyn(36, ms);

  relative_time *busyloop_tolerance = to_utility_time_dyn(100, us);
  unsigned busyloop_search_passes = 10;

  relative_time *offset_to_start = to_utility_time_dyn(3, s);
  relative_time *offset_to_stop = to_utility_time_dyn(13, s);
  /* END: Tuneable parameters */

  int exit_code = EXIT_FAILURE;
  int rc = 0;
  relative_time *job_stats_overhead = NULL;
  relative_time *task_overhead = NULL;
  relative_time *overhead = NULL;
  cpu_busyloop *tau_1_busyloop = NULL;
  cpu_busyloop *tau_2_busyloop = NULL;
  cpu_busyloop *tau_3_busyloop = NULL;
  cpu_busyloop *tau_4_busyloop = NULL;
  task *tau_1 = NULL;
  task *tau_2 = NULL;
  task *tau_3 = NULL;
  task *tau_4 = NULL;
  char t_str[32];

  /* Determining overheads */
  if (job_statistics_overhead(0, &job_stats_overhead) != 0) {
    log_error("Cannot obtain job statistics overhead");
    goto error;
  }
  utility_time_set_gc_manual(job_stats_overhead);
  to_string(job_stats_overhead, t_str, sizeof(t_str));
  printf("job_stats_overhead: %s\n", t_str);

  if (finish_to_start_overhead(0, 0, &task_overhead) != 0) {
    log_error("Cannot obtain finish to start overhead");
    goto error;
  }
  utility_time_set_gc_manual(task_overhead);
  to_string(task_overhead, t_str, sizeof(t_str));
  printf("     task_overhead: %s\n", t_str);

  overhead = utility_time_add_dyn_gc(job_stats_overhead, task_overhead);
  utility_time_set_gc_manual(overhead);  
  /* END: Determining overheads */

  /* Check that each WCET is not less than the overhead */
#define check_wcet(id) do {                                     \
    if (utility_time_lt(tau_ ## id ## _wcet, overhead)) {       \
      log_error("Tau_" #id " WCET is too small");               \
      goto error;                                               \
    }                                                           \
  } while (0)

  check_wcet(1);
  check_wcet(2);
  check_wcet(3);
  check_wcet(4);

#undef check_wcet
  /* END: Check that each WCET is not less than the overhead */

  /* Create needed busyloops */
#define create_busyloop(id) do {                                        \
    tau_ ## id ## _wcet = utility_time_sub_dyn_gc(tau_ ## id ## _wcet,  \
                                                  overhead);            \
    utility_time_set_gc_manual(tau_ ## id ## _wcet);                    \
    rc = create_cpu_busyloop(0, tau_ ## id ## _wcet,                    \
                             busyloop_tolerance,                        \
                             busyloop_search_passes,                    \
                             &tau_ ## id ## _busyloop);                 \
    utility_time_set_gc_auto(tau_ ## id ## _wcet);                      \
    if (rc == -2) {                                                     \
      log_error("Tau_" #id " WCET is too small to create busyloop");    \
      goto error;                                                       \
    } else if (rc == -4) {                                              \
      log_error("Tau_" #id " WCET is too big to create busyloop");      \
      goto error;                                                       \
    } else if (rc != 0) {                                               \
      log_error("Cannot create busyloop for Tau_" #id);                 \
      goto error;                                                       \
    }                                                                   \
  } while (0)

  utility_time_set_gc_manual(busyloop_tolerance);
  create_busyloop(1);
  create_busyloop(2);
  create_busyloop(3);
  create_busyloop(4);
  utility_time_gc(busyloop_tolerance);

#undef create_busyloop
  /* END: Create needed busyloops */

  /* Calculate the absolute starting & ending time */
  struct timespec t_now;
  if (clock_gettime(CLOCK_MONOTONIC, &t_now) != 0) {
    log_syserror("Cannot get t_now");
    goto error;
  }

  struct timespec t_release;
  to_timespec_gc(utility_time_add_dyn_gc(timespec_to_utility_time_dyn(&t_now),
                                         offset_to_start),
                 &t_release);

  struct timespec t_stop;
  to_timespec_gc(utility_time_add_dyn_gc(timespec_to_utility_time_dyn(&t_now),
                                         offset_to_stop),
                 &t_stop);
  /* END: Calculate the absolute starting & ending time */

  /* Create the tasks */
#define create_task(id)                                                 \
  struct busyloop_exact_args tau_ ## id ## _busyloop_args = {           \
    .busyloop_obj = tau_ ## id ## _busyloop,                            \
  };                                                                    \
  utility_time_set_gc_manual(tau_ ## id ## _period);                    \
  rc = task_create("tau " #id, tau_ ## id ## _wcet,                     \
                   tau_ ## id ## _period,                               \
                   tau_ ## id ## _period,                               \
                   timespec_to_utility_time_dyn(&t_release),            \
                   to_utility_time_dyn(0, ms),                          \
                   NULL, NULL,                                          \
                   "tau_" #id "_stats.bin", job_stats_overhead,         \
                   task_overhead,                                       \
                   0, busyloop_exact, &tau_ ## id ## _busyloop_args,    \
                   &tau_ ## id);                                        \
  if (rc == -2) {                                                       \
    log_error("Cannot open tau_" #id "_stats.bin");                     \
    goto error;                                                         \
  } else if (rc != 0) {                                                 \
    log_error("Cannot create Tau_" #id);                                \
    goto error;                                                         \
  }                                                                     \
  utility_time_gc(tau_ ## id ## _period)

  create_task(1);
  create_task(2);
  create_task(3);
  create_task(4);

#undef create_task
  /* END: Create the tasks */

  /* Be task manager */
  if (sched_fifo_enter_max(NULL) != 0) {
    log_error("Cannot become task manager");
    goto error;
  }
  /* END: Be task manager */

  /* Designate task SCHED_FIFO priorities */
  struct task_thread_prms {
    task *tau;
    int sched_fifo_prio;
    int rc;
  };

#define set_prio(id)                                                    \
  struct task_thread_prms tau_ ## id ## _thread_args = {                \
    .tau = tau_ ## id,                                                  \
  };                                                                    \
  do {                                                                  \
    if (sched_fifo_prio(id,                                             \
                        &tau_ ## id ## _thread_args.sched_fifo_prio)    \
        != 0) {                                                         \
      log_error("Cannot set SCHED_FIFO priority of Tau_" #id);          \
      goto error;                                                       \
    }                                                                   \
  } while (0)

  set_prio(1);
  set_prio(2);
  set_prio(3);
  set_prio(4);

#undef set_prio
  /* END: Designate task SCHED_FIFO priorities */

  /* Create task threads */
  void *task_thread(void *args)
  {
    struct task_thread_prms *prms = args;

    prms->rc = sched_fifo_enter(prms->sched_fifo_prio, NULL);
    if (prms->rc != 0) {
      log_error("Task %s cannot become SCHED_FIFO thread",
                task_statistics_name(prms->tau));
      goto out;
    }

    if (task_start(prms->tau) != 0) {
      log_error("Task %s does not complete successfully",
                task_statistics_name(prms->tau));
      prms->rc = -2;
      goto out;
    }

    prms->rc = 0;

  out:
    return &prms->rc;
  }

#define create_task_thread(id)                                          \
  pthread_t tau_ ## id ## _thread;                                      \
  do {                                                                  \
    if ((errno = pthread_create(&tau_ ## id ## _thread, NULL,           \
                                task_thread,                            \
                                &tau_ ## id ## _thread_args)) != 0) {   \
      log_syserror("Cannot create task thread of Tau_" #id);            \
      goto error;                                                       \
    }                                                                   \
  } while (0)

  create_task_thread(1);
  create_task_thread(2);
  create_task_thread(3);
  create_task_thread(4);

#undef create_task_thread
  /* END: Create task threads */

  /* Wait for stopping time */
  if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t_stop, NULL) != 0) {
    log_syserror("Task manager fails to wait for the stopping time");
    goto error;
  }
  /* END: Wait for stopping time */

  /* Stop the tasks */
  task_stop(tau_1);
  task_stop(tau_2);
  task_stop(tau_3);
  task_stop(tau_4);
  /* END: Stop the tasks */

  /* Join task threads */
#define join_thread(id) do {                                            \
    if ((errno = pthread_join(tau_ ## id ## _thread, NULL)) != 0) {     \
      log_syserror("Cannot join Tau_" #id " thread");                   \
      goto error;                                                       \
    }                                                                   \
  } while (0)

  join_thread(1);
  join_thread(2);
  join_thread(3);
  join_thread(4);

#undef join_thread
  /* END: Join task threads */

  /* Check task return statuses */
#define check_rc(id) do {                                               \
    if (tau_ ## id ## _thread_args.rc != 0) {                           \
      log_error("Task Tau_" #id " does not return successfully"         \
                " (rc = %d)",                                           \
                tau_ ## id ## _thread_args.rc);                         \
      goto error;                                                       \
    }                                                                   \
  } while (0)

  check_rc(1);
  check_rc(2);
  check_rc(3);
  check_rc(4);

#undef check_rc
  /* END: Check task return statuses */

  exit_code = EXIT_SUCCESS;

 error:
  /* Clean-up */
  if (tau_4 != NULL) {
    task_destroy(tau_4);
  }
  if (tau_3 != NULL) {
    task_destroy(tau_3);
  }
  if (tau_2 != NULL) {
    task_destroy(tau_2);
  }
  if (tau_1 != NULL) {
    task_destroy(tau_1);
  }

  if (tau_4_busyloop != NULL) {
    destroy_cpu_busyloop(tau_4_busyloop);
  }
  if (tau_3_busyloop != NULL) {
    destroy_cpu_busyloop(tau_3_busyloop);
  }
  if (tau_2_busyloop != NULL) {
    destroy_cpu_busyloop(tau_2_busyloop);
  }
  if (tau_1_busyloop != NULL) {
    destroy_cpu_busyloop(tau_1_busyloop);
  }

  if (overhead != NULL) {
    utility_time_gc(overhead);
  }
  if (task_overhead != NULL) {
    utility_time_gc(task_overhead);
  }
  if (job_stats_overhead != NULL) {
    utility_time_gc(job_stats_overhead);
  }

  return exit_code;

} MAIN_END
