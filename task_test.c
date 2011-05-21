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
#include "utility_testcase.h"
#include "utility_log.h"
#include "utility_cpu.h"
#include "utility_file.h"
#include "utility_sched_fifo.h"
#include "utility_time.h"
#include "job.h"
#include "task.h"

static char tmp_file_name[] = "task_test_XXXXXX";
static cpu_freq_governor *used_gov = NULL;
static int used_gov_in_use = 0;
static void cleanup(void)
{
  if (remove(tmp_file_name) != 0 && errno != ENOENT) {
    log_syserror("Unable to remove the temporary file");
  }
  if (used_gov_in_use) {
    int rc;
    if ((rc = cpu_freq_restore_governor(used_gov)) != 0) {
      log_error("You must restore the CPU freq governor yourself");
    }
  }
}

static void log_verbose_utility_time(const utility_time *t, const char *msg)
{
  char abs_t[32];
  const size_t abs_t_len = sizeof(abs_t);
  gracious_assert(to_string(t, abs_t, abs_t_len) == 0);
  log_verbose("%s = %s\n", msg, abs_t);
}

struct task_params
{
  task *tau;
  int exit_status;
};
void *task_thread(void *args)
{
  struct task_params *params = args;
  params->exit_status = 0;

  /* Be an RT thread with the second highest priority */
  int second_max_prio;
  gracious_assert(sched_fifo_prio(1, &second_max_prio) == 0);
  gracious_assert(sched_fifo_enter(second_max_prio, NULL) == 0);  
  /* END: Be an RT thread with the second highest priority */

  /* Run the task */
  gracious_assert(task_start(params->tau) == 0);
  /* END: Run the task */

  return &params->exit_status;
}
struct task_manager_params
{
  task *tau;
  struct timespec stopping_time; /* Absolute time */
  int exit_status;
};
void *task_manager_thread(void *args)
{
  struct task_manager_params *params = args;
  params->exit_status = 0;

  /* Be a non-preemptible RT thread with the highest prioprity */
  gracious_assert(sched_fifo_enter_max(NULL) == 0);  
  /* END: Be a non-preemptible RT thread with the highest prioprity */

  /* Create a task thread */
  pthread_t task_thread_tid;
  struct task_params tau_params = {
    .tau = params->tau,
  };
  gracious_assert(pthread_create(&task_thread_tid, NULL, task_thread,
                                 &tau_params) == 0);
  /* END: Create a task thread */

  /* Wait for the time to stop the task thread */
  gracious_assert(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                  &params->stopping_time, NULL) == 0);  
  /* END: Wait for the time to stop the task thread */

  /* Stop the task thread */
  task_stop(params->tau);
  gracious_assert(pthread_join(task_thread_tid, NULL) == 0);
  gracious_assert(tau_params.exit_status == 0);
  /* END: Stop the task thread */

  return &params->exit_status;
}
static void testcase_1_periodic_task(FILE *report,
                                     const relative_time *job_duration,
                                     unsigned overhead_approximation_scaling,
                                     const relative_time *job_stats_overhead,
                                     unsigned sample_count,
                                     int under_valgrind)
{
  /* Get and print finish-to-start overhead */
  relative_time *finish_to_start;
  gracious_assert(finish_to_start_overhead(0, 0, &finish_to_start) == 0);
  log_verbose_utility_time(finish_to_start, "Finish-to-start overhead");
  /* END: Get and print finish-to-start overhead */

  /* Subtracting the periodic overhead from the given job duration */
  relative_time periodic_overhead;
  utility_time_init(&periodic_overhead);
  utility_time_add_gc(finish_to_start, job_stats_overhead, &periodic_overhead);
  utility_time_mul(&periodic_overhead, overhead_approximation_scaling,
                   &periodic_overhead);
  log_verbose_utility_time(&periodic_overhead, "Periodic overhead");

  gracious_assert_msg(utility_time_ge(job_duration, &periodic_overhead),
                      "job_duration is too short to account for"
                      " scaled the periodic overhead");
  /* End of subtracting the periodic overhead from the given job duration */

  /* Prepare the job object using program busyloop_exact */
  relative_time *loop_duration = utility_time_sub_dyn(job_duration,
                                                      &periodic_overhead);

  struct busyloop_exact_args busyloop_periodic;
  relative_time *error = to_utility_time_dyn(1, us);
  utility_time_set_gc_manual(error);
  gracious_assert(create_cpu_busyloop(0,
                                      utility_time_sub_dyn_gc(loop_duration,
                                                              error),
                                      error, 10,
                                      &busyloop_periodic.busyloop_obj) == 0);
  utility_time_gc(error);
  /* End of preparing the job object using program busyloop_exact */

  /* Calculate task offset */
  struct timespec t_now;
  gracious_assert(clock_gettime(CLOCK_MONOTONIC, &t_now) == 0);

  absolute_time *release_time
    = utility_time_add_dyn_gc(timespec_to_utility_time_dyn(&t_now),
                              to_utility_time_dyn(1, s));
  utility_time_set_gc_manual(release_time);
  /* END: Calculate task offset */

  /* Calculate task period */
  relative_time *task_period = utility_time_mul_dyn(job_duration, 1);
  utility_time_set_gc_manual(task_period);
  /* END: Calculate task period */

  /* Create periodic task */
  task *periodic_task = NULL;
  char task_name[] = "testcase_1_periodic_task";
  gracious_assert(task_create(task_name,
                              job_duration,
                              task_period,
                              task_period,
                              release_time,
                              NULL, NULL,
                              tmp_file_name,
                              job_stats_overhead,
                              &periodic_overhead,
                              0,
                              busyloop_exact,
                              &busyloop_periodic,
                              &periodic_task) == 0);
  gracious_assert(periodic_task != NULL);
  /* END: Create periodic task */

  /* Check task parameters */
  task_name[0] = 'T';

  struct task_stats_checker_params
  {
    char *name;
    const relative_time *wcet;
    relative_time *period;
    relative_time *deadline;
    absolute_time *offset;
    int aperiodic;
    int job_statistics_disabled;
    const relative_time *job_stats_overhead;
    relative_time *finish_to_start_overhead;
  } expected_task_params = {
    .name = "testcase_1_periodic_task",
    .wcet = job_duration,
    .period = task_period,
    .deadline = task_period,
    .offset = release_time,
    .aperiodic = 0,
    .job_statistics_disabled = 0,
    .job_stats_overhead = job_stats_overhead,
    .finish_to_start_overhead = &periodic_overhead,
  };
  int task_stats_checker(task *tau, void *args)
  {
    struct task_stats_checker_params *prms = args;
    gracious_assert(strcmp(task_statistics_name(tau), prms->name) == 0);

    gracious_assert(utility_time_eq_gc_t2(prms->wcet,
                                          task_statistics_wcet(tau)));
    gracious_assert(utility_time_eq_gc_t2(prms->period,
                                        task_statistics_period(tau)));
    gracious_assert(utility_time_eq_gc_t2(prms->deadline,
                                          task_statistics_deadline(tau)));
    gracious_assert(utility_time_eq_gc_t2(prms->offset,
                                          task_statistics_offset(tau)));
    gracious_assert(task_statistics_aperiodic(tau) == prms->aperiodic);
    gracious_assert(task_statistics_job_statistics_disabled(tau)
                    == prms->job_statistics_disabled);
    gracious_assert(utility_time_eq_gc_t2(prms->job_stats_overhead,
                                 task_statistics_job_statistics_overhead(tau)));
    gracious_assert(utility_time_eq_gc_t2(prms->finish_to_start_overhead,
                                task_statistics_finish_to_start_overhead(tau)));
    return 0;
  }
  task_stats_checker(periodic_task, &expected_task_params);
  /* END: Check task parameters */

  /* Run task */
  pthread_t task_manager_tid;

  /** Calculate stopping time **/
  struct task_manager_params params = {
    .tau = periodic_task,
  };
  to_timespec_gc(utility_time_add_dyn_gc(release_time,
                                         utility_time_mul_dyn(task_period,
                                                              sample_count + 1)
                                         ),
                 &params.stopping_time);
  /** END: Calculate stopping time **/

  gracious_assert(pthread_create(&task_manager_tid, NULL,
                                 task_manager_thread, &params) == 0);
  gracious_assert(pthread_join(task_manager_tid, NULL) == 0);
  gracious_assert(params.exit_status == 0);
  /* END: Run task */

  /* Check task timeliness */
  FILE *stats_file = utility_file_open_for_reading_bin(tmp_file_name);
  gracious_assert(stats_file != NULL);

  struct job_stats_checker_params
  {
    unsigned nth_job;
    unsigned sample_count;
    absolute_time *t_release;
    relative_time *period;
    unsigned late_count;
    FILE *report;
    const relative_time *job_stats_overhead;
  } expected_job_params = {
    .nth_job = 1,
    .t_release = release_time,
    .period = task_period,
    .sample_count = sample_count,
    .report = report,
    .late_count = 0,
    .job_stats_overhead = job_stats_overhead,
  };
  int job_stats_checker(job_statistics *stats, void *args)
  {
    struct job_stats_checker_params *prms = args;

    if (prms->nth_job > prms->sample_count) {
      /* Do not check the last job because the timing includes the
         preemption time from manager thread */
      return 0;
    } else if (prms->nth_job == 1) {
      fprintf(prms->report, "%5s%15s%15s%15s%15s%15s%15s%15s\n",
              "#job", "release", "delta_s", "start", "deadline", "delta_f",
              "finish", "exec_time");
    }

    fprintf(prms->report, "%5d", prms->nth_job);

    char abs_t[15];

    char later_buf[15];
    const size_t later_len = sizeof(later_buf) - 1;
    later_buf[0] = '+';
    char *later = &later_buf[1];

    char earlier_buf[15];
    const size_t earlier_len = sizeof(earlier_buf) - 1;
    earlier_buf[0] = '-';
    char *earlier = &earlier_buf[1];

    relative_time delta;
    utility_time_init(&delta);

    /* Start time */
    utility_time_mul(prms->period, prms->nth_job - 1, &delta);
    gracious_assert(to_string(&delta, abs_t, sizeof(abs_t)) == 0);
    fprintf(prms->report, "%15s", abs_t);

    absolute_time time_start_expected;
    utility_time_init(&time_start_expected);
    utility_time_add(prms->t_release, &delta, &time_start_expected);

    absolute_time time_start;
    utility_time_init(&time_start);
    utility_time_to_utility_time_gc(job_statistics_time_start(stats),
                                    &time_start);

    if (utility_time_lt(&time_start_expected, &time_start)) {
      gracious_assert(to_string_gc(utility_time_sub_dyn(&time_start,
                                                        &time_start_expected),
                                   later, later_len) == 0);
      fprintf(prms->report, "%15s", later_buf);
    } else {
      gracious_assert(to_string_gc(utility_time_sub_dyn(&time_start_expected,
                                                        &time_start),
                                   earlier, earlier_len) == 0);
      fprintf(prms->report, "%15s", earlier_buf);
    }

    gracious_assert(to_string_gc(utility_time_sub_dyn(&time_start,
                                                      prms->t_release),
                                 abs_t, sizeof(abs_t)) == 0);
    fprintf(prms->report, "%15s", abs_t);
    /* End of start time */

    /* Finishing time */
    utility_time_mul(prms->period, prms->nth_job, &delta);
    gracious_assert(to_string(&delta, abs_t, sizeof(abs_t)) == 0);
    fprintf(prms->report, "%15s", abs_t);

    absolute_time time_finish_expected;
    utility_time_init(&time_finish_expected);
    utility_time_add(prms->t_release, &delta, &time_finish_expected);

    absolute_time time_finish;
    utility_time_init(&time_finish);
    utility_time_to_utility_time_gc(job_statistics_time_finish(stats),
                                    &time_finish);
    if (utility_time_lt(&time_finish_expected, &time_finish)) {
      gracious_assert(to_string_gc(utility_time_sub_dyn(&time_finish,
                                                        &time_finish_expected),
                                   later, later_len) == 0);
      fprintf(prms->report, "%15s", later_buf);

      log_error("Job #%d of %d is late (deadline - t_finish = %s)",
                prms->nth_job, prms->sample_count, later_buf);
      prms->late_count++;
    } else {
      gracious_assert(to_string_gc(utility_time_sub_dyn(&time_finish_expected,
                                                        &time_finish),
                                   earlier, earlier_len) == 0);
      fprintf(prms->report, "%15s", earlier_buf);
    }

    gracious_assert(to_string_gc(utility_time_sub_dyn(&time_finish,
                                                      prms->t_release),
                                 abs_t, sizeof(abs_t)) == 0);
    fprintf(prms->report, "%15s", abs_t);
    /* End of finishing time */

    /* Execution time */
    utility_time_sub(&time_finish, &time_start, &delta);
    gracious_assert(to_string(&delta, abs_t, sizeof(abs_t)) == 0);
    fprintf(prms->report, "%15s", abs_t);
    /* End of execution time */

    fprintf(prms->report, "\n");

    prms->nth_job++;

    return 0;
  }
  gracious_assert(task_statistics_read(stats_file,
                                       task_stats_checker,
                                       &expected_task_params,
                                       job_stats_checker,
                                       &expected_job_params) == 0);

  if (under_valgrind) {
    if (expected_job_params.late_count != 0) {
      log_error("Late count = %u is not zero", expected_job_params.late_count);
    }
  } else {
    gracious_assert_msg(expected_job_params.late_count == 0,
                        "Late count = %u is not zero",
                        expected_job_params.late_count);
  }
  /* END: Check task timeliness */

  /* Check EOF */
  int task_stats_nocall(task *tau, void *args)
  {
    gracious_assert_msg(0, "task_stats_nocall must never be executed");
  }
  int job_stats_nocall(job_statistics *stats, void *args)
  {
    gracious_assert_msg(0, "job_stats_nocall must never be executed");
  }
  gracious_assert(task_statistics_read(stats_file,
                                       task_stats_nocall, NULL,
                                       job_stats_nocall, NULL) == -4);
  /* END: Check EOF */

  /* Check early bailout from task_statistics_fn */
  gracious_assert(fseek(stats_file, 0, SEEK_SET) == 0);
  int task_stats_bailout(task *tau, void *args)
  {
    return -1;
  }
  gracious_assert(task_statistics_read(stats_file,
                                       task_stats_bailout, NULL,
                                       job_stats_nocall, NULL) == -1);
  /* END: Check early bailout from task_statistics_fn */

  /* Check early bailout from job_statistics_fn */
  gracious_assert(fseek(stats_file, 0, SEEK_SET) == 0);
  int job_stats_bailout(job_statistics *stats, void *args)
  {
    return -1;
  }
  gracious_assert(task_statistics_read(stats_file,
                                       task_stats_checker,
                                       &expected_task_params,
                                       job_stats_bailout, NULL) == -2);
  gracious_assert(!feof(stats_file));
  /* END: Check early bailout from job_statistics_fn */

  /* Clean-up */
  gracious_assert(utility_file_close(stats_file, tmp_file_name) == 0);
  utility_time_gc(release_time);
  utility_time_gc(task_period);
  task_destroy(periodic_task);
  destroy_cpu_busyloop(busyloop_periodic.busyloop_obj);
  /* END: Clean-up */
}

