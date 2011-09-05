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
#include "utility_memory.h"
#include "job.h"

static cpu_freq_governor *used_gov = NULL;
static int used_gov_in_use = 0;
static void cleanup(void)
{
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

  if (under_valgrind()) {
    /* Since Valgrind instruments the access to every variable, the
       busyloop operation involving incrementation of a variable is
       really slowed down to the point it is impossible to perform
       busyloop search. */

    return EXIT_SUCCESS;
  }

  /* Lock memory and preallocate stack */
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

  memory_preallocate_stack(1024);
  /* END: Lock memory and preallocate stack */

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
  unsigned overhead_approximation_scaling = 10;

  /** The number of jobs to be generated and checked for
      lateness. Beware that a large number of samples may cause memory
      swapping invalidating the approximation given by function
      job_statistics_overhead. */
  int sample_count = 512;

  /** Change this to /dev/stdout to see the job statistics */
  const char *report_path = "/dev/null";
  FILE *report = utility_file_open_for_writing(report_path);

  /** This controls how much the busyloop can deviate **/
  relative_time *error = to_utility_time_dyn(50, us);
  utility_time_set_gc_manual(error);
  /* End of adjustable test parameters */

  /*
   * ring_x_y where x can be "nowrap" if it has enough slots to not overrun or
   *                         "wrap"   if it has not enough slots and
   *                y can be "disabled" if overrun is disabled or
   *                         "enabled"  if overrun is enabled.
   */
#define make_ring_name(wrap_cond, overrun_cond) \
  ring_ ## wrap_cond ## _ ## overrun_cond

#define make_ring_stream_name(wrap_cond, overrun_cond) \
  ring_ ## wrap_cond ## _ ## overrun_cond ## _stream

  /* Prepare the streams to log job statistics */
#define open_tmpfile(ring_stream_name)          \
  FILE *ring_stream_name = tmpfile();           \
  gracious_assert(ring_stream_name != NULL)

  open_tmpfile(make_ring_stream_name(nowrap, enabled));
  open_tmpfile(make_ring_stream_name(nowrap, disabled));
  open_tmpfile(make_ring_stream_name(wrap, enabled));
  open_tmpfile(make_ring_stream_name(wrap, disabled));

  open_tmpfile(make_ring_stream_name(wrap_2, enabled));
  open_tmpfile(make_ring_stream_name(wrap_3, enabled));
  open_tmpfile(make_ring_stream_name(wrap_4, disabled));

#undef open_tmpfile
  /* End of preparing the streams to log job statistics */

  /* Prepare the ring buffers to record job statistics. */
#define create_ringbuf(ring_name, disable_overrun)                      \
  jobstats_ringbuf *ring_name;                                          \
  ring_name = jobstats_ringbuf_create(sample_count, disable_overrun)

  create_ringbuf(make_ring_name(nowrap, enabled), 0);
  create_ringbuf(make_ring_name(nowrap, disabled), 1);
  create_ringbuf(make_ring_name(wrap, enabled), 0);
  create_ringbuf(make_ring_name(wrap, disabled), 1);

  create_ringbuf(make_ring_name(wrap_2, enabled), 0);
  create_ringbuf(make_ring_name(wrap_3, enabled), 0);
  create_ringbuf(make_ring_name(wrap_4, disabled), 1);

#undef create_ringbuf
  /* END: Prepare the ring buffers to record job statistics */

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

#define serialize_overhead(ring_stream_name)            \
  gracious_assert(fwrite(&serialized_overhead,          \
                         sizeof(serialized_overhead),   \
                         1, ring_stream_name) == 1)

  serialize_overhead(make_ring_stream_name(nowrap, enabled));
  serialize_overhead(make_ring_stream_name(nowrap, disabled));
  serialize_overhead(make_ring_stream_name(wrap, enabled));
  serialize_overhead(make_ring_stream_name(wrap, disabled));

  serialize_overhead(make_ring_stream_name(wrap_2, enabled));
  serialize_overhead(make_ring_stream_name(wrap_3, enabled));
  serialize_overhead(make_ring_stream_name(wrap_4, disabled));

#undef serialize_overhead
  /* End of serializing the scaled overhead */

  /* Prepare the job object using program busyloop_exact */
  relative_time *loop_duration = utility_time_sub_dyn(&job_duration,
                                                      &job_stats_overhead);

  struct busyloop_exact_args busyloop_exact_args;
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

  /* Execute the job one after another for all rings */
  int nth_job;
  int job_start_rc = 0;

#define execute_job(ring_name, job_count)               \
  for (nth_job = 1; nth_job <= job_count; nth_job++) {  \
    job_start_rc += job_start(ring_name, &job);         \
  }                                                     \
  gracious_assert(job_start_rc == 0)

  execute_job(make_ring_name(nowrap, enabled), sample_count);
  execute_job(make_ring_name(nowrap, disabled), sample_count);
  execute_job(make_ring_name(wrap, enabled), sample_count + 1);
  execute_job(make_ring_name(wrap, disabled), sample_count + 1);

  execute_job(make_ring_name(wrap_2, enabled), sample_count * 4);
  execute_job(make_ring_name(wrap_3, enabled), sample_count * 3 + 50);
  execute_job(make_ring_name(wrap_4, disabled), sample_count * 3 + 50);

#undef execute_job
  /* End of executing a stream of jobs */

  /* Restore the former environment */
  destroy_cpu_busyloop(busyloop_exact_args.busyloop_obj);

  gracious_assert(sched_fifo_leave(&default_scheduler) == 0);

  gracious_assert(cpu_freq_restore_governor(used_gov) == 0);
  used_gov_in_use = 0;
  /* End of restoring the former environment */

  /* Check ring buffers work correctly */
#define check_ring_buffer(expected_overrun, expected_overrun_count,     \
                          expected_overrun_disabled, expected_size,     \
                          expected_write_count, expected_oldest_pos,    \
                          expected_lost_count, ringbuf)                 \
  do {                                                                  \
    gracious_assert(jobstats_ringbuf_overrun(ringbuf)                   \
                    == expected_overrun);                               \
    gracious_assert(jobstats_ringbuf_overrun_count(ringbuf)             \
                    == expected_overrun_count);                         \
    gracious_assert(jobstats_ringbuf_overrun_disabled(ringbuf)          \
                    == expected_overrun_disabled);                      \
    gracious_assert(jobstats_ringbuf_size(ringbuf)                      \
                    == expected_size);                                  \
    gracious_assert(jobstats_ringbuf_write_count(ringbuf)               \
                    == expected_write_count);                           \
    gracious_assert_msg(jobstats_ringbuf_oldest_pos(ringbuf)            \
                        == expected_oldest_pos,                         \
                        #ringbuf " oldest pos = %lu",                   \
                        jobstats_ringbuf_oldest_pos(ringbuf));          \
    gracious_assert(jobstats_ringbuf_lost_count(ringbuf)                \
                    == expected_lost_count)                             \
      } while (0)

  check_ring_buffer(0, 0, 0, sample_count, sample_count, 1, 0,
                    make_ring_name(nowrap, enabled));
  check_ring_buffer(0, 0, 1, sample_count, sample_count, 1, 0,
                    make_ring_name(nowrap, disabled));
  check_ring_buffer(1, 1, 0, sample_count, sample_count + 1, 2, 1,
                    make_ring_name(wrap, enabled));
  check_ring_buffer(1, 0, 1, sample_count, sample_count + 1, 1, 1,
                    make_ring_name(wrap, disabled));

  check_ring_buffer(1, 3, 0, sample_count, sample_count * 4, 1537,
                    sample_count * 3, make_ring_name(wrap_2, enabled));
  check_ring_buffer(1, 3, 0, sample_count, sample_count * 3 + 50, 1075,
                    sample_count * 2 + 50, make_ring_name(wrap_3, enabled));
  check_ring_buffer(1, 0, 1, sample_count, sample_count * 3 + 50, 1,
                    1074, make_ring_name(wrap_4, disabled));

