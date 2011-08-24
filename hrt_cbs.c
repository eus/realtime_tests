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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include "utility_experimentation.h"
#include "utility_log.h"
#include "utility_cpu.h"
#include "utility_time.h"
#include "utility_sched_deadline.h"
#include "task.h"

struct periodic_task_thread_prms {
  int wcet_ms;
  int deadline_ms;
  int budget_ms;
  int period_ms;
  const char *task_name;
  const char *stats_file_path;
  struct busyloop_exact_args busyloop_exact_args;
  const relative_time *job_stats_overhead;
  const relative_time *task_overhead;
  task *periodic_task;
  int rc;
};
static void *periodic_task_thread(void *args)
{
  struct periodic_task_thread_prms *prms = args;
  prms->rc = EXIT_FAILURE;

  /* Block all signals */
  {
    sigset_t blocked_signals;
    if (sigfillset(&blocked_signals) != 0) {
      log_error("Cannot fill blocked_signals");
      return &prms->rc;
    }
    if ((errno = pthread_sigmask(SIG_BLOCK, &blocked_signals, NULL)) != 0) {
      log_syserror("Cannot block all signals");
      return &prms->rc;
    }
  }
  /* END: Block signals */

  /* Use CBS server */
  {
    int rc = sched_deadline_enter(to_utility_time_dyn(prms->budget_ms, ms),
                                  to_utility_time_dyn(prms->period_ms, ms),
                                  NULL);
    if (rc == -1) {
      log_error("Insufficient privilege to use SCHED_DEADLINE");
      return &prms->rc;
    } else if (rc != 0) {
      log_error("Cannot enter SCHED_DEADLINE");
      return &prms->rc;
    }
  }
  /* END: Use CBS server */

  /* Wait for starting signal */
  {
    int signo;
    sigset_t starting_signal;
    sigemptyset(&starting_signal);
    sigaddset(&starting_signal, SIGUSR1);
    sigwait(&starting_signal, &signo);
  }
  /* END: Wait for starting signal */

  /* Create periodic task */
  {
    struct timespec t_now;
    if (clock_gettime(CLOCK_MONOTONIC, &t_now) != 0) {
      fatal_syserror("Cannot get t_now");
    }
    struct timespec t_release;
    to_timespec_gc(utility_time_add_dyn_gc(timespec_to_utility_time_dyn(&t_now),
                                           to_utility_time_dyn(1, s)),
                   &t_release);

    int rc = task_create(prms->task_name,
                         to_utility_time_dyn(prms->wcet_ms, ms),
                         to_utility_time_dyn(prms->period_ms, ms),
                         to_utility_time_dyn(prms->deadline_ms == -1
                                             ? prms->period_ms
                                             : prms->deadline_ms, ms),
                         timespec_to_utility_time_dyn(&t_release),
                         to_utility_time_dyn(0, ms),
                         NULL, NULL,
                         prms->stats_file_path,
                         prms->job_stats_overhead,
                         prms->task_overhead,
                         0,
                         busyloop_exact, &prms->busyloop_exact_args,
                         &prms->periodic_task);
    if (rc == -2) {
      fatal_error("Cannot open %s for writing", prms->stats_file_path);
    } else if (rc != 0) {
      fatal_error("Cannot create periodic task");
    }
  }  
  /* END: Create periodic task */

  /* Run the task */
  if (task_start(prms->periodic_task) != 0) {
    log_error("Periodic task cannot be completed successfully");
    return &prms->rc;
  }
  /* END: Run the task */

  prms->rc = EXIT_SUCCESS;
  return &prms->rc;
}

