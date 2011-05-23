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

#include "task.h"

/* Start of timing sensitive code */

/* CLOCK_MONOTONIC must be used because function run_program may be
   subject to preemption, and therefore, CLOCK_THREAD_CPUTIME_ID
   will not give the right timing information. */
#define CLOCK_TYPE CLOCK_MONOTONIC

/*
 * Accounted overhead is release-to-start overhead:
 * (job starting time) - (release time)
 * and release-to-release overhead:
 * (next release time) - (this release time).
 *
 * No unaccounted overhead because the task is periodic.
 */
#define task_start_periodic(tau)                                        \
  do {                                                                  \
    /* Harness the sleeping period to provide starting offset as well. */ \
    rc -= clock_nanosleep(CLOCK_TYPE, TIMER_ABSTIME,                    \
                          &tau->next_release_time, NULL);               \
    /* End of harnessing the sleeping period to provide starting offset. */ \
                                                                        \
    rc -= job_start(tau->disable_job_statistics                         \
                    ? NULL : tau->stats_log, &tau->job);                \
                                                                        \
    /* Calculate the next release time */                               \
    timespec_to_utility_time(&tau->next_release_time, &tau->t);         \
    utility_time_inc(&tau->t, &tau->period);                            \
    to_timespec(&tau->t, &tau->next_release_time);                      \
    /* End of calculating the next release time */                      \
  } while (rc == 0                                                      \
           && !tau->stopped) /* To have a consistent overhead, the
                                conditions should be ordered from the
                                most rare to the most likely to
                                happen */

/*
 * Accounted overhead is release-to-start overhead:
 * (job starting time) - (release time).
 *
 * Unaccounted overhead is release-to-completion overhead:
 * (thread completion time) - (job finishing time) because the task is
 * aperiodic. Specifically, suppose the aperiodic task has the maximum
 * priority, then once the only job of the task has ended, the code
 * that executes afterwards like flushing the statistics log file will
 * still block the other threads.
 *
 * Assuming that the thread that is going to stop this thread cannot
 * be preempted by this thread, using tau->inside_aperiodic_release is
 * sufficient to guarantee that function task_stop will not kill this
 * thread when the task program is being executed.
 */
#define task_start_aperiodic(tau)                                       \
  do {                                                                  \
    tau->inside_aperiodic_release = 1;                                  \
    tau->aperiodic_release(tau->args); /* Wait for the aperiodic event */ \
    tau->inside_aperiodic_release = 0;                                  \
    rc -= job_start(tau->disable_job_statistics                         \
                    ? NULL : tau->stats_log, &tau->job);                \
  } while (rc == 0                                                      \
           && !tau->stopped); /* To have a consistent overhead, the
                                 conditions should be ordered from the
                                 most rare to the most likely to
                                 happen */

int task_start(task *tau)
{
  int rc = 0;

  tau->thread_id = pthread_self();

  if (tau->aperiodic) {
    task_start_aperiodic(tau);
  } else {
    task_start_periodic(tau);
  }

  /* Close logging file */
  if (tau->stats_log != NULL) {
    if (utility_file_close(tau->stats_log, tau->stats_log_path) != 0) {
      log_error("Cannot close task statistics log file");
      rc--;
    } else {
      tau->stats_log = NULL;
      free(tau->stats_log_path);
      tau->stats_log_path = NULL;
    }
  }  
  /* END: Close logging file */

  return rc;
}

