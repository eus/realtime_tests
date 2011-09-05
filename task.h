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
 * @file task.h
 *
 * @brief Various functionalities to deal with Liu & Layland's task
 * model including to generate a stream of jobs while recording each
 * job's starting time and finishing time as accurately as possible.
 *
 * The following overheads that should be taken into account when
 * deciding the WCET of a task program exist:
 * <ol>
 * <li>Job statistics overhead. This overhead is included in the
 * duration delimited by the recorded starting and finishing time of
 * each job. Function job_statistics_overhead returns the approximated
 * duration of this overhead.</li>
 * <li>Finish-to-start overhead. This overhead is included in the
 * duration delimited by the recorded finishing time of the previous
 * job and the recorded starting time of the current job. Function
 * finish_to_start_overhead returns the approximated duration of this
 * overhead.</li>
 * <li>Release-to-start overhead. In the absent of preemption, this
 * overhead can be determined for each job by the recorded starting
 * time. For example, if it is known that the release time is exactly
 * at time t_release, for sure the recorded start time t_start will be
 * slightly later due to release-to-start overhead, and therefore, the
 * overhead duration is t_start - t_release.</li>
 * <li>Finish-to-release overhead. In the absent of preemption, this
 * overhead can be determined using finish-to-start-overhead minus
 * release-to-start overhead.</li>
 * </ol>
 *
 * To schedule a task according to a particular scheduling algorithm,
 * create a thread that will invoke function task_start() and schedule
 * the thread according to the scheduling algorithm.
 *
 * @author Tadeus Prastowo <eus@member.fsf.org>
 */

#ifndef TASK_H
#define TASK_H

#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include "utility_time.h"
#include "utility_cpu.h"
#include "utility_file.h"
#include "job.h"

