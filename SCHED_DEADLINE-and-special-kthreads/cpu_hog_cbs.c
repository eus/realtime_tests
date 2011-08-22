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

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <setjmp.h>
#include "../utility_experimentation.h"
#include "../utility_log.h"
#include "../utility_sched_deadline.h"
#include "../utility_time.h"
#include "../utility_cpu.h"

static inline void print_stats(const absolute_time *t1_abs, int chunk_counter)
{
  struct timespec t2;
  absolute_time t2_abs;
  char t_str[32];

  clock_gettime(CLOCK_MONOTONIC, &t2);
  utility_time_init(&t2_abs);
  timespec_to_utility_time(&t2, &t2_abs);
  utility_time_sub(&t2_abs, t1_abs, &t2_abs);
  to_string(&t2_abs, t_str, sizeof(t_str));

  printf("%d\n%s\n", chunk_counter, t_str);
}

static int chunk_counter = 0;
static absolute_time t1_abs;
static sigjmp_buf jmp_env;
static void sighand(int signo)
{
  if (signo == SIGTERM) {
    print_stats(&t1_abs, chunk_counter);
    siglongjmp(jmp_env, 0);
  }
}

/* When compiled with
 * "cc -O3 -Wall  -pthread   -c -o cpu_hog_cbs.o cpu_hog_cbs.c",
 * without __attribute__((optimize(0))) whose assembly listing is
 * "cpu_hog_cbs_optimized.s",
 * "sudo ./cpu_hog_cbs -e 20 -b 10 -q 5 -t 30 -s 3000" gives only:
 * 3
 * 4.015135706
 *
 * That is incorrect. With __attribute__((optimize(0))) whose assembly
 * listing is "cpu_hog_cbs_non-optimized.s", the same
 * command gives the following correct output:
 * 100
 * 3.008222320
 */
static __attribute__((optimize(0))) void
busyloop_sleep_cycle_with_break(const cpu_busyloop *exec_busyloop,
                                int *chunk_counter,
                                const struct timespec *break_time)
{
  keep_cpu_busy(exec_busyloop);
  clock_nanosleep(CLOCK_MONOTONIC, 0, break_time, NULL);
  *chunk_counter += 1;
}

static __attribute__((optimize(0))) void
busyloop_sleep_cycle_no_break(const cpu_busyloop *exec_busyloop,
                              int *chunk_counter)
{
  keep_cpu_busy(exec_busyloop);
  *chunk_counter += 1;
}

static inline int break_at_stopping_time(const relative_time *max_duration,
                                         const absolute_time *t1_abs,
                                         absolute_time *t2_abs)
{
  struct timespec t2;

  clock_gettime(CLOCK_MONOTONIC, &t2);
  timespec_to_utility_time(&t2, t2_abs);
  utility_time_sub(t2_abs, t1_abs, t2_abs);

  return utility_time_ge(t2_abs, max_duration);
}

static void hog_cpu(const struct timespec *break_time,
                    const relative_time *max_duration,
                    const cpu_busyloop *exec_busyloop,
                    absolute_time *t1_abs,
                    int *chunk_counter)
{
  struct timespec t1;
  absolute_time t2_abs;

  utility_time_init(t1_abs);
  utility_time_init(&t2_abs);

#define CYCLE                                   \
  clock_gettime(CLOCK_MONOTONIC, &t1);          \
  timespec_to_utility_time(&t1, t1_abs);       \
  while (1)

  if (break_time != NULL) {
    if (max_duration == NULL) {
      CYCLE {
        busyloop_sleep_cycle_with_break(exec_busyloop, chunk_counter,
                                        break_time);
      }
    } else {
      CYCLE {
        busyloop_sleep_cycle_with_break(exec_busyloop, chunk_counter,
                                        break_time);

        if (break_at_stopping_time(max_duration, t1_abs, &t2_abs)) {
          break;
        }
      }
    }
  } else {
    if (max_duration == NULL) {
      CYCLE {
        busyloop_sleep_cycle_no_break(exec_busyloop, chunk_counter);
      }
    } else {
      CYCLE {
        busyloop_sleep_cycle_no_break(exec_busyloop, chunk_counter);

        if (break_at_stopping_time(max_duration, t1_abs, &t2_abs)) {
          break;
        }
      }
    }
  }

#undef CYCLE
}