static void testcase_2_aperiodic_task(FILE *report,
                                      const relative_time *job_duration,
                                      unsigned overhead_approximation_scaling,
                                      const relative_time *job_stats_overhead,
                                      unsigned sample_count,
                                      int under_valgrind)
{
  /* Subtracting the aperiodic overhead from the given job duration */
  relative_time *aperiodic_overhead_ptr;
  gracious_assert(finish_to_start_overhead(0, 1, &aperiodic_overhead_ptr) == 0);

  relative_time aperiodic_overhead;
  utility_time_init(&aperiodic_overhead);
  utility_time_to_utility_time_gc(aperiodic_overhead_ptr, &aperiodic_overhead);

  log_verbose_utility_time(&aperiodic_overhead, "Aperiodic task overhead");

  gracious_assert_msg(utility_time_ge(job_duration, &aperiodic_overhead),
                      "job_duration is too short to account for"
                      " the aperiodic overhead");
  /* End of subtracting the aperiodic overhead from the given job duration */
}

static relative_time *job_stats_overhead(void)
{
  relative_time *job_stats_overhead;
  gracious_assert(job_statistics_overhead(0, &job_stats_overhead) == 0);
  utility_time_set_gc_manual(job_stats_overhead);
  log_verbose_utility_time(job_stats_overhead, "Job statistics overhead");

  return job_stats_overhead;
}

