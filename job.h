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

/**
 * @file job.h
 * @brief Various functionalities to start the next job of a real-time
 * task while as accurately as possible recording the absolute start
 * time and absolute finishing time of the job.
 *
 * The most crucial function is job_start() whose documentation
 * details the associated timing problem. To avoid costly timing
 * overhead associated with saving job statistics into a disk, the job
 * statistics are written into a ring buffer that can overrun. The
 * content of the ring buffer can then be stored in a file after the
 * program completes and no temporal guarantee is needed
 * anymore. Since the ring buffer can overrun, depending on the user
 * requirement, the user might want to serialize the release position
 * of the oldest job in the ring buffer into the file storing the
 * content of the ring buffer to aid later analysis (e.g., to
 * determine the expected release time of the oldest job). The test
 * unit provides complete usage example.
 *
 * @author Tadeus Prastowo <eus@member.fsf.org>
 */

#ifndef JOB_H
#define JOB_H

#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "utility_log.h"
#include "utility_time.h"
#include "utility_file.h"
#include "utility_sched_fifo.h"
#include "utility_cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

  /* I. Main data structures */
  /**
   * The job of a real-time task.
   */
  struct job
  {
    void (*run_program)(void *args); /**< Pointer to the job program. */
    void *args; /**< Pointer to the data passed as the argument to
                   run_program. */
  };

  /**
   * The statistics of a particular job of a real-time task. This is
   * an opaque type; do not manipulate any of its instances directly.
   */
  typedef struct __attribute__((packed))
  {
    struct timespec t_begin; /* The start time (s_{i,j}) of the job */
    struct timespec t_end; /* The finishing time (f_{i,j}) of the job */
  } job_statistics;

  /**
   * Following the idea of Linux ftrace, job statistics are logged to
   * ring buffer to avoid expensive disk writing cost. The user of
   * this infrastructure component is then responsible for allocating
   * large enough ring buffer if all statistics are to be recorded.
   * This is an opaque type; do not manipulate any of its instances directly.
   */
  typedef struct
  {
    job_statistics *ringbuf; /* The ring buffer as an array */
    unsigned long slot_count; /* The number of slots in the ring
                                 buffer array */
    unsigned long next; /* The next slot in the ring buffer to write to */
    unsigned long overrun_count; /* The number of times the first
                                    slot in ring buffer has been
                                    overwritten */
    int overrun_disabled; /* Non-zero if the ring must not wrap around. */
    int overrun; /* Non-zero if overrun has happened. */
    unsigned long write_count; /* The number of times fn job_start has
                                  tried to save data into this ring
                                  buffer. */
  } jobstats_ringbuf;
  /* End of main data structures */

  /* II */
  /**
   * @name Collection of job programs.
   * @{
   */

  /**
   * The holder of arguments to busyloop_exact().
   */
  struct busyloop_exact_args
  {
    cpu_busyloop *busyloop_obj; /**< Pointer to the object needed to
                                  keep a CPU busy for a certain
                                  duration */
  };
  /**
   * Run a busy loop program for the specified duration of time.
   *
   * @param args a pointer to the object of type ::busyloop_exact_args
   * containing the desired duration of the busy loop.
   */
  static inline void busyloop_exact(void *args)
  {
    struct busyloop_exact_args *params = args;
    keep_cpu_busy(params->busyloop_obj);
  }
  /** @} End of collection of job programs */

  /* III */
  /**
   * @name Collection of job execution functions.
   * @{
   */

  /**
   * Run the program text of the job while recording the starting time
   * and the finishing time of the job in stats_log as a
   * job_statistics object.
   *
   * This function itself has an overhead that cannot be accounted in
   * the difference between the finishing time and the starting
   * time. For example, if the overhead is 777 us and the recorded
   * difference between the finishing time and the starting time is 100
   * ms, then this function would actually have run for 100.777
   * ms. The 777 us overhead can be calculated by invoking this
   * function like:
   * @code
   * clock_gettime(CLOCK_MONOTONIC, &t_begin);
   * job_start(log_stream, my_job);
   * clock_gettime(CLOCK_MONOTONIC, &t_end);
   * @endcode
   * If my_job runs for 100 ms, then 777 us is obtained by
   * (t_end - t_begin) - (overhead of invoking clock_gettime) - 100.
   *
   * Beside the aforementioned unaccounted overhead, the recorded
   * starting time and finishing time of the job actually records the
   * timing and function call overhead (overhead of invoking
   * clock_gettime and calling the callback function run_program found
   * in the passed struct job object). The approximated overhead can
   * be obtained by invoking function job_statistics_overhead(). This
   * means that if a job must complete within 100 ms, the program of
   * the job must complete within approximately 100 ms - (unaccounted
   * overhead) - job_statistics_overhead(). Since the approximated
   * overhead differs from one host to another, it is strongly
   * recommended to note down the result of invoking function
   * job_statistics_overhead() before releasing the first job so that
   * a more accurate analysis of the job statistics can be made at a
   * later time by taking into account the noted approximated overhead
   * for a particular collection of job statistics. An example of such
   * an analysis can be found in the unit test.
   *
   * @param stats_log a pointer to the jobstats_ringbuf object to which the
   * statistics of each job is to be logged. Set this to NULL to
   * disable job statistics logging that can reduce the amount of
   * unaccountable overhead.
   * @param job a pointer to the job to be started.
   *
   * @return zero if there is no error. Otherwise, return a negative
   * value in case of hard error (the error is not @ref utility_log.h
   * "logged" if the fix will require a change in the code of this
   * function to keep the overhead low; otherwise, the error is @ref
   * utility_log.h "logged"). Even in case of hard error, the job
   * statistics are still written to stats_log.
   */
  int job_start(jobstats_ringbuf *stats_log, struct job *job);
  /** @} End of collection of job execution functions */

  /* IV */
  /**
   * @name Collection of job statistics functions.
   * @{
   */

  /**
   * Deserialize (read) the next job_statistics object from a file
   * containing a collection of serialized job_statistics objects.
   *
   * @param stats_log a pointer to the FILE object containing a
   * collection of job statistics.
   * @param stats a pointer to a job_statistics object to store
   * the result of deserialization.
   *
   * @return zero if the next job_statistics object can be
   * deserialized successfully, -1 if there is no more job_statistics
   * object to be deserialized, or -2 if there is an I/O error while
   * deserializing (the error itself is @ref utility_log.h "logged"
   * directly). If the return value is not zero, stats is not touched.
   */
  int job_statistics_read(FILE *stats_log, job_statistics *stats);

  /**
   * Measure the approximate timing and function call overhead that is
   * included within the recorded start time and finishing time of a
   * job (c.f., job_start()).
   *
   * It is highly recommended to fix the frequency of the CPU at which
   * the overhead is to be measured to a fix value using
   * cpu_freq_set(). Otherwise, the measurement result will not be
   * reliable. The measurement will be carried out with an RT thread
   * having the maximum priority. This is justifiable because
   * otherwise, the measurement may include the preemption times from
   * other threads.
   *
   * @param which_cpu the ID of the CPU at which the measurement
   * should be carried out. This should match the CPU at which the
   * collection of jobs is to be released.
   * @param result a pointer to the location to store the address of
   * the utility_time object containing the measurement result. The
   * pointed location is set to NULL if the return value is not zero.
   *
   * @return zero if there is no error and the resulting utility_time
   * object can be successfully created, -1 if the caller is not
   * privileged to use the real-time scheduler, or -2 in case of hard
   * error that requires the investigation of the output of the
   * logging facility to fix the error.
   */
  int job_statistics_overhead(int which_cpu, relative_time **result);

  /**
   * @return the starting time of this particular job as a
   * utility_time object fits for automatic garbage collection.
   */
  absolute_time *job_statistics_time_start(const job_statistics *stats);

  /**
   * @return the finishing time of this particular job as a
   * utility_time object fits for automatic garbage collection.
   */
  absolute_time *job_statistics_time_finish(const job_statistics *stats);
  /** @} End of collection of job statistics functions */

  /* V */
  /**
   * @name Collection of job statistics ring buffer functions.
   * @{
   */

  /**
   * Create a ring buffer to store the recorded job statistics.
   *
   * @param slot_count is the number of job statistics that the buffer
   * should be able to store before wrapping around. This must be at least one.
   * @param disable_overrun if non-zero, once the ring buffer is full,
   * additional job statistics is not recorded. Otherwise, the ring
   * buffer will wrap around and the data will be overwritten starting
   * from the oldest one.
   *
   * @return the ring buffer object or NULL if there is not enough
   * memory or slot_count is zero.
   */
  jobstats_ringbuf *jobstats_ringbuf_create(unsigned long slot_count,
                                            int disable_overrun);

  /**
   * Destroy a job statistics ring buffer object. An already destroyed
   * ring buffer must not be passed to this function again.
   *
   * @param ringbuf the ring buffer to be destroyed.
   */
  void jobstats_ringbuf_destroy(jobstats_ringbuf *ringbuf);

  /**
   * Save the content of a job statistics ring buffer to a file.
   *
   * @param ringbuf a pointer to the ring buffer object whose contents
   * are to be saved.
   * @param record_file a binary file stream to which the content of
   * the ring buffer will be saved.
   *
   * @return 0 if there is no error or -1 if there is an I/O error to
   * open the file for reading (the error is @ref utility_log.h
   * "logged" directly).
   */
  int jobstats_ringbuf_save(const jobstats_ringbuf *ringbuf, FILE *record_file);

  /**
   * Test if a job statistics ring buffer object has overrun.
   *
   * @param ringbuf a pointer to the ring buffer object.
   *
   * @return non-zero if the ring buffer has overrun, zero
   * otherwise. If the ring buffer is not allowed to overrun, the test
   * will return non-zero if additional data have been lost due to
   * overrun.
   */
  int jobstats_ringbuf_overrun(const jobstats_ringbuf *ringbuf);

  /**
   * Return the number of times the first slot in ring buffer has been
   * overwritten.
   *
   * @param ringbuf a pointer to the ring buffer object.
   *
   * @return the overrun count.
   */
  unsigned long jobstats_ringbuf_overrun_count(const jobstats_ringbuf *ringbuf);

  /**
   * Test if a job statistics ring buffer object is not allowed to overrun.
   *
   * @param ringbuf a pointer to the ring buffer object.
   *
   * @return non-zero if the ring buffer is not allowed to overrun, zero
   * otherwise.
   */
  int jobstats_ringbuf_overrun_disabled(const jobstats_ringbuf *ringbuf);

  /**
   * Return the number of slots available in the ring buffer.
   *
   * @param ringbuf a pointer to the ring buffer object.
   *
   * @return the number of slots in the ring buffer.
   */
  unsigned long jobstats_ringbuf_size(const jobstats_ringbuf *ringbuf);

  /**
   * Return the number of job statistics data that have been written
   * into the buffer. If overrun is not disabled and overrun happens,
   * subtracting the number of slots in the ring buffer from this
   * function return value and adding one will yield the release
   * position of the oldest job in the ring buffer. If overrun is
   * disabled and overrun happens, doing the same arithmetics but
   * without adding one will yield the number of job statistics data
   * that are lost. If either of the above two is the thing that you
   * need, you can use convenient function
   * jobstats_ringbuf_oldest_pos() or jobstats_ringbuf_lost_count().
   *
   * @param ringbuf a pointer to the ring buffer object to be processed.
   *
   * @return zero if the ring buffer is empty or positive integer
   * indicating the count of job statistics that have been tried to be
   * saved into the ring buffer.
   */
  unsigned long jobstats_ringbuf_write_count(const jobstats_ringbuf *ringbuf);

  /**
   * Return the release position starting from 1 of the oldest job in
   * the ring buffer. So, a return value of 1 means that the oldest
   * job statistics stored in the ring buffer belongs to the first
   * released job. Zero is returned if the ring buffer is empty.
   *
   * @param ringbuf a pointer to the ring buffer object to be processed.
   *
   * @return the release position of the oldest job.
   */
  extern inline unsigned long
  jobstats_ringbuf_oldest_pos(const jobstats_ringbuf *ringbuf)
  {
    if (jobstats_ringbuf_write_count(ringbuf) == 0) {
      return 0;
    }

    if (jobstats_ringbuf_overrun_disabled(ringbuf)
        || !jobstats_ringbuf_overrun(ringbuf)) {
      return 1;
    } else {
      return (jobstats_ringbuf_write_count(ringbuf)
              - jobstats_ringbuf_size(ringbuf) + 1);
    }
  }

  /**
   * If overrun is not disabled, return the number of jobs that has
   * been overwritten. Otherwise, return the number of jobs that
   * cannot be saved into the ring buffer.
   *
   * @param ringbuf a pointer to the ring buffer object to be processed.
   *
   * @return the number of job statistics data that are lost.
   */
  extern inline unsigned long
  jobstats_ringbuf_lost_count(const jobstats_ringbuf *ringbuf)
  {
    if (!jobstats_ringbuf_overrun(ringbuf)) {
      return 0;
    } else if (jobstats_ringbuf_overrun_disabled(ringbuf)) {
      return (jobstats_ringbuf_write_count(ringbuf)
              - jobstats_ringbuf_size(ringbuf));
    } else {
      return (jobstats_ringbuf_oldest_pos(ringbuf) - 1);
    }
  }
  /** @} End of collection of job statistics ring buffer functions */

#ifdef __cplusplus
}
#endif

#endif /* JOB_H */
