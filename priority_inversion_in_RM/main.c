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

#define _GNU_SOURCE /* To use PTHREAD_PRIO_NONE and friends */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "../utility_log.h"
#include "../utility_experimentation.h"
#include "../utility_time.h"
#include "../task.h"
#include "../utility_memory.h"

struct tau_prog_prms
{
  pthread_mutex_t *R1;
  cpu_busyloop *busyloop_10;
  cpu_busyloop *busyloop_30;
  cpu_busyloop *busyloop_500;
  int exit_code;
};

static void tau_1_prog(void *args)
{
  struct tau_prog_prms *prms = args;
  prms->exit_code = -1;

  keep_cpu_busy(prms->busyloop_10);

  if ((errno = pthread_mutex_lock(prms->R1)) != 0) {
    log_syserror("Tau_3 cannot lock R1");
    return;
  }

  keep_cpu_busy(prms->busyloop_10);

  if ((errno = pthread_mutex_unlock(prms->R1)) != 0) {
    log_syserror("Tau_3 cannot unlock R1");
    return;
  }

  keep_cpu_busy(prms->busyloop_10);

  prms->exit_code = 0;
}

static void tau_2_prog(void *args)
{
  struct tau_prog_prms *prms = args;
  prms->exit_code = 0;

  keep_cpu_busy(prms->busyloop_500);
}

static void tau_3_prog(void *args)
{
  struct tau_prog_prms *prms = args;
  prms->exit_code = -1;

  keep_cpu_busy(prms->busyloop_10);

  if ((errno = pthread_mutex_lock(prms->R1)) != 0) {
    log_syserror("Tau_3 cannot lock R1");
    return;
  }

  keep_cpu_busy(prms->busyloop_30);

  if ((errno = pthread_mutex_unlock(prms->R1)) != 0) {
    log_syserror("Tau_3 cannot unlock R1");
    return;
  }

  keep_cpu_busy(prms->busyloop_10);

  prms->exit_code = 0;
}

struct one_time_release_prms
{
  char *name;
  struct timespec *t_release;
  int already_fired;
  sigset_t mask;
};

static void one_time_release(void *args)
{
  struct one_time_release_prms *prms = args;

  if (prms->already_fired) {
    int unused;
    sigwait(&prms->mask, &unused);
  } else {
    prms->already_fired = 1;
    if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, prms->t_release, NULL)
        != 0) {
      log_syserror("Task %s cannot wait for the release time", prms->name);
    }
  }
}

struct task_thread_prms {
  task *tau;
  int sched_fifo_prio;
  int rc;
};