MAIN_UNIT_TEST_BEGIN("task_test", "stderr", NULL, cleanup)
{
  require_valgrind_indicator();

  int tmp_file_fd = mkstemp(tmp_file_name);
  if (tmp_file_fd == -1) {
    fatal_syserror("Unable to open a temporary file");
  }
  if (close(tmp_file_fd) != 0) {
    fatal_syserror("Unable to close the temporary file");
  }

  /* Adjustable test parameters */
  /** The worst-case execution time of this hard real-time job (timing
      and function call overhead is included in this WCET). */
  relative_time job_duration;
  utility_time_init(&job_duration);
  to_utility_time(1, ms, &job_duration);

  /** This controls how many times the overhead can be larger than the
      one returned by function job_statistics_overhead. Specifically,
      the busyloop program will have a duration of job_duration -
      overhead_approximation_scaling * job_statistics_overhead() */
  unsigned overhead_approximation_scaling = 1;

  /** The number of jobs to be generated and checked for
      lateness. Beware that a large number of samples may cause memory
      swapping invalidating the approximation given by function
      job_statistics_overhead. */
  int sample_count = 512;

  /** Change this to /dev/stdout to see the job statistics */
  const char *report_path = "/dev/null";
  FILE *report = utility_file_open_for_writing(report_path);
  /* End of adjustable test parameters */
  
  /* Setup the environment to have a stable CPU frequency */
  gracious_assert(enter_UP_mode_freq_max(&used_gov) == 0);
  used_gov_in_use = 1;
  /* End of environment setup */

  /* Obtain worst-case job statistics overhead */
  relative_time *job_overhead = job_stats_overhead();
  /* END: Obtain worst-case job statistics overhead */


  /* Testcase 1: Periodic task */
  testcase_1_periodic_task(report, &job_duration,
                           overhead_approximation_scaling,
                           job_overhead, sample_count,
                           strcmp(argv[1], "1") == 0);

  /* [TODO] Testcase 2: Aperiodic task */
  testcase_2_aperiodic_task(report, &job_duration,
                            overhead_approximation_scaling,
                            job_overhead, sample_count,
                            strcmp(argv[1], "1") == 0);

  /* Clean-up */
  gracious_assert(utility_file_close(report, report_path) == 0);
  utility_time_gc(job_overhead);

  return EXIT_SUCCESS;

} MAIN_UNIT_TEST_END
