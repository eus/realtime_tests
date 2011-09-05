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

int job_start(jobstats_ringbuf *stats_log, struct job *job)
{
  job_statistics dummy;
  job_statistics *stats = &dummy;
  int rc = 0;

  /* Log the job statistics (this is the biggest unaccountable overhead) */
  if (stats_log != NULL) {
    if (stats_log->next == stats_log->slot_count) {
      if (!stats_log->overrun) {
        stats_log->overrun = 1;
      }
      if (!stats_log->overrun_disabled) {
        stats_log->next = 0;
        stats_log->overrun_count++;
        stats = &stats_log->ringbuf[stats_log->next++];
      }
    } else {
      stats = &stats_log->ringbuf[stats_log->next++];
    }

    stats_log->write_count++;
  }
  /* End of logging the job statistics */

  common_code_section(rc, stats_log != NULL, job,
                      &stats->t_begin, &stats->t_end);

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

jobstats_ringbuf *jobstats_ringbuf_create(unsigned long slot_count,
                                          int disable_overrun)
{
  if (slot_count == 0) {
    return NULL;
  }

  jobstats_ringbuf *res = malloc(sizeof(jobstats_ringbuf));
  if (res == NULL) {
    return NULL;
  }
  memset(res, 0, sizeof(*res));

  res->ringbuf = malloc(sizeof(*res->ringbuf) * slot_count);
  if (res->ringbuf == NULL) {
    free(res);
    return NULL;
  }
  memset(res->ringbuf, 0, sizeof(*res->ringbuf) * slot_count);

  res->slot_count = slot_count;
  res->overrun_disabled = !!disable_overrun;

  return res;
}

void jobstats_ringbuf_destroy(jobstats_ringbuf *ringbuf)
{
  free(ringbuf->ringbuf);
  free(ringbuf);
}

int jobstats_ringbuf_save(const jobstats_ringbuf *ringbuf, FILE *record_file)
{
  unsigned long i, end;

  if (ringbuf->write_count == 0) {
    return 0;
  }

  if (ringbuf->overrun_disabled || !ringbuf->overrun) {
    i = 0;
  } else {
    i = ringbuf->next;
  }

  if (!ringbuf->overrun_disabled && ringbuf->overrun
      && ringbuf->slot_count == 1) {
    end = 1;
  } else {
    end = ringbuf->next;
  }

  do {
    i %= ringbuf->slot_count;

    if (fwrite(&ringbuf->ringbuf[i], sizeof(*ringbuf->ringbuf), 1, record_file)
        == 0) {
      log_syserror("Cannot write job statistics at ring buffer slot #%lu", i);
      return -1;
    }

    i++;
  } while (i != end);

  return 0;
}

int jobstats_ringbuf_overrun(const jobstats_ringbuf *ringbuf)
{
  return (ringbuf->overrun_disabled
          ? ringbuf->overrun
          : !!ringbuf->overrun_count);
}

unsigned long jobstats_ringbuf_overrun_count(const jobstats_ringbuf *ringbuf)
{
  return ringbuf->overrun_count;
}

int jobstats_ringbuf_overrun_disabled(const jobstats_ringbuf *ringbuf)
{
  return ringbuf->overrun_disabled;
}

unsigned long jobstats_ringbuf_size(const jobstats_ringbuf *ringbuf)
{
  return ringbuf->slot_count;
}

unsigned long jobstats_ringbuf_write_count(const jobstats_ringbuf *ringbuf)
{
  return ringbuf->write_count;
}

unsigned long jobstats_ringbuf_oldest_pos(const jobstats_ringbuf *ringbuf)
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

unsigned long jobstats_ringbuf_lost_count(const jobstats_ringbuf *ringbuf)
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
