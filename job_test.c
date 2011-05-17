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
#include "job.h"

static char tmp_file_name[] = "job_test_XXXXXX";
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

MAIN_UNIT_TEST_BEGIN("job_test", "stderr", NULL, cleanup)
{
  require_valgrind_indicator();

  /* Adjustable test parameters */
  /** The worst-case duration that is accountable including timing and
      function call overhead. */
  relative_time job_duration;
  utility_time_init(&job_duration);
  to_utility_time(20, ms, &job_duration);

  /** The number of jobs to be generated and checked. Beware that a
      large number of samples may cause memory swapping invalidating
      the worst-case assumption given by function
      job_statistics_overhead. */
  int sample_count = 1000;

  /** This controls how much the busy loop can deviate from the range
      of [20 - job_statistics_overhead(), 20] ms. Such a range arises
      because job_statistics_overhead() returns the worst-case
      overhead. This test unit checks that the actual duration of each
      job as recorded is in the range of
      [20 - job_statistics_overhead() - loop_tolerance, 20 + loop_tolerance] */
  relative_time loop_tolerance;
  utility_time_init(&loop_tolerance);
  to_utility_time(10, us, &loop_tolerance);

  /** Change this to /dev/stdout to see the job statistics */
  const char *report_path = "/dev/null";
  FILE *report = utility_file_open_for_writing(report_path);
  /* End of adjustable test parameters */

  /* Prepare the stream to log job statistics */
  int tmp_file_fd = mkstemp(tmp_file_name);
  if (tmp_file_fd == -1) {
    fatal_syserror("Unable to open a temporary file");
  }
  if (close(tmp_file_fd) != 0) {
    fatal_syserror("Unable to close the temporary file");
  }

  FILE *job_stats_log_stream = utility_file_open_for_writing_bin(tmp_file_name);
  gracious_assert(job_stats_log_stream != NULL);
  /* End of preparing the stream to log job statistics */

  /* Setup the environment to run a non-preemptable RT task */
  gracious_assert(enter_UP_mode_freq_max(&used_gov) == 0);
  used_gov_in_use = 1;

  struct scheduler default_scheduler;
  gracious_assert(sched_fifo_enter_max(&default_scheduler) == 0);
  /* End of environment setup */

  /* Subtracting the overhead from the given job duration */
  relative_time *job_duration_overhead_ptr;
  gracious_assert(job_statistics_overhead(0, &job_duration_overhead_ptr) == 0);

  relative_time job_duration_overhead;
  utility_time_init(&job_duration_overhead);
  utility_time_to_utility_time_gc(job_duration_overhead_ptr,
                                  &job_duration_overhead);

  gracious_assert_msg(utility_time_ge(&job_duration, &job_duration_overhead),
                      "job_duration is too short to account for the overhead");
  /* End of subtracting the overhead from the given job duration */

  /* Serialize the overhead to support analysis endeavour */
  struct timespec serialized_overhead;
  to_timespec(&job_duration_overhead, &serialized_overhead);
  gracious_assert(fwrite(&serialized_overhead, sizeof(serialized_overhead), 1,
                         job_stats_log_stream) == 1);
  /* End of serializing the overhead */

  /* Prepare the job object using program busyloop_exact */
  relative_time *loop_duration = utility_time_sub_dyn(&job_duration,
                                                      &job_duration_overhead);

  struct busyloop_exact_args busyloop_exact_args;
  gracious_assert(create_cpu_busyloop(0, loop_duration, &loop_tolerance, 10,
                                      &busyloop_exact_args.busyloop_obj) == 0);

  struct job job = {
    .run_program = busyloop_exact,
    .args = &busyloop_exact_args,
  };
  /* End of preparing the job object using program busyloop_exact */

  /* Execute the job one after another */
  int nth_job;
  int job_start_rc = 0;
  for (nth_job = 1; nth_job <= sample_count; nth_job++) {
    job_start_rc += job_start(job_stats_log_stream, &job);
  }
  gracious_assert(job_start_rc == 0);
  /* End of executing a stream of jobs */

  /* Restore the former environment */
  destroy_cpu_busyloop(busyloop_exact_args.busyloop_obj);

  gracious_assert(sched_fifo_leave(&default_scheduler) == 0);

  gracious_assert(cpu_freq_restore_governor(used_gov) == 0);
  used_gov_in_use = 0;
  /* End of restoring the former environment */

  /* Close & reopen job statistics log stream for analysis */
  gracious_assert(utility_file_close(job_stats_log_stream, tmp_file_name) == 0);

  job_stats_log_stream = utility_file_open_for_reading_bin(tmp_file_name);
  gracious_assert(job_stats_log_stream != NULL);
  /* End of closing & reopening job statistics log stream for analysis */

  /* Deserialize the job duration overhead */
  struct timespec deserialized_overhead;
  gracious_assert(fread(&deserialized_overhead, 1, sizeof(struct timespec),
                        job_stats_log_stream) == sizeof(struct timespec));
  utility_time *t = timespec_to_utility_time_dyn(&deserialized_overhead);
  gracious_assert(utility_time_eq_gc(&job_duration_overhead, t));
  /* End of deserializing the job duration overhead */

  /* Analyze the job statistics */
  char abs_t[15];
  const size_t abs_t_len = sizeof(abs_t);

  gracious_assert(to_string(&job_duration_overhead, abs_t, abs_t_len) == 0);
  log_verbose("Worst-case job duration overhead = %s\n", abs_t);

  job_statistics job_stats;
  int rc = -2;
  nth_job = 1;
  fprintf(report, "%5s%15s%15s%15s%15s%15s%15s%15s\n",
         "#job", "release", "delta_s", "start", "deadline", "delta_f", "finish",
         "exec_time");
  char later_buf[15];
  const size_t later_len = sizeof(later_buf) - 1;
  later_buf[0] = '+';
  char *later = &later_buf[1];

  char earlier_buf[15];
  const size_t earlier_len = sizeof(earlier_buf) - 1;
  earlier_buf[0] = '-';
  char *earlier = &earlier_buf[1];

  char char_lower_bound[32];
  char char_upper_bound[32];

  int err_job_statistics_overhead = 0;
  absolute_time t_0;
  utility_time_init(&t_0);
  while ((rc = job_statistics_read(job_stats_log_stream, &job_stats)) == 0) {
    if (nth_job == 1) {
      utility_time_to_utility_time_gc(job_statistics_time_start(&job_stats),
                                      &t_0);
    }

    fprintf(report, "%5d", nth_job);

    relative_time delta;
    utility_time_init(&delta);

    /* Start time */
    utility_time_mul(&job_duration, nth_job - 1, &delta);
    gracious_assert(to_string(&delta, abs_t, abs_t_len) == 0);
    fprintf(report, "%15s", abs_t);

    absolute_time time_start_expected;
    utility_time_init(&time_start_expected);
    utility_time_add(&t_0, &delta, &time_start_expected);

    absolute_time time_start;
    utility_time_init(&time_start);
    utility_time_to_utility_time_gc(job_statistics_time_start(&job_stats),
                                    &time_start);

    if (utility_time_lt(&time_start_expected, &time_start)) {
      gracious_assert(to_string_gc(utility_time_sub_dyn(&time_start,
                                                        &time_start_expected),
                                   later, later_len) == 0);
      fprintf(report, "%15s", later_buf);
    } else {
      gracious_assert(to_string_gc(utility_time_sub_dyn(&time_start_expected,
                                                        &time_start),
                                   earlier, earlier_len) == 0);
      fprintf(report, "%15s", earlier_buf);
    }

    gracious_assert(to_string_gc(utility_time_sub_dyn(&time_start, &t_0),
                                 abs_t, abs_t_len) == 0);
    fprintf(report, "%15s", abs_t);
    /* End of start time */

    /* Finishing time */
    utility_time_mul(&job_duration, nth_job, &delta);
    gracious_assert(to_string(&delta, abs_t, abs_t_len) == 0);
    fprintf(report, "%15s", abs_t);

    absolute_time time_finish_expected;
    utility_time_init(&time_finish_expected);
    utility_time_add(&t_0, &delta, &time_finish_expected);

    absolute_time time_finish;
    utility_time_init(&time_finish);
    utility_time_to_utility_time_gc(job_statistics_time_finish(&job_stats),
                                    &time_finish);
    if (utility_time_lt(&time_finish_expected, &time_finish)) {
      gracious_assert(to_string_gc(utility_time_sub_dyn(&time_finish,
                                                        &time_finish_expected),
                                   later, later_len) == 0);
      fprintf(report, "%15s", later_buf);
    } else {
      gracious_assert(to_string_gc(utility_time_sub_dyn(&time_finish_expected,
                                                        &time_finish),
                                   earlier, earlier_len) == 0);
      fprintf(report, "%15s", earlier_buf);
    }

    gracious_assert(to_string_gc(utility_time_sub_dyn(&time_finish, &t_0),
                                 abs_t, abs_t_len) == 0);
    fprintf(report, "%15s", abs_t);
    /* End of finishing time */

    /* Execution time */
    utility_time_sub(&time_finish, &time_start, &delta);
    gracious_assert(to_string(&delta, abs_t, abs_t_len) == 0);
    fprintf(report, "%15s", abs_t);

    utility_time lower_bound;
    utility_time_init(&lower_bound);
    utility_time_sub_gc(utility_time_sub_dyn(&job_duration,
                                             &job_duration_overhead),
                        &loop_tolerance, &lower_bound);
    gracious_assert(to_string(&lower_bound, char_lower_bound,
                              sizeof(char_lower_bound)) == 0);

    utility_time upper_bound;
    utility_time_init(&upper_bound);
    utility_time_add(&job_duration, &loop_tolerance, &upper_bound);
    gracious_assert(to_string(&upper_bound, char_upper_bound,
                              sizeof(char_upper_bound)) == 0);

    if (utility_time_lt(&delta, &lower_bound)
        || utility_time_gt(&delta, &upper_bound)) {
      /* If this is run under Valgrind, this is expected.  If Valgrind
         reports no leak, run this again without Valgrind. If this
         messages persists, this testcase really fails. */

      if (strcmp(argv[1], "1") != 0) {
        fprintf(report, "\n");
      }
      char char_delta[32];
      gracious_assert(to_string(&delta, char_delta, sizeof(char_delta)) == 0);
      if (strcmp(argv[1], "1") != 0) {
        err_job_statistics_overhead = 1;
        log_error("job_statistics_overhead() is incorrect for job #%d:"
                  " %s not in [%s, %s]",
                  nth_job, char_delta, char_lower_bound, char_upper_bound);
      }
    }
    /* End of execution time */

    fprintf(report, "\n");

    nth_job++;
  }

  fprintf(report,
          "Note:\tPay attention to delta_s and delta_f.\n"
          "\tIf both are monotonically increasing, the job keeps being late.\n"
          "\tBut, this test unit only makes sure that the execution duration\n"
          "\tis as expected: [%s, %s])\n"
          "\tbecause this test unit only tests the functionalities provided\n"
          "\tby infrastructure component job.h.\n",
         char_lower_bound, char_upper_bound);

  gracious_assert(!err_job_statistics_overhead);

  gracious_assert(rc != -2);
  gracious_assert(utility_file_close(job_stats_log_stream, tmp_file_name) == 0);
  /* End of reading the job statistics */

  /* Clean-up */
  gracious_assert(utility_file_close(report, report_path) == 0);

  return EXIT_SUCCESS;

} MAIN_UNIT_TEST_END