#undef check_ring_buffer
  /* END: Check ring buffers work correctly */

  /* Serialize the oldest job position for later analysis */
  unsigned long serialized_release_position;
#define serialize_release_pos(wrap_cond, overrun_cond)                  \
  serialized_release_position                                           \
    = jobstats_ringbuf_oldest_pos(make_ring_name(wrap_cond,             \
                                                 overrun_cond));        \
  gracious_assert(fwrite(&serialized_release_position,                  \
                         sizeof(serialized_release_position),           \
                         1, make_ring_stream_name(wrap_cond,            \
                                                  overrun_cond))        \
                  == 1)

  serialize_release_pos(nowrap, enabled);
  serialize_release_pos(nowrap, disabled);
  serialize_release_pos(wrap, enabled);
  serialize_release_pos(wrap, disabled);

  serialize_release_pos(wrap_2, enabled);
  serialize_release_pos(wrap_3, enabled);
  serialize_release_pos(wrap_4, disabled);

#undef serialize_release_pos
  /* END: Serialize the oldest job position for later analysis */

  /* Save the ringbuffers into their corresponding file streams */
#define save_ringbuf(wrap_cond, overrun_cond)                           \
  gracious_assert(jobstats_ringbuf_save(make_ring_name(wrap_cond,       \
                                                       overrun_cond),   \
                                        make_ring_stream_name(wrap_cond, \
                                                              overrun_cond)) \
                  == 0)

  save_ringbuf(nowrap, enabled);
  save_ringbuf(nowrap, disabled);
  save_ringbuf(wrap, enabled);
  save_ringbuf(wrap, disabled);

  save_ringbuf(wrap_2, enabled);
  save_ringbuf(wrap_3, enabled);
  save_ringbuf(wrap_4, disabled);