static __attribute__((noinline,optimize(0)))
void aperiodic_release_empty(void *args)
{
}
static __attribute__((noinline,optimize(0)))
void run_program_stop_task(void *args)
{
  task *tau = (task *) args;
  tau->stopped = (tau->stop_counter-- == 0);
}
static __attribute__((noinline,optimize(0)))
int overhead_measurement(task *tau)
{
  int rc = 0;

  tau->thread_id = pthread_self();

  if (tau->aperiodic) {
    task_start_aperiodic(tau);
  } else {
    task_start_periodic(tau);
  }

  return rc == 0 ? 0 : -2;
}
struct overhead_measurement_parameters
{
  int which_cpu;
  int aperiodic;
  relative_time *finish_to_start_overhead;
  int exit_status;
};
static void *overhead_measurement_thread(void *args)
{
  struct overhead_measurement_parameters *params = args;
  params->exit_status = -2;

  /* Lock to the CPU to be measured */
  if (lock_me_to_cpu(params->which_cpu) != 0) {
    log_error("Cannot lock myself to CPU #%d", params->which_cpu);
    goto out;
  }

  /* Use RT scheduler without preemption with the maximum priority possible */
  switch (sched_fifo_enter_max(NULL)) {
  case 0:
    break;
  case -1:
    params->exit_status = -1;
    goto out;
  default: /* Anticipate further addition of exit status */
    log_error("Cannot change scheduler to SCHED_FIFO with max priority");
    goto out;
  }

  /* Prepare argument for overhead measurement */
  task tau;
  tau.stats_log = NULL;

  tau.stopped = 0;
  tau.stop_counter = 1;
  utility_time_init(&tau.t);
  tau.aperiodic = params->aperiodic;
  tau.aperiodic_release = (params->aperiodic ? aperiodic_release_empty : NULL);
  tau.args = NULL;
  tau.inside_aperiodic_release = 0;

  tau.job.run_program = run_program_stop_task;
  tau.job.args = &tau;

  tau.stats_log = tmpfile();
  if (tau.stats_log == NULL) {
    log_syserror("Cannot create temporary file to measure"
                 " finish-to-start overhead");
    goto out;
  }

  tau.disable_job_statistics = 0;

  utility_time_init(&tau.period);
  to_utility_time(0, ns, &tau.period);

  if (clock_gettime(CLOCK_TYPE, &tau.next_release_time) != 0) {
    log_syserror("Cannot get current time");
    goto out;
  }
  /** clock_nanosleep takes longer when it really has to sleep **/
  utility_time_init(&tau.offset);
  timespec_to_utility_time(&tau.next_release_time, &tau.offset);
  utility_time_add_gc(&tau.offset, to_utility_time_dyn(100, ms), &tau.offset);
  to_timespec(&tau.offset, &tau.next_release_time);
  /** END: clock_nanosleep takes longer when it really has to sleep **/
  /* END: Prepare argument for overhead measurement */

  /* Run the measurement algorithm */
  params->exit_status = overhead_measurement(&tau);
  if (params->exit_status != 0) {
    log_error("Error during overhead measurement");
    goto out;
  }
  params->exit_status = -2;

  /* Read the stats log */
  if (fseek(tau.stats_log, 0, SEEK_SET) != 0) {
    log_syserror("Cannot rewind stats log");
    goto out;
  }
  /* END: Read the stats log */

  /* Calculate the overhead */
  job_statistics job_stats_1;
  if (job_statistics_read(tau.stats_log, &job_stats_1) != 0) {
    log_error("Cannot obtain the first job statistics");
    goto out;
  }
  job_statistics job_stats_2;
  if (job_statistics_read(tau.stats_log, &job_stats_2) != 0) {
    log_error("Cannot obtain the second job statistics");
    goto out;
  }

  /** Calculate release-to-start overhead when clock_nanosleep really sleeps **/
  absolute_time *t_release = &tau.offset;
  absolute_time *t_start_1 = job_statistics_time_start(&job_stats_1);

  /** Calculate finish-to-start overhead that includes the overhead
      when clock_nanosleep does not sleep as well as the overhead of
      logging the times to the file stream and incrementing
      next_release_time **/
  absolute_time *t_finish_1 = job_statistics_time_finish(&job_stats_1);
  absolute_time *t_start_2 = job_statistics_time_start(&job_stats_2);

  params->finish_to_start_overhead
    = utility_time_add_dyn_gc(utility_time_sub_dyn_gc(t_start_1, t_release),
                              utility_time_sub_dyn_gc(t_start_2, t_finish_1));
  params->exit_status = 0;
  /* END: Calculate the overhead */

 out:
  if (tau.stats_log != NULL) {
    if (fclose(tau.stats_log) != 0) {
      log_syserror("Cannot close temporary file to measure"
                   " finish-to-start overhead");
    }
  }
  return &params->exit_status;
}
int finish_to_start_overhead(int which_cpu, int aperiodic,
                             relative_time **result)
{
  struct overhead_measurement_parameters params = {
    .which_cpu = which_cpu,
    .aperiodic = aperiodic,
  };

  pthread_t overhead_measurement_tid;
  if (pthread_create(&overhead_measurement_tid, NULL,
                     overhead_measurement_thread, &params) != 0) {
    log_syserror("Cannot create finish-to-start overhead measurement thread");
    *result = NULL;
    return -2;
  }
  if (pthread_join(overhead_measurement_tid, NULL) != 0) {
    log_syserror("Cannot join finish-to-start overhead measurement thread");
    *result = NULL;
    return -2;
  }
  if (params.exit_status != 0) {
    *result = NULL;
    return params.exit_status;
  }

  *result = params.finish_to_start_overhead;
  return 0;
}