MAIN_BEGIN("hrt_cbs", "stderr", NULL)
{
  const char *task_name = NULL;
  const char *stats_file_path = NULL;
  int wcet_ms = -1;
  int budget_ms = -1;
  int deadline_ms = -1;
  int period_ms = -1;
  {
    int optchar;
    opterr = 0;
    while ((optchar = getopt(argc, argv, ":hn:s:c:d:q:t:")) != -1) {
      switch (optchar) {
      case 'n':
        task_name = optarg;
        break;
      case 's':
        stats_file_path = optarg;
        break;
      case 'c':
        wcet_ms = atoi(optarg);
        if (wcet_ms <= 0) {
          fatal_error("WCET must be at least 1 ms (-h for help)");
        }
        break;
      case 't':
        period_ms = atoi(optarg);
        if (period_ms <= 0) {
          fatal_error("PERIOD must be at least 1 ms (-h for help)");
        }
        break;
      case 'q':
        budget_ms = atoi(optarg);
        if (budget_ms <= 0) {
          fatal_error("BUDGET must be at least 1 ms (-h for help)");
        }
        break;
      case 'd':
        deadline_ms = atoi(optarg);
        if (deadline_ms <= 0) {
          fatal_error("DEADLINE must be at least 1 ms (-h for help)");
        }
        break;
      case 'h':
        printf("Usage: %s -n NAME -s STATS_FILE -c WCET -q BUDGET -t PERIOD\n"
               "       [-d DEADLINE]"
               "\n"
               "A HRT CBS is a CBS that never postpones its deadline because\n"
               "it serves a periodic task that obeys the stated WCET and\n"
               "period.\n"
               "SIGTERM should be used to graciously terminate this program.\n"
               "Signal SIGUSR1 must be sent to this program to start the\n"
               "periodic task.\n"
               "\n"
               "-n NAME is the name of the periodic task.\n"
               "-s STATS_FILE is the file to record the task statistics.\n"
               "-c WCET is the worst-case execution time in millisecond.\n"
               "-d DEADLINE is the relative deadline of the task, not the\n"
               "   CBS, in millisecond. If this is omitted, the relative\n"
               "   deadline of the task is equal to the CBS period.\n"
               "-q BUDGET is the CBS budget in millisecond.\n"
               "-t PERIOD is the CBS period in millisecond.\n",
               prog_name);
        return EXIT_SUCCESS;
      case '?':
        fatal_error("Unrecognized option character -%c", optopt);
      case ':':
        fatal_error("Option -%c needs an argument", optopt);
      default:
        fatal_error("Unexpected return value of fn getopt");
      }
    }
  }
  if (task_name == NULL) {
    fatal_error("-n must be specified (-h for help)");
  }
  if (stats_file_path == NULL) {
    fatal_error("-s must be specified (-h for help)");
  }
  if (wcet_ms == -1) {
    fatal_error("-c must be specified (-h for help)");
  }
  if (budget_ms == -1) {
    fatal_error("-q must be specified (-h for help)");
  }
  if (period_ms == -1) {
    fatal_error("-t must be specified (-h for help)");
  }
  if (deadline_ms == -1) {
    if (wcet_ms > budget_ms) {
      fatal_error("WCET must be less than or equal to the budget"
                  " (-h for help)");
    }
  } else {
    if (wcet_ms > deadline_ms) {
      fatal_error("WCET must be less than or equal to the deadline"
                  " (-h for help)");
    }
    if (deadline_ms > budget_ms) {
      fatal_error("Deadline must be less than or equal to the budget"
                  " (-h for help)");
    }
  }
  if (budget_ms > period_ms) {
    fatal_error("Budget must be less than or equal to the period"
                " (-h for help)");
  }

  /* Measure overhead */
  relative_time *job_stats_overhead;
  relative_time *task_overhead;
  relative_time *overhead = to_utility_time_dyn(0, ms);
  utility_time_set_gc_manual(overhead);
  {
    char t_str[32];

    /* Job statistics overhead */
    if (job_statistics_overhead(0, &job_stats_overhead) != 0) {
      fatal_error("Cannot obtain job statistics overhead");
    }
    utility_time_set_gc_manual(job_stats_overhead);
    to_string(job_stats_overhead, t_str, sizeof(t_str));
    printf("job_stats_overhead: %s\n", t_str);    
    /* END: Job statistics overhead */    

    /* Task overhead */
    if (finish_to_start_overhead(0, 0, &task_overhead) != 0) {
      fatal_error("Cannot obtain finish to start overhead");
    }
    utility_time_set_gc_manual(task_overhead);
    to_string(task_overhead, t_str, sizeof(t_str));
    printf("     task_overhead: %s\n", t_str);    
    /* END: Task overhead */

    /* Sum all overheads up */
    utility_time_inc_gc(overhead, job_stats_overhead);
    utility_time_inc_gc(overhead, task_overhead);
    /* END: Sum all overheads up */
  }
  /* END: Measure overhead */

  /* Prepare busyloops */
  cpu_busyloop *wcet_busyloop = NULL;
  {
    int search_tolerance_us = 100;
    int search_passes = 10;
    relative_time *real_wcet
      = utility_time_sub_dyn_gc(to_utility_time_dyn(wcet_ms, ms), overhead);
    int rc = create_cpu_busyloop(0,
                                 real_wcet,
                                 to_utility_time_dyn(search_tolerance_us, us),
                                 search_passes,
                                 &wcet_busyloop);
    if (rc == -2) {
      fatal_error("%d is too small for WCET busyloop", wcet_ms);
    } else if (rc == -4) {
      fatal_error("%d is too big for WCET busyloop", wcet_ms);
    } else if (rc != 0) {
      fatal_error("Cannot create WCET busyloop");
    }
  }
  /* END: Prepare busyloops */

  /* Launching periodic task */
  sigset_t waited_signal;
  if (sigemptyset(&waited_signal) != 0) {
    log_syserror("Cannot empty waited_signal");
  }
  if (sigaddset(&waited_signal, SIGTERM) != 0) {
    log_syserror("Cannot add SIGTERM to waited_signal");
  }
  if (sigaddset(&waited_signal, SIGUSR1) != 0) {
    log_syserror("Cannot add SIGUSR1 to waited_signal");
  }
  pthread_sigmask(SIG_BLOCK, &waited_signal, NULL);

  pthread_t periodic_task_tid;
  struct periodic_task_thread_prms periodic_task_thread_args = {
    .task_name = task_name,
    .wcet_ms = wcet_ms,
    .deadline_ms = deadline_ms,
    .budget_ms = budget_ms,
    .period_ms = period_ms,
    .busyloop_exact_args = {
      .busyloop_obj = wcet_busyloop,
    },
    .stats_file_path = stats_file_path,
    .job_stats_overhead = job_stats_overhead,
    .task_overhead = task_overhead,
    .periodic_task = NULL,
  };
  if ((errno = pthread_create(&periodic_task_tid, NULL, periodic_task_thread,
                              &periodic_task_thread_args)) != 0) {
    fatal_syserror("Cannot create periodic task thread");
  }

  int rcvd_signo;
  if (sigdelset(&waited_signal, SIGUSR1) != 0) {
    log_syserror("Cannot delete SIGUSR1 from waited_signal");
  }
  sigwait(&waited_signal, &rcvd_signo);
  pthread_sigmask(SIG_UNBLOCK, &waited_signal, NULL);

  task_stop(periodic_task_thread_args.periodic_task);

  if ((errno = pthread_join(periodic_task_tid, NULL)) != 0) {
    fatal_syserror("Cannot join periodic task thread");
  }

  if (periodic_task_thread_args.rc != EXIT_SUCCESS) {
    fatal_syserror("Cannot execute periodic task thread successfully");
  }
  /* END: Launching periodic task */

  /* Clean-up */
  task_destroy(periodic_task_thread_args.periodic_task);
  destroy_cpu_busyloop(wcet_busyloop);
  utility_time_gc(job_stats_overhead);
  utility_time_gc(task_overhead);
  utility_time_gc(overhead);
  /* END: Clean-up */

  return EXIT_SUCCESS;

} MAIN_END