#undef save_ringbuf
  /* END: Save the ringbuffers into their corresponding file streams */

  /* Rewind the file streams */
#define rewind_stream(ring_stream_name) rewind(ring_stream_name)

  rewind_stream(make_ring_stream_name(nowrap, enabled));
  rewind_stream(make_ring_stream_name(nowrap, disabled));
  rewind_stream(make_ring_stream_name(wrap, enabled));
  rewind_stream(make_ring_stream_name(wrap, disabled));

  rewind_stream(make_ring_stream_name(wrap_2, enabled));
  rewind_stream(make_ring_stream_name(wrap_3, enabled));
  rewind_stream(make_ring_stream_name(wrap_4, disabled));

#undef rewind_stream
  /* END: Rewind the file streams */

  /* Deserialize the job duration overhead */
  struct timespec deserialized_overhead;
  utility_time *t;
#define deserialize_overhead(ring_stream_name)                          \
  gracious_assert(fread(&deserialized_overhead, 1, sizeof(struct timespec), \
                        ring_stream_name) == sizeof(struct timespec));  \
  t = timespec_to_utility_time_dyn(&deserialized_overhead);             \
  gracious_assert(utility_time_eq_gc(&job_stats_overhead, t))

  deserialize_overhead(make_ring_stream_name(nowrap, enabled));
  deserialize_overhead(make_ring_stream_name(nowrap, disabled));
  deserialize_overhead(make_ring_stream_name(wrap, enabled));
  deserialize_overhead(make_ring_stream_name(wrap, disabled));

  deserialize_overhead(make_ring_stream_name(wrap_2, enabled));
  deserialize_overhead(make_ring_stream_name(wrap_3, enabled));
  deserialize_overhead(make_ring_stream_name(wrap_4, disabled));

#undef deserialize_overhead
  /* End of deserializing the job duration overhead */

  /* Deserialize the first job release position */
  unsigned long deserialized_release_position;

#define deserialize_release_pos(ring_stream_name, expected_release_pos) \
  gracious_assert(fread(&deserialized_release_position, 1,              \
                        sizeof(deserialized_release_position),          \
                        ring_stream_name)                               \
                  == sizeof(deserialized_release_position));            \
  gracious_assert(expected_release_pos == deserialized_release_position)

  deserialize_release_pos(make_ring_stream_name(nowrap, enabled), 1);
  deserialize_release_pos(make_ring_stream_name(nowrap, disabled), 1);
  deserialize_release_pos(make_ring_stream_name(wrap, enabled), 2);
  deserialize_release_pos(make_ring_stream_name(wrap, disabled), 1);

  deserialize_release_pos(make_ring_stream_name(wrap_2, enabled), 1537);
  deserialize_release_pos(make_ring_stream_name(wrap_3, enabled), 1075);
  deserialize_release_pos(make_ring_stream_name(wrap_4, disabled), 1);