#undef release_job
#undef task_start_aperiodic
#undef task_start_periodic
/* End of timing sensitive code */

int task_create(const char *name,
                const relative_time *wcet,
                const relative_time *period,
                const relative_time *deadline,
                const absolute_time *offset,
                void (*aperiodic_release)(void *args),
                void *aperiodic_release_args,
                const char *stats_file_path,
                const relative_time *job_statistics_overhead,
                const relative_time *finish_to_start_overhead,
                int disable_job_statistics_logging,
                void (*task_program)(void *args),
                void *args,
                task **res)
{
  int rc = -1;
  task *result = NULL;

  /* Create task_statistics object */
  task_statistics *task_stats = NULL;
  size_t task_stats_len = sizeof(*task_stats) + (strlen(name) + 1);
  task_stats = malloc(task_stats_len);
  if (task_stats == NULL) {
    log_error("Cannot create task statistics object");
    goto error;
  }
  /* END: Create task_statistics object */

  /* Initialize task stats to be serialized */
  task_stats->byte_order = host_byte_order();
  task_stats->sizeof_struct_timespec_tv_sec
    = sizeof(((struct timespec *) 0)->tv_sec);
  task_stats->sizeof_struct_timespec_tv_nsec
    = sizeof(((struct timespec *) 0)->tv_nsec);
  /* END: Initialize task stats to be serialized */

  /* Create task object */
  result = malloc(sizeof(*result));
  if (result == NULL) {
    log_error("No memory to create task object");
    goto error;
  }
  /* End of creating task object */

  /* Initialize trivial fields of task object */
  result->stopped = 0;
  utility_time_init(&result->t);
  result->inside_aperiodic_release = 0;

  result->disable_job_statistics = !!disable_job_statistics_logging;
  task_stats->job_statistics_disabled = !!disable_job_statistics_logging;
  /* END: Initialize trivial fields of task object */


  /* Create & serialize task's name */
  result->name = malloc(strlen(name) + 1);
  if (result->name == NULL) {
    log_error("No memory to create task name");
    goto error;
  }
  strcpy(result->name, name);

  task_stats->name_len = strlen(name);
  strcpy(task_stats->name, name);
  /* End of creating & serializing task's name */

  /* Create stats log's name */
  result->stats_log_path = malloc(strlen(stats_file_path) + 1);
  if (result->stats_log_path == NULL) {
    log_error("No memory to save task stats log name");
    goto error;
  }
  strcpy(result->stats_log_path, stats_file_path);
  /* End of creating stats log's name */

  /* Open stats log's stream */
  result->stats_log = utility_file_open_for_writing_bin(result->stats_log_path);
  if (result->stats_log == NULL) {
    log_error("Cannot open '%s' for binary writing", result->stats_log_path);
    rc = -2;
    goto error;
  }
  /* End of opening stats log's name */

  /* Handle all utility_time objects */
#define arg_to_task_and_task_stats(arg)                 \
  do {                                                  \
    utility_time_init(&result->arg);                    \
    utility_time_to_utility_time_gc(arg, &result->arg); \
    struct timespec t;                                  \
    to_timespec(&result->arg, &t);                       \
    task_stats->arg.tv_sec = t.tv_sec;                   \
    task_stats->arg.tv_nsec = t.tv_nsec;                 \
  } while (0)

  arg_to_task_and_task_stats(wcet);
  arg_to_task_and_task_stats(period);
  arg_to_task_and_task_stats(deadline);

  arg_to_task_and_task_stats(offset);
  to_timespec(&result->offset, &result->next_release_time);

  arg_to_task_and_task_stats(job_statistics_overhead);
  arg_to_task_and_task_stats(finish_to_start_overhead);

#undef arg_to_task_and_task_stats
  /* END: Handle all utility_time objects */

  /* Handle aperiodic mode */
  result->aperiodic_release = aperiodic_release;
  result->args = aperiodic_release_args;
  result->aperiodic = (aperiodic_release != NULL);
  task_stats->aperiodic = (aperiodic_release != NULL);
  /* END: Handle aperiodic mode */

  /* Initialize task's job */
  result->job.run_program = task_program;
  result->job.args = args;
  /* END: Initialize task's job */

  /* Serialize task statistics */
  if (result->stats_log != NULL) {
    if (fwrite(task_stats, task_stats_len - 1, 1, result->stats_log) != 1) {
      log_syserror("Cannot log task parameters");
      goto error;
    }
  }
  free(task_stats);
  /* END: Serialize task statistics */

  *res = result;
  return 0;

 error:
  *res = NULL;
  if (task_stats != NULL) {
    free(task_stats);
  }
  if (result != NULL) {
    if (result->name != NULL) {
      free(result->name);
    }
    if (result->stats_log != NULL) {
      utility_file_close(result->stats_log, result->stats_log_path);
    }
    if (result->stats_log_path != NULL) {
      free(result->stats_log_path);
    }
    free(result);
  }
  return rc;
}