MAIN_BEGIN("cpu_hog_cbs", "stderr", NULL)
{
  struct sigaction signalhandler = {
    .sa_handler = sighand,
  };
  sigset_t sigterm_mask, sigusr1_mask;
  sigemptyset(&sigterm_mask);
  sigaddset(&sigterm_mask, SIGTERM);
  sigprocmask(SIG_BLOCK, &sigterm_mask, NULL);
  if (sigaction(SIGTERM, &signalhandler, NULL) != 0) {
    fatal_syserror("Cannot install SIGTERM handler");
  }
  sigemptyset(&sigusr1_mask);
  sigaddset(&sigusr1_mask, SIGUSR1);
  sigprocmask(SIG_BLOCK, &sigusr1_mask, NULL);
  if (sigaction(SIGUSR1, &signalhandler, NULL) != 0) {
    fatal_syserror("Cannot install SIGUSR1 handler");
  }

  int exec_time_ms = -1;
  int break_time_ms = -1;
  struct timespec break_time;
  int cbs_budget_ms = -1;
  int cbs_period_ms = -1;
  int stopping_time = -1;
  relative_time max_duration;
  {
    int optchar;
    opterr = 0;
    while ((optchar = getopt(argc, argv, ":he:b:q:t:s:")) != -1) {
      switch (optchar) {
      case 'e':
        exec_time_ms = atoi(optarg);
        break;
      case 'b':
        break_time_ms = atoi(optarg);
        break;
      case 'q':
        cbs_budget_ms = atoi(optarg);
        break;
      case 't':
        cbs_period_ms = atoi(optarg);
        break;
      case 's':
        stopping_time = atoi(optarg);
        break;
      case 'h':
        printf("Usage: %s -e EXEC_TIME -b SLEEP_TIME -q CBS_BUDGET\n"
               "       -t CBS_PERIOD [-s STOPPING_TIME]\n"
               "\n"
               "This program will run busyloop chunks, each for the given\n"
               "execution time before sleeping for the stated duration.\n"
               "The busyloop-sleep cycle is done within CBS having the stated\n"
               "budget and period. If desired, the program can quit after\n"
               "the total time to perform the busyloop-sleep cycles exceed\n"
               "the given time. Otherwise, SIGTERM can be sent to quit the\n"
               "program gracefully. When the program quits gracefully, it\n"
               "prints on stdout the number of cycles fully performed\n"
               "followed by a newline followed by the elapsed time in second\n"
               "since the first chunk execution followed by a newline.\n"
               "SIGUSR1 must be sent to this program to start chunk execution."
               "\n"
               "-e EXEC_TIME is the busyloop duration in millisecond.\n"
               "   This should be greater than zero.\n"
               "-b SLEEP_TIME is the sleeping duration in millisecond.\n"
               "   This should be greater than or equal to zero.\n"
               "-q CBS_BUDGET is the CBS budget in millisecond.\n"
               "   This should be greater than zero.\n"
               "-t CBS_PERIOD is the CBS period in millisecond.\n"
               "   This should be greater than or equal to CBS_BUDGET.\n"
               "-i STOPPING_TIME is the elapsed time in ms since the\n"
               "   beginning of the first busyloop-sleep cycle after which\n"
               "   this program will quit before beginning the next cycle.\n",
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
  if (exec_time_ms <= 0) {
    fatal_error("EXEC_TIME must be properly specified (-h for help)");
  }
  if (break_time_ms < 0) {
    fatal_error("SLEEP_TIME must be properly specified (-h for help)");
  } else {
    relative_time break_duration;
    utility_time_init(&break_duration);
    to_utility_time(break_time_ms, ms, &break_duration);
    to_timespec(&break_duration, &break_time);
  }
  if (cbs_budget_ms <= 0) {
    fatal_error("CBS_BUDGET must be properly specified (-h for help)");
  }
  if (cbs_period_ms < cbs_budget_ms) {
    fatal_error("CBS_PERIOD must be properly specified (-h for help)");
  }
  if (stopping_time != -1 && stopping_time <= 0) {
    fatal_error("STOPPING_TIME must be at least 1 ms (-h for help)");
  } else {
    utility_time_init(&max_duration);
    to_utility_time(stopping_time, ms, &max_duration);
  }

  /* Create execution busyloop */
  cpu_busyloop *exec_busyloop = NULL;
  {
    int search_tolerance_us = 100;
    int search_passes = 10;
    int rc = create_cpu_busyloop(0,
                                 to_utility_time_dyn(exec_time_ms, ms),
                                 to_utility_time_dyn(search_tolerance_us, us),
                                 search_passes,
                                 &exec_busyloop);
    if (rc == -2) {
      fatal_error("%d is too small for execution busyloop", exec_time_ms);
    } else if (rc == -4) {
      fatal_error("%d is too big for execution busyloop", exec_time_ms);
    } else if (rc != 0) {
      fatal_error("Cannot create execution busyloop");
    }
  }  
  /* END: Create chunk busyloop */

  if (sigsetjmp(jmp_env, 0) == 0) {
    sigset_t start_signal;
    sigfillset(&start_signal);
    sigdelset(&start_signal, SIGUSR1);

    sigprocmask(SIG_UNBLOCK, &sigterm_mask, NULL);
    
    if (sched_deadline_enter(to_utility_time_dyn(cbs_budget_ms, ms),
                             to_utility_time_dyn(cbs_period_ms, ms),
                             NULL) != 0) {
      fatal_error("Cannot enter SCHED_DEADLINE (privilege may be insufficient)"
                  );
    }

    sigsuspend(&start_signal);

    hog_cpu(break_time_ms == 0 ? NULL : &break_time,
            stopping_time == -1 ? NULL : &max_duration,
            exec_busyloop, &t1_abs, &chunk_counter);

    print_stats(&t1_abs, chunk_counter);
  }

  destroy_cpu_busyloop(exec_busyloop);

  return EXIT_SUCCESS;

} MAIN_END