#undef deserialize_release_pos
  /* End of deserializing the first job release position */

  /* Analyze the job statistics */
  char abs_t[15];
  const size_t abs_t_len = sizeof(abs_t);

  gracious_assert(to_string(&job_stats_overhead, abs_t, abs_t_len) == 0);
  log_verbose("Scaled job stats overhead = %s\n", abs_t);

  job_statistics job_stats;
  int rc;
  int late_count;
  absolute_time time_start_prev;
  utility_time_init(&time_start_prev);

#define analyze_jobstats(ring_stream_name)                              \
  rc = -2;                                                              \
  nth_job = 1;                                                          \
  late_count = 0;                                                       \
  while ((rc = job_statistics_read(ring_stream_name, &job_stats)) == 0) { \
                                                                        \
    absolute_time time_start;                                           \
    utility_time_init(&time_start);                                     \
    utility_time_to_utility_time_gc(job_statistics_time_start(&job_stats), \
                                    &time_start);                       \
                                                                        \
    absolute_time time_finish;                                          \
    utility_time_init(&time_finish);                                    \
    utility_time_to_utility_time_gc(job_statistics_time_finish(&job_stats), \
                                    &time_finish);                      \
                                                                        \
    gracious_assert(utility_time_gt(&time_finish, &time_start));        \
                                                                        \
    if (nth_job > 1) {                                                  \
      gracious_assert(utility_time_gt(&time_start, &time_start_prev));  \
    }                                                                   \
    utility_time_to_utility_time(&time_start, &time_start_prev);        \
                                                                        \
    relative_time delta;                                                \
    utility_time_init(&delta);                                          \
    utility_time_sub(&time_finish, &time_start, &delta);                \
    gracious_assert(to_string(&delta, abs_t, abs_t_len) == 0);          \
                                                                        \
    fprintf(report, "%5d%15s", nth_job, abs_t);                         \
    if (utility_time_gt(&delta, &job_duration)) {                       \
      fprintf(report, " LATE");                                         \
      log_error("Job #%d of %d is late (t_finish - t_start = %s)",      \
                nth_job, sample_count, abs_t);                          \
      late_count++;                                                     \
    }                                                                   \
    fprintf(report, "\n");                                              \
                                                                        \
    nth_job++;                                                          \
  }                                                                     \
                                                                        \
  nth_job--;                                                            \
  gracious_assert_msg(nth_job == sample_count,                          \
                      "read job count %d != sample count %d",           \
                      nth_job, sample_count);                           \
                                                                        \
  gracious_assert_msg(late_count == 0, "late count is %d out of %d",    \
                      late_count, sample_count);                        \
                                                                        \
  gracious_assert(rc != -2)

  analyze_jobstats(make_ring_stream_name(nowrap, enabled));
  analyze_jobstats(make_ring_stream_name(nowrap, disabled));
  analyze_jobstats(make_ring_stream_name(wrap, enabled));
  analyze_jobstats(make_ring_stream_name(wrap, disabled));

  analyze_jobstats(make_ring_stream_name(wrap_2, enabled));
  analyze_jobstats(make_ring_stream_name(wrap_3, enabled));
  analyze_jobstats(make_ring_stream_name(wrap_4, disabled));

#undef analyze_jobstats
  /* End of reading the job statistics */

  /* Clean-up */
  gracious_assert(utility_file_close(report, report_path) == 0);
#define destroy_ringbuf(ring_name) jobstats_ringbuf_destroy(ring_name)

  destroy_ringbuf(make_ring_name(nowrap, enabled));
  destroy_ringbuf(make_ring_name(nowrap, disabled));
  destroy_ringbuf(make_ring_name(wrap, enabled));
  destroy_ringbuf(make_ring_name(wrap, disabled));

  destroy_ringbuf(make_ring_name(wrap_2, enabled));
  destroy_ringbuf(make_ring_name(wrap_3, enabled));
  destroy_ringbuf(make_ring_name(wrap_4, disabled));

#undef destroy_ringbuf

  return EXIT_SUCCESS;

} MAIN_UNIT_TEST_END