static void *task_thread(void *args)
{
  struct task_thread_prms *prms = args;

  prms->rc = sched_fifo_enter(prms->sched_fifo_prio, NULL);
  if (prms->rc != 0) {
    log_error("Task %s cannot become SCHED_FIFO thread",
              task_statistics_name(prms->tau));
    goto out;
  }

  memory_preallocate_stack(1024);

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

MAIN_BEGIN("priority_inversion_in_RM", "stderr", NULL)
{
  switch (memory_lock()) {
  case 0:
    break;
  case -1:
    fatal_error("Cannot lock current and future memory due to memory limit");
  case -2:
    fatal_error("Insufficient privilege to lock current and future memory");
  default:
    fatal_error("Cannot lock current and future memory");
  }

  int mutex_protocol = PTHREAD_PRIO_NONE;

  if (argc != 2) {
    fatal_error("Usage: %s USE_PRIORITY_INHERITANCE\n"
                "Replace USE_PRIORITY_INHERITANCE with 0 to not use PI.\n"
                "Replace it with a non-zero to use PI.", argv[0]);
  }
  if (atoi(argv[1]) != 0) {
    mutex_protocol = PTHREAD_PRIO_INHERIT;
  }

  /* Tuneable parameters */
  utility_time busyloop_tolerance;
  utility_time_init(&busyloop_tolerance);
  to_utility_time(50, us, &busyloop_tolerance);
  unsigned busyloop_passes = 10;

  relative_time *offset_to_start = to_utility_time_dyn(3, s);
  /* END: Tuneable parameters */

  int exit_code = EXIT_FAILURE;
  cpu_busyloop *busyloop_10 = NULL;
  cpu_busyloop *busyloop_30 = NULL;
  cpu_busyloop *busyloop_500 = NULL;
  task *tau_1 = NULL;
  task *tau_2 = NULL;
  task *tau_3 = NULL;
  int R1_initialized = 0;

  /* Determining overheads */
  relative_time *job_stats_overhead = NULL;
  relative_time *task_overhead = NULL;
  relative_time *overhead = NULL;
  char *t_str = NULL;

  if (job_statistics_overhead(0, &job_stats_overhead) != 0) {
    log_error("Cannot obtain job statistics overhead");
    goto error;
  }
  utility_time_set_gc_manual(job_stats_overhead);
  t_str = to_string_dyn(job_stats_overhead);
  printf("job_stats_overhead: %s\n", t_str);
  free(t_str);

  if (finish_to_start_overhead(0, 0, &task_overhead) != 0) {
    log_error("Cannot obtain finish to start overhead");
    goto error;
  }
  utility_time_set_gc_manual(task_overhead);
  t_str = to_string_dyn(task_overhead);
  printf("     task_overhead: %s\n", t_str);
  free(t_str);

  overhead = utility_time_add_dyn_gc(job_stats_overhead, task_overhead);
  utility_time_set_gc_manual(overhead);  
  /* END: Determining overheads */

  /* Create needed busyloop objects */
#define create_busyloop(duration)                                       \
  do {                                                                  \
    relative_time *length = to_utility_time_dyn(duration, ms);          \
    length = utility_time_sub_dyn_gc(length, job_stats_overhead);       \
    length = utility_time_sub_dyn_gc(length, task_overhead);            \
    int rc = create_cpu_busyloop(0, length,                             \
                                 &busyloop_tolerance, busyloop_passes,  \
                                 &busyloop_ ## duration);               \
    if (rc == -2) {                                                     \
      log_error(#duration " ms is too short to create busyloop");       \
      goto error;                                                       \
    } else if (rc == -4) {                                              \
      log_error(#duration " ms too long to create busyloop");           \
      goto error;                                                       \
    } else if (rc != 0) {                                               \
      log_error("Cannot create a " #duration " ms busyloop");           \
      goto error;                                                       \
    }                                                                   \
  } while (0)

  create_busyloop(10);
  create_busyloop(30);
  create_busyloop(500);

#undef create_busyloop  
  /* END: Create needed busyloop objects */

  /* Create the shared resource object */
  pthread_mutex_t R1;
  {
    pthread_mutexattr_t R1_attr;
    pthread_mutexattr_init(&R1_attr);
    if ((errno = pthread_mutexattr_setprotocol(&R1_attr, mutex_protocol))
        != 0) {
        log_syserror("Cannot set R1 protocol to plain mutex");
        goto error;
      }
    pthread_mutex_init(&R1, &R1_attr);
    R1_initialized = 1;
    pthread_mutexattr_destroy(&R1_attr);
  }
  /* END: Create the shared resource object */

  /* Create task parameter objects */
#define create_task_prog_prms(id)                       \
  struct tau_prog_prms tau_ ## id ## _prog_prms = {     \
    .R1 = &R1,                                          \
    .busyloop_10 = busyloop_10,                         \
    .busyloop_30 = busyloop_30,                         \
    .busyloop_500 = busyloop_500,                       \
  }

  create_task_prog_prms(1);
  create_task_prog_prms(2);
  create_task_prog_prms(3);

#undef create_task_prog_prms
  /* END: Create task parameter objects */

  /* Create task WCETs */
#define create_wcet(id, duration)                       \
  relative_time tau_ ## id ## _wcet;                    \
  utility_time_init(&tau_ ## id ## _wcet);              \
  to_utility_time(duration, ms, &tau_ ## id ## _wcet)

  create_wcet(1, 30);
  create_wcet(2, 500);
  create_wcet(3, 50);

#undef create_wcet  
  /* END: Create task WCETs */

  /* Create task deadlines */
#define create_deadline(id, offset)                     \
  relative_time tau_ ## id ## _deadline;                \
  utility_time_init(&tau_ ## id ## _deadline);          \
  to_utility_time(offset, ms, &tau_ ## id ## _deadline)

  create_deadline(1, 50);
  create_deadline(2, 530);
  create_deadline(3, 580);

#undef create_deadline  
  /* END: Create task deadlines */

  /* Calculate the absolute starting & ending time */
  struct timespec t_now;
  if (clock_gettime(CLOCK_MONOTONIC, &t_now) != 0) {
    log_syserror("Cannot get t_now");
    goto error;
  }

#define create_t_release(id, off)                               \
  relative_time *offset_ ## id = to_utility_time_dyn(off, ms);  \
  struct timespec t_release_ ## id;                             \
  do {                                                          \
    relative_time *offset                                       \
      = utility_time_add_dyn_gc(offset_to_start,                \
                                to_utility_time_dyn(off,        \
                                                    ms));       \
    absolute_time *t = timespec_to_utility_time_dyn(&t_now);    \
    to_timespec_gc(utility_time_add_dyn_gc(t,                   \
                                           offset),             \
                   &t_release_ ## id);                          \
  } while (0)

  utility_time_set_gc_manual(offset_to_start);
  create_t_release(1, 20);
  create_t_release(2, 40);
  create_t_release(3, 0);

  struct timespec t_release;
  to_timespec_gc(utility_time_add_dyn_gc(timespec_to_utility_time_dyn(&t_now),
                                         offset_to_start),
                 &t_release);
  utility_time_gc(offset_to_start);

#undef create_t_release

  struct timespec t_stop_rel = {
    .tv_sec = 5,
    .tv_nsec = 0,
  };
  /* END: Calculate the absolute starting & ending time */

  /* Create task objects */
#define create_task(id)                                                 \
  struct one_time_release_prms tau_ ## id ## _release_prms = {          \
    .name = "tau " #id,                                                 \
    .t_release = &t_release_ ## id,                                     \
    .already_fired = 0,                                                 \
  };                                                                    \
  sigfillset(&tau_ ## id ## _release_prms.mask);                        \
  do {                                                                  \
    int rc = task_create("tau " #id, &tau_ ## id ## _wcet,              \
                         to_utility_time_dyn(0, ms),                    \
                         &tau_ ## id ## _deadline,                      \
                         timespec_to_utility_time_dyn(&t_release),      \
                         offset_ ## id,                                 \
                         one_time_release, &tau_ ## id ## _release_prms, \
                         "tau_" #id "_stats.bin", 1, 1,                 \
                         job_stats_overhead, task_overhead,             \
                         tau_ ## id ## _prog, &tau_ ## id ## _prog_prms, \
                         &tau_ ## id);                                  \
    if (rc == -2) {                                                     \
      log_error("Cannot open tau_" #id "_stats.bin");                   \
      goto error;                                                       \
    } else if (rc != 0) {                                               \
      log_error("Cannot create Tau_" #id);                              \
      goto error;                                                       \
    }                                                                   \
  } while (0)

  create_task(1);
  create_task(2);
  create_task(3);

#undef create_task
  /* END: Create task objects */

  /* Designate task SCHED_FIFO priorities */
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

#undef set_prio
  /* END: Designate task SCHED_FIFO priorities */

  /* Be task manager */
  if (sched_fifo_enter_max(NULL) != 0) {
    log_error("Cannot become task manager");
    goto error;
  }
  /* END: Be task manager */

  /* Create task threads */
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

#undef create_task_thread
  /* END: Create task threads */

  /* Wait for stopping time */
  if (clock_nanosleep(CLOCK_MONOTONIC, 0, &t_stop_rel, NULL) != 0) {
    log_syserror("Task manager fails to wait for the stopping time");
    goto error;
  }
  /* END: Wait for stopping time */

  /* Stop the tasks */
  task_stop(tau_1);
  task_stop(tau_2);
  task_stop(tau_3);
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

#undef check_rc
  /* END: Check task return statuses */

  exit_code = EXIT_SUCCESS;

 error:
  if (tau_3 != NULL) {
    task_destroy(tau_3);
  }
  if (tau_2 != NULL) {
    task_destroy(tau_2);
  }
  if (tau_1 != NULL) {
    task_destroy(tau_1);
  }

  if (R1_initialized && (errno = pthread_mutex_destroy(&R1)) != 0) {
    log_syserror("Cannot destroy R1");
  }

  if (busyloop_500 != NULL) {
    destroy_cpu_busyloop(busyloop_500);
  }
  if (busyloop_30 != NULL) {
    destroy_cpu_busyloop(busyloop_30);
  }
  if (busyloop_10 != NULL) {
    destroy_cpu_busyloop(busyloop_10);
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