void task_destroy(task *tau)
{
  free(tau->name);
  if (tau->stats_log != NULL) {
    utility_file_close(tau->stats_log, tau->stats_log_path);
  }
  if (tau->stats_log_path != NULL) {
    free(tau->stats_log_path);
  }
  free(tau);
}

void task_stop(task *tau)
{
  tau->stopped = 1;

  if (tau->aperiodic) {
    while (!tau->inside_aperiodic_release) {
      struct timespec t;
      to_timespec(&tau->wcet, &t);
      if (clock_nanosleep(CLOCK_TYPE, 0, &t, NULL) != 0) {
        log_syserror("Cannot wait the completion of task program");
        break;
      }
    }

    if ((errno = pthread_cancel(tau->thread_id)) != 0) {
      log_syserror("Cannot cancel thread %u", (unsigned) tau->thread_id);
    }
  }
}

int task_statistics_read(FILE *stats_log,
                         int (*task_statistics_fn)(task *tau, void *args),
                         void *task_statistics_fn_args,
                         int (*job_statistics_fn)(job_statistics *stats,
                                                  void *args),
                         void *job_statistics_fn_args)
{
  int exit_code = -3;
  char *task_name = NULL;

  if (feof(stats_log)) {
    exit_code = -4;
    goto out;
  }

  /* Read task statistics */
  task_statistics task_stats;
  size_t byte_read = fread(&task_stats, 1, sizeof(task_stats), stats_log);
  if (byte_read != sizeof(task_stats))
    {
      if (ferror(stats_log)) {
        log_syserror("Cannot read task stats log stream");
      } else if (byte_read == 0 && feof(stats_log)) {
        exit_code = -4;
        goto out;
      } else {
        log_error("Corrupted task stats log");
      }
      goto out;
    }
  /* END: Read task statistics */

  /* Check binary compatibility with host */
  if (task_stats.byte_order != host_byte_order()) {
    log_error("Task stats host byte order does not match host");
    goto out;
  }
  if (task_stats.sizeof_struct_timespec_tv_sec
      != sizeof((struct timespec *) 0)->tv_sec) {
    log_error("Task stats tv_sec field of struct timespec does not match host");
    goto out;
  }
  if (task_stats.sizeof_struct_timespec_tv_nsec
      != sizeof((struct timespec *) 0)->tv_nsec) {
    log_error("Task stats tv_nsec field of struct timespec"
              " does not match host");
    goto out;
  }
  /* END: Check binary compatibility with host */

  task tau;

  /* Read task name */
  task_name = malloc(task_stats.name_len + 1);
  if (task_name == NULL) {
    log_error("No memory to deserialize task name");
    goto out;
  }
  if (fread(task_name, 1, task_stats.name_len, stats_log)
      != task_stats.name_len) {
    if (ferror(stats_log)) {
      log_syserror("Cannot read task stats log stream");
    } else {
      log_error("Corrupted task stats log");
    }
    goto out;
  }
  task_name[task_stats.name_len] = '\0';
  tau.name = task_name;
  /* END: Read task name */

  /* Initialize trivial fields of task */
  tau.aperiodic = task_stats.aperiodic;
  tau.disable_job_statistics = task_stats.job_statistics_disabled;
  /* END: Initialize trivial fields of task */

  /* Populate task from task_stats */
#define task_stats_arg_to_task(arg)             \
  do {                                          \
    utility_time_init(&tau.arg);                \
    struct timespec t;                          \
    t.tv_sec = task_stats.arg.tv_sec;          \
    t.tv_nsec = task_stats.arg.tv_nsec;        \
    timespec_to_utility_time(&t, &tau.arg);     \
  } while (0)

  task_stats_arg_to_task(wcet);
  task_stats_arg_to_task(period);
  task_stats_arg_to_task(deadline);
  task_stats_arg_to_task(offset);
  task_stats_arg_to_task(job_statistics_overhead);
  task_stats_arg_to_task(finish_to_start_overhead);

#undef task_stats_arg_to_task
  /* END: Populate task from task_stats */

  /* Let task parameters be processed */
  if (task_statistics_fn(&tau, task_statistics_fn_args) != 0) {
    exit_code = -1;
    goto out;
  }
  /* END: Let task parameters be processed */

  /* Read each job recorded timings */
  if (!tau.disable_job_statistics) {
    int rc = 0;
    job_statistics job_stats;

    while ((rc = job_statistics_read(stats_log, &job_stats)) == 0) {
      if (job_statistics_fn(&job_stats, job_statistics_fn_args) != 0)
        {
          exit_code = -2;
          goto out;
        }
    }

    switch (rc) {
    case -1:
      exit_code = 0;
      goto out;
    default:
      log_error("Cannot read the next job timings");
      goto out;
    }
  }
  /* END: Read each job recorded timings */

 out:
  if (task_name != NULL) {
    free(task_name);
  }
  return exit_code;
}

const char *task_statistics_name(const task* tau)
{
  return tau->name;
}

relative_time *task_statistics_wcet(const task *tau)
{
  return utility_time_to_utility_time_dyn(&tau->wcet);
}

relative_time *task_statistics_period(const task *tau)
{
  return utility_time_to_utility_time_dyn(&tau->period);
}

relative_time *task_statistics_deadline(const task *tau)
{
  return utility_time_to_utility_time_dyn(&tau->deadline);
}

absolute_time *task_statistics_offset(const task *tau)
{
  return utility_time_to_utility_time_dyn(&tau->offset);
}

relative_time *task_statistics_job_statistics_overhead(const task *tau)
{
  return utility_time_to_utility_time_dyn(&tau->job_statistics_overhead);
}

relative_time *task_statistics_finish_to_start_overhead(const task *tau)
{
  return utility_time_to_utility_time_dyn(&tau->finish_to_start_overhead);
}

int task_statistics_aperiodic(const task *tau)
{
  return tau->aperiodic;
}

int task_statistics_job_statistics_disabled(const task *tau)
{
  return tau->disable_job_statistics;
}