#ifdef __cplusplus
extern "C" {
#endif

  /* I. Main data structures */
  /**
   * The Liu & Layland's real-time task model.
   * This is an opaque type; do not manipulate any of its instances directly.
   */
  typedef struct
  {
    /*
     * @name Collection of theoretical information
     * @{
     */
    char *name; /* The name of this task. */
    relative_time wcet; /* The worst-case execution time of this task
                           relative to a release time. */
    relative_time period; /* The periodicity of this task relative to
                             a release time. */
    relative_time deadline; /* The deadline of this task relative to
                               a release time. */
    absolute_time t_0; /* Absolute time at which the task is spawned. */
    relative_time offset; /* The delay from t_0 after which the task
                             releases its first job. */
    /* @} End of collection of theoretical information */

    pthread_t thread_id; /* The thread ID of the thread that invokes
                            function task_start*/
    int stopped; /* Non-zero means that the task should not release
                    the next job. */
    int stop_counter; /* Only used by function
                         finish_to_start_overhead to measure the
                         overhead. */
    struct timespec next_release_time; /* Absolute time for the next
                                          job release. */
    absolute_time t; /* Provide an initialized scratch paper to
                        calculate the next release time. */

    int aperiodic; /* This is non-zero iff aperiodic_release is not NULL. */
    void (*aperiodic_release)(void *args); /* If this is set
                                              (non-NULL), the job will
                                              only be released
                                              everytime this function
                                              returns. Otherwise, this
                                              task will run
                                              periodically. */
    void *args; /* Arguments to be passed to aperiodic_release
                   callback function. */
    int inside_aperiodic_release; /* Non-zero means that the task is
                                     still inside function
                                     aperiodic_release. */

    struct job job; /* The job of this task. */

    FILE *stats_log; /* The file to which the statistics of this
                        task is to be logged. */
    unsigned fail_to_close_stats_log; /* Non-zero if stats_log cannot
                                         be closed. */
    char *stats_log_path; /* NULL-terminated file name of stats_log. */
    int disable_job_statistics; /* Non-zero will disable the recording
                                   of job starting and finishing times*/
    jobstats_ringbuf *stats_ringbuf; /* The ring buffer to temporary
                                        store all job statistics. */
    unsigned long oldest_job_pos; /* The release position of the oldest
                                     job in the ring buffer. This is zero
                                     if either the job statistics logging
                                     is disabled or the ring buffer has
                                     not been saved into the log file. */
    unsigned long lost_job_count; /* The number of jobs lost due to
                                     ring buffer overrun. */
    unsigned long write_count; /* Total number of job statistics data. */
    relative_time finish_to_start_overhead; /* finish-to-start overhead. */
    relative_time job_statistics_overhead; /* Overhead included in sampled
                                              start and finishing time
                                              of a job. This is
                                              not included in
                                              finish_to_start_overhead. */
  } task;

  /**
   * The statistics of a real-time task.
   * This is an opaque type; do not manipulate any of its instances directly.
   */
  typedef struct __attribute__((packed))
  {
    /*
     * @name I/O-related fields.
     * @{
     */
    uint8_t byte_order; /* The byte order of the serialization. */
    uint8_t sizeof_struct_timespec_tv_sec; /* Size of tv_sec field of
                                              struct timespec. */
    uint8_t sizeof_struct_timespec_tv_nsec; /* Size of tv_nsec field
                                               of struct timespec. */
    uint8_t sizeof_unsigned_long; /* Size of unsigned long. */
    /* @} End of I/O-related fields */

    uint8_t aperiodic; /* Non-zero if this task is aperiodic. */
    uint8_t job_statistics_disabled; /* Non-zero if starting and
                                        finishing time of each job is
                                        not logged. */

    struct {
      uint32_t tv_sec;
      uint32_t tv_nsec;
    } wcet;
    struct {
      uint32_t tv_sec;
      uint32_t tv_nsec;
    } period;
    struct {
      uint32_t tv_sec;
      uint32_t tv_nsec;
    } deadline;
    struct {
      uint32_t tv_sec;
      uint32_t tv_nsec;
    } t_0;
    struct {
      uint32_t tv_sec;
      uint32_t tv_nsec;
    } offset;
    struct {
      uint32_t tv_sec;
      uint32_t tv_nsec;
    } job_statistics_overhead;
    struct {
      uint32_t tv_sec;
      uint32_t tv_nsec;
    } finish_to_start_overhead;
    uint32_t name_len; /**< The length in bytes of the name of this task. */
    char name[0]; /**< The name of this task. */
  } task_statistics;

  /**
   * The ring buffer states of the statistics of a real-time task.
   * This is an opaque type; do not manipulate any of its instances directly.
   */
  typedef struct __attribute__((packed))
  {
    unsigned long oldest_job_pos; /**< The release position of the
                                     oldest job. */
    unsigned long lost_job_count; /**< The number of job lost due to overrun. */
    unsigned long write_count; /**< Total number of job statistics data. */
  } task_statistics_ringbuf;
  /* End of main data structures */

  /* II */
  /**
   * @name Collection of task maintenance functions.
   * @{
   */

  /**
   * Create a task object. Setting ringbuffer_size to zero will
   * disable the sampling and logging of starting time and finishing
   * time of a job reducing the finish-to-start overhead.
   *
   * All utility_time objects whose addresses are passed as the
   * arguments are garbage collected automatically if it is possible.
   *
   * @param name a pointer to a NULL-terminated string object
   * containing the name of the task. Any subsequent modification to
   * the string object will not affect the internal states of the
   * created task object.
   * @param wcet a pointer to the utility_time object specifying the
   * worst-case execution time of each job of the task that includes
   * the appropriate overhead as obtained using
   * finish_to_start_overhead() (i.e., the caller knows that the task
   * program must complete within wcet minus job-statistics overhead
   * minus finish-to-start overhead. The utility_time object is
   * garbage collected automatically if it is possible.
   * @param period a pointer to the utility_time object specifying the
   * period of the task. The utility_time object is garbage collected
   * automatically if it is possible.
   * @param deadline a pointer to the utility_time object specifying
   * the relative deadline of the task. The utility_time object is
   * garbage collected automatically if it is possible.
   * @param t_0 a pointer to the utility_time object specifying the
   * absolute spawning time of the first job of the task. The
   * utility_time object is garbage collected automatically if it is
   * possible.
   * @param offset a pointer to the utility_time object specifying the
   * delay from t_0 after which the first job of the task is
   * released. The utility_time object is garbage collected
   * automatically if it is possible.
   * @param aperiodic_release if this is set, then the task will
   * release one job whenever function aperiodic_release
   * returns. Since function aperiodic_release can block waiting for
   * an event, the function must be ready to be killed at any time
   * when the task set is shut down. The task itself will not be
   * killed when executing its task program because the task program
   * has a definite WCET and so it is reasonable to wait for the
   * completion. Whenever this function returns, the task program will
   * always be executed once. If this is not set, then the task will
   * release a stream of jobs.
   * @param aperiodic_release_args a pointer to the arguments to be
   * passed to function aperiodic_release.
   * @param stats_file_path a pointer to a NULL-terminated string
   * object containing a file path to which the task statistics is to
   * be saved from the ring buffer when the task is stopped using task_stop().
   * @param ringbuffer_size the number of job statistics that the ring
   * buffer will be able to store without overrunning. Setting this to
   * zero will disable the sampling and logging of job start times and
   * finishing times reducing the finish-to-start overhead.
   * @param ringbuffer_disable_overrun if non-zero, once the ring
   * buffer is full, additional job statistics are not
   * saved. Otherwise, the ring buffer will overwrite the saved job
   * statistics starting from the oldest one.
   * @param job_statistics_overhead the result of running
   * job_statistics_overhead(). The utility_time object is garbage
   * collected automatically if it is possible. This overhead is
   * not included in the finish-to-start overhead.
   * @param finish_to_start_overhead the result of running
   * finish_to_start_overhead() with the right arguments (i.e., if the
   * created task is aperiodic, then finish_to_start_overhead() should
   * be requested to return the worst-case overhead of an aperiodic
   * task). The utility_time object is garbage collected
   * automatically if it is possible.
   * @param task_program a pointer to the task's program that will be
   * executed.
   * @param args a pointer to the object that will be passed to the
   * task's program.
   * @param result a pointer to a location to store the address of the
   * resulting task object. The location is set to NULL if the return
   * value is not zero.
   *
   * @return zero if the initialization is successful, -1 in case of
   * hard error that requires the investigation of the output of the
   * logging facility to fix the error, or -2 if stats_file_path
   * cannot be opened successfully for writing.
   */
  int task_create(const char *name,
                  const relative_time *wcet,
                  const relative_time *period,
                  const relative_time *deadline,
                  const absolute_time *t_0,
                  const relative_time *offset,
                  void (*aperiodic_release)(void *args),
                  void *aperiodic_release_args,
                  const char *stats_file_path,
                  unsigned long ringbuffer_size,
                  int ringbuffer_disable_overrun,
                  const relative_time *job_statistics_overhead,
                  const relative_time *finish_to_start_overhead,
                  void (*task_program)(void *args),
                  void *args,
                  task **result);

  /**
   * Destroy a task object. An already started but not stopped task
   * and an already destroyed task object must not be passed to this
   * function.
   */
  void task_destroy(task *tau);
  /** @} End of collection of task maintenance functions. */

  /* III */
  /**
   * @name Collection of task execution functions.
   * @{
   */

  /**
   * Release a stream of jobs while sampling and logging the job
   * starting and finishing times if logging is not disabled. Even in
   * the absence of preemption, the sampled starting time of a job is
   * expected to be slightly later than the calculated one due to
   * function call and timing overheads that collectively are referred
   * as release-to-start overhead.
   *
   * The sampled starting time and finishing time of each job contains
   * an overhead whose approximation can be obtained using
   * job_statistics_overhead().
   *
   * The difference between the previous job finishing time and the
   * current job starting time also includes finish-to-start overhead
   * whose approximation can be obtained using
   * finish_to_start_overhead().
   *
   * In a periodic task, job_statistics_overhead() and
   * finish_to_start_overhead() reduces the WCET of the task
   * program. For example, if the task period is 50 ms and function
   * job_statistics_overhead returns 2 us while function
   * finish_to_start_overhead returns 100 us, then the WCET of the
   * task program should be approximately (50 - 0.002 - 0.100) ms.
   *
   * Similarly in an aperiodic task, job_statistics_overhead() and
   * finish_to_start_overhead() reduces the WCET of the task
   * program. For example, if the task minimum interarrival time is 50
   * ms and function job_statistics_overhead returns 2 us while
   * function finish_to_start_overhead returns 100 us, then the WCET
   * of the task program should be approximately (50 - 0.002 - 0.100)
   * ms. Remember that each time function aperiodic_release returns,
   * the task program will always be run once.
   *
   * @param tau a pointer to the task to be started.
   *
   * @return zero if there is no error. Otherwise, return a negative
   * value in case of hard error (the error is not @ref utility_log.h
   * "logged" if the fix will require a change in the code of this
   * function to keep the overhead low; otherwise, the error is @ref
   * utility_log.h "logged"). Even in case of hard error, the task
   * statistics is still written to the file passed to task_create().
   */
  int task_start(task *tau);

  /**
   * Stop the release of a stream of jobs by not releasing the next
   * job and flush the ring buffer into the log file. This must be
   * called by another thread that cannot be preempted by the thread
   * that is going to be stopped or by the thread that is going to
   * stop itself.
   *
   * @param tau a pointer to the task to be stopped.
   */
  void task_stop(task *tau);
  /** @} End of collection of task execution functions */

  /* IV */
  /**
   * @name Collection of task statistics functions.
   * @{
   */

  /**
   * Deserialize (read) the task_statistics object from the given
   * file. The callback task_statistics_fn is first called to process
   * the parameters of the task. Afterwards, for each job, the
   * callback job_statistics_fn is called.
   *
   * Both callback functions must not free any of its arguments except
   * for task_statistics_fn_args and job_statistics_fn_args.
   *
   * If job statistics logging was disabled during task creation, the
   * callback function job_statistics_fn will not be called.
   *
   * @param stats_log a pointer to the FILE object containing a
   * serialized task statistics object.
   * @param task_statistics_fn the callback function to process the
   * task parameters. The callback can stop the deserializing process
   * by returning a non-zero value.
   * @param task_statistics_fn_args the argument to be passed to the
   * callback function task_statistics_fn.
   * @param job_statistics_fn the callback function to process each
   * task's job. The callback can stop the deserializing process by
   * returning a non-zero value.
   * @param job_statistics_fn_args the argument to be passed to the
   * callback function job_statistics_fn.
   *
   * @return zero if the task_statistics object can be deserialized
   * successfully, -1 if task_statistics_fn returns a non-zero value,
   * -2 if job_statistics_fn returns a non-zero value, -3 if there is
   * an I/O error while deserializing (the error itself is @ref
   * utility_log.h "logged" directly), or -4 if end of file has been
   * reached (no callback function is called).
   */
  int task_statistics_read(FILE *stats_log,
                           int (*task_statistics_fn)(task *tau, void *args),
                           void *task_statistics_fn_args,
                           int (*job_statistics_fn)(job_statistics *stats,
                                                    void *args),
                           void *job_statistics_fn_args);

  /**
   * @return the name of the task.
   */
  const char *task_statistics_name(const task *tau);

  /**
   * @return the worst-case execution time of this task relative to
   * any release time as a utility_time object fits for automatic
   * garbage collection.
   */
  relative_time *task_statistics_wcet(const task *tau);

  /**
   * @return the period of this task relative to any release time as a
   * utility_time object fits for automatic garbage collection.
   */
  relative_time *task_statistics_period(const task *tau);

  /**
   * @return the deadline of this task relative to any release time as
   * a utility_time object fits for automatic garbage collection.
   */
  relative_time *task_statistics_deadline(const task *tau);

  /**
   * @return the spawning time of this task as a utility_time object
   * fits for automatic garbage collection.
   */
  absolute_time *task_statistics_t0(const task *tau);

  /**
   * @return the offset of this task with respect to t0 as a
   * utility_time object fits for automatic garbage collection.
   */
  relative_time *task_statistics_offset(const task *tau);

  /**
   * @return one if the task is an aperiodic task. Otherwise, return zero.
   */
  int task_statistics_aperiodic(const task *tau);

  /**
   * @return one if the task does not sample and log starting and
   * finishing time of each job. Otherwise, return zero.
   */
  int task_statistics_job_statistics_disabled(const task *tau);

  /**
   * @return the release position of the oldest job statistics
   * recorded in the task statistics starting from one. So, if all job
   * statistics are recorded, the return value will be one. If job
   * statistics recording is disabled or the ring buffer has not been
   * flushed into the log file, zero is returned.
   */
  unsigned long task_statistics_oldest_job_pos(const task *tau);

  /**
   * @return the number of jobs that are lost due to ring buffer overrun.
   */
  unsigned long task_statistics_lost_job_count(const task *tau);

  /**
   * @return the total number of job statistics that the ring buffer
   * should have if suppose no overrun had occured.
   */
  unsigned long task_statistics_write_count(const task *tau);

  /**
   * @return the approximated duration of the overhead that is
   * included in the difference between the sampled job start time and
   * finishing time.
   */
  relative_time *task_statistics_job_statistics_overhead(const task *tau);

  /**
   * @return the approximated duration of the overhead that is
   * included in the difference between the finishing time of the
   * previous job and the starting time of the current job in case of
   * a periodic task, or the approximated duration of the overhead
   * that serves as the most minimal interarrival time possible in
   * case of an aperiodic task.
   */
  relative_time *task_statistics_finish_to_start_overhead(const task *tau);

  /**
   * Measure the approximated timing and function call overheads that
   * are included in the difference between the finishing time of a
   * previous job and the starting time of a current job in case of
   * periodic task, or the approximated most minimal interarrival time
   * possible in case of aperiodic task (cf., task_start()).
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
   * task is to be run.
   * @param aperiodic a non-zero value will calculate the worst-case
   * overhead that serves as the most minimal interarrival time
   * possible.
   * @param result a pointer to the location to store the address of
   * the utility_time object containing the desired worst-case
   * overhead of a task. The pointed location is set to NULL if the
   * return value is not zero.
   *
   * @return zero if there is no error and the resulting utility_time
   * objects can be successfully created, -1 if the caller is not
   * privileged to use the real-time scheduler, or -2 in case of hard
   * error that requires the investigation of the output of the
   * logging facility to fix the error.
   */
  int finish_to_start_overhead(int which_cpu, int aperiodic,
                               relative_time **result);
  /** @} End of collection of task statistics functions */

#ifdef __cplusplus
}
#endif

#endif /* TASK_H */
