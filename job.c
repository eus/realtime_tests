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

#include "job.h"

/* CLOCK_MONOTONIC must be used because function job->run_program may
   be subject to preemption, and therefore, CLOCK_THREAD_CPUTIME_ID
   will not give the right timing information. */
#define CLOCK_TYPE CLOCK_MONOTONIC

/* The following code section must be the same in function job_start
   and in function overhead_measurement used by function
   job_statistics_overhead. */
#define common_code_section(rc, logging_enabled,        \
                            job, t_begin, t_end)        \
  if (logging_enabled)                                  \
    rc -= clock_gettime(CLOCK_TYPE, t_begin);           \
  job->run_program(job->args);                          \
  if (logging_enabled)                                  \
    rc -= clock_gettime(CLOCK_TYPE, t_end)

int job_start(FILE *stats_log, struct job *job)
{
  int rc = 0;
  job_statistics stats;

  common_code_section(rc, stats_log != NULL, job, &stats.t_begin, &stats.t_end);

  /* Log the job statistics (this is the biggest unaccountable overhead) */
  if (stats_log != NULL) {
    if (fwrite(&stats, sizeof(stats), 1, stats_log) == 0) {
      log_syserror("Cannot write job statistics");
      rc--;
    }
  }
  /* End of logging the job statistics */

  return rc;
}

int job_statistics_read(FILE *stats_log, job_statistics *stats)
{
  job_statistics result;
  size_t result_len = sizeof(result);

  switch (utility_file_read_bin(stats_log, &result, &result_len)) {
  case -2:
    if (result_len != 0) {
      log_error("Corrupted stream");
      return -2;
    }
    if (ferror(stats_log)) {
      log_error("I/O error is encountered");
      return -2;
    }
    /* It should be EOF; no break */
  case -1:
    return -1;
  }

  memcpy(stats, &result, sizeof(*stats));

  return 0;
}

static __attribute__((noinline,optimize(0)))
void empty_fn(void *args)
{
  /* Just to measure call overhead */
}
struct overhead_measurement_parameters
{
  relative_time *result;
  int which_cpu;
  int exit_status;
};
static __attribute__((noinline,optimize(0)))
int overhead_measurement(struct job *probing_job,
                         struct timespec *t_begin, struct timespec *t_end)
{
  int rc = 0;
  common_code_section(rc, 1, probing_job, t_begin, t_end);
  return rc;
}
static void *overhead_measurement_thread(void *args)
{
  struct overhead_measurement_parameters *params = args;
  params->result = NULL;
  params->exit_status = -2;

  /* Prepare the job object */
  struct job probing_job = {
    .run_program = empty_fn,
    .args = log_stream,
  };
  /* End of preparing the job object */

  /* Lock to the CPU to be measured */
  if (lock_me_to_cpu(params->which_cpu) != 0) {
    log_error("Cannot lock myself to CPU #%d", params->which_cpu);
    goto out;
  }
  /* End of locking to the CPU to be measured */

  /* Use RT scheduler without preemption with the maximum priority possible */
  switch (sched_fifo_enter_max(NULL)) {
  case 0:
    break;
  case -1:
    params->exit_status = -1;
    goto out;
  default:
    log_error("Cannot change scheduler to SCHED_FIFO with max priority");
    goto out;
  }
  /* End of becoming an RT thread */

  /* Do measurement */
  struct timespec t_begin, t_end;
  if (overhead_measurement(&probing_job, &t_begin, &t_end) != 0) {
    log_error("Fail to get either t_begin or t_end or both");
    goto out;
  }
  /* End of measurement */

  params->result
    = utility_time_sub_dyn_gc(timespec_to_utility_time_dyn(&t_end),
                              timespec_to_utility_time_dyn(&t_begin));
  params->exit_status = 0;

 out:
  return &params->exit_status;
}

int job_statistics_overhead(int which_cpu, relative_time **result)
{
  *result = NULL;

  pthread_t measurement_thread;
  struct overhead_measurement_parameters params = {
    .which_cpu = which_cpu,
  };
  if ((errno = pthread_create(&measurement_thread, NULL,
                              overhead_measurement_thread,
                              &params)) != 0) {
    log_syserror("Cannot create measurement thread");
    return -2;
  }
  if ((errno = pthread_join(measurement_thread, NULL)) != 0) {
    log_syserror("Cannot join the measurement thread");
    return -2;
  }

  if (params.exit_status == 0) {
    *result = params.result;
  }
  return params.exit_status;
}

absolute_time *job_statistics_time_start(const job_statistics *stats)
{
  return timespec_to_utility_time_dyn(&stats->t_begin);
}

absolute_time *job_statistics_time_finish(const job_statistics *stats)
{
  return timespec_to_utility_time_dyn(&stats->t_end);
}
