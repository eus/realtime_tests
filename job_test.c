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
  /** The worst-case execution time of this hard real-time job (timing
      and function call overhead is included in this WCET). */
  relative_time job_duration;
  utility_time_init(&job_duration);
  to_utility_time(1, ms, &job_duration);

  /** This controls how many times the overhead can be larger than the
      one returned by function job_statistics_overhead. Specifically,
      the busyloop program will have a duration of job_duration -
      overhead_approximation_scaling * job_statistics_overhead() */
  unsigned overhead_approximation_scaling = 2;

  /** The number of jobs to be generated and checked for
      lateness. Beware that a large number of samples may cause memory
      swapping invalidating the approximation given by function
      job_statistics_overhead. */
  int sample_count = 512;

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

  /* Check that the scaled overhead is less than or equal to job duration */
  relative_time *job_stats_overhead_ptr;
  gracious_assert(job_statistics_overhead(0, &job_stats_overhead_ptr) == 0);

  job_stats_overhead_ptr
    = utility_time_mul_dyn_gc(job_stats_overhead_ptr,
                              overhead_approximation_scaling);

  relative_time job_stats_overhead;
  utility_time_init(&job_stats_overhead);
  utility_time_to_utility_time_gc(job_stats_overhead_ptr, &job_stats_overhead);

  gracious_assert_msg(utility_time_ge(&job_duration, &job_stats_overhead),
                      "job_duration is too short to account for the overhead");
  /* End of checking the scaled overhead vs. job duration */

  /* Serialize the scaled overhead to support analysis endeavour */
  struct timespec serialized_overhead;
  to_timespec(&job_stats_overhead, &serialized_overhead);
  gracious_assert(fwrite(&serialized_overhead, sizeof(serialized_overhead), 1,
                         job_stats_log_stream) == 1);
  /* End of serializing the scaled overhead */

  /* Prepare the job object using program busyloop_exact */
  relative_time *loop_duration = utility_time_sub_dyn(&job_duration,
                                                      &job_stats_overhead);

  struct busyloop_exact_args busyloop_exact_args;
  relative_time *error = to_utility_time_dyn(1, us);
  utility_time_set_gc_manual(error);
  gracious_assert(create_cpu_busyloop(0,
                                      utility_time_sub_dyn_gc(loop_duration,
                                                              error),
                                      error, 10,
                                      &busyloop_exact_args.busyloop_obj) == 0);
  utility_time_gc(error);

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
  gracious_assert(utility_time_eq_gc(&job_stats_overhead, t));
  /* End of deserializing the job duration overhead */

  /* Analyze the job statistics */
  char abs_t[15];
  const size_t abs_t_len = sizeof(abs_t);

  gracious_assert(to_string(&job_stats_overhead, abs_t, abs_t_len) == 0);
  log_verbose("Scaled job stats overhead = %s\n", abs_t);

  job_statistics job_stats;
  int rc = -2;
  nth_job = 1;
  int late_count = 0;
  while ((rc = job_statistics_read(job_stats_log_stream, &job_stats)) == 0) {

    absolute_time time_start;
    utility_time_init(&time_start);
    utility_time_to_utility_time_gc(job_statistics_time_start(&job_stats),
                                    &time_start);

    absolute_time time_finish;
    utility_time_init(&time_finish);
    utility_time_to_utility_time_gc(job_statistics_time_finish(&job_stats),
                                    &time_finish);

    relative_time delta;
    utility_time_init(&delta);
    utility_time_sub(&time_finish, &time_start, &delta);
    gracious_assert(to_string(&delta, abs_t, abs_t_len) == 0);

    fprintf(report, "%5d%15s", nth_job, abs_t);
    if (utility_time_gt(&delta, &job_duration)) {
      fprintf(report, " LATE");
      log_error("Job #%d of %d is late (t_finish - t_start = %s)",
                nth_job, sample_count, abs_t);
      late_count++;
    }
    fprintf(report, "\n");

    nth_job++;
  }

  if (strcmp(argv[1], "0") == 0) {
    gracious_assert_msg(late_count == 0, "late count is %d out of %d",
                        late_count, sample_count);
  } else {
    if (late_count != 0) {
      log_error("late count is %d out of %d", late_count, sample_count);
    }
  }

  gracious_assert(rc != -2);
  gracious_assert(utility_file_close(job_stats_log_stream, tmp_file_name) == 0);
  /* End of reading the job statistics */

  /* Clean-up */
  gracious_assert(utility_file_close(report, report_path) == 0);

  return EXIT_SUCCESS;

} MAIN_UNIT_TEST_END
