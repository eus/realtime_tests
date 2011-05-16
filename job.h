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
 * time and absolute finishing time of the job. The most crucial
 * function is job_start() whose documentation details the associated
 * timing problem.
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
  /** @} End of collection of task programs */

  /* III */
  /**
   * @name Collection of job execution functions.
   * @{
   */

  /**
   * Run the program text of the job while recording the release time
   * and the finishing time of the job in stats_log as a
   * job_statistics object.
   *
   * This function itself has an overhead that cannot be accounted in
   * the difference between the finishing time and the release
   * time. For example, if the overhead is 777 us and the recorded
   * difference between the finishing time and the release time is 100
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
   * Beside the aformentioned unaccounted overhead, the recorded
   * release time and finishing time of the job actually records the
   * timing and function call overhead (overhead of invoking
   * clock_gettime and calling the callback function run_program found
   * in the passed struct job object). The worst-case overhead can be
   * obtained by invoking function job_statistics_overhead(). This
   * means that if a job must complete within 100 ms, the program of
   * the job must complete within 100 ms - (unaccounted overhead) -
   * job_statistics_overhead(). Since job_statistics_overhead()
   * returns the worst-case, the job can actually completes faster
   * than 100 ms. Since the worst-case overhead differs from one host
   * to another, it is strongly recommended to serialize the result of
   * invoking function job_statistics_overhead() to stats_log before
   * releasing the first job so that a more accurate analysis of the
   * job statistics can be made at a later time by taking into account
   * the recorded worst-case overhead for a particular collection of
   * job statistics. An example of such an analysis can be found in
   * the unit test.
   *
   * @param stats_log a pointer to the FILE object to which the
   * statistics of each job is to be logged. Set this to NULL to
   * disable job statistics logging that can avoid a logging overhead
   * making the finishing time statistics more reliable.
   * @param job a pointer to the job to be started.
   *
   * @return zero if there is no error. Otherwise, return a negative
   * value in case of hard error (the error is not @ref utility_log.h
   * "logged" if the fix will require a change in the code of this
   * function to reduce overhead; otherwise, the error is @ref
   * utility_log.h "logged"). Even in case of hard error, the job
   * statistics are still written to stats_log.
   */
  int job_start(FILE *stats_log, struct job *job);
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
   * Measure the worst-case timing and function call overhead that is
   * included in the measured start time and finishing time of a job
   * (c.f., job_start()).
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

#ifdef __cplusplus
}
#endif

#endif /* JOB_H */
