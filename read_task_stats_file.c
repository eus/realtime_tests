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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <strings.h>
#include "utility_log.h"
#include "utility_file.h"
#include "utility_time.h"
#include "task.h"

struct exec_time_list
{
  struct exec_time_list *next;
  unsigned job_count;
  unsigned long long exec_time;
};

static void free_exec_time_list(struct exec_time_list *smallest_exec_time)
{
  while (smallest_exec_time != NULL) {
    struct exec_time_list *n = smallest_exec_time;
    smallest_exec_time = n->next;
    free(n);
  }
}

static struct exec_time_list *new_exec_time_list(unsigned long long exec_time)
{
  struct exec_time_list *res = malloc(sizeof(*res));
  if (res == NULL) {
    log_error("Insufficient memory to create exec time list item");
    return NULL;
  }

  res->next = NULL;
  res->job_count = 1;
  res->exec_time = exec_time;

  return res;
}

static struct exec_time_list *
insert_exec_time(unsigned long long exec_time,
                 struct exec_time_list *smallest_exec_time)
{
  /* Adjust exec_time unit to microsecond from nanosecond */
  exec_time = (exec_time % 1000 >= 500
               ? exec_time / 1000 + 1
               : exec_time / 1000);
  /* END: Adjust exec_time unit to microsecond from nanosecond */

  struct exec_time_list *itr_prev = NULL;
  struct exec_time_list *itr = smallest_exec_time;

  while (itr != NULL) {

    if (itr->exec_time == exec_time) {
      /* The item already exists; just increment job_count and exit */
      itr->job_count++;
      return smallest_exec_time;
    } else if (itr->exec_time > exec_time) {
      /* Found an insertion point */
      break;
    }

    itr_prev = itr;
    itr = itr->next;
  }

  /* +----------+----------+------------------------------+
   * | itr_prev | itr      | exec_time                    |
   * +----------+----------+------------------------------+
   * | NULL     | NULL     | smallest                     |
   * | NULL     | not NULL | smallest                     |
   * | not NULL | NULL     | largest                      |
   * | not NULL | not NULL | neither smallest nor largest |
   * +----------+----------+------------------------------+
   */
  if (itr_prev == NULL && itr == NULL) {

    smallest_exec_time = new_exec_time_list(exec_time);

  } else if (itr_prev == NULL && itr != NULL) {

    smallest_exec_time = new_exec_time_list(exec_time);
    if (smallest_exec_time == NULL) {
      return itr;
    }

    smallest_exec_time->next = itr;

  } else if (itr_prev != NULL && itr == NULL) {

    struct exec_time_list *largest_exec_time = new_exec_time_list(exec_time);
    itr_prev->next = largest_exec_time;

  } else {

    struct exec_time_list *n = new_exec_time_list(exec_time);
    if (n == NULL) {
      return smallest_exec_time;
    }

    n->next = itr;
    itr_prev->next = n;
  }

  return smallest_exec_time;
}

static void print_gnuplot_cdf(struct exec_time_list *smallest_exec_time,
                              unsigned total_job_count, FILE *report)
{
  unsigned cummulative_job_count = 0;

  struct exec_time_list *itr = smallest_exec_time;
  while (itr != NULL) {
    cummulative_job_count += itr->job_count;

    fprintf(report, "%llu\t%.06f\n",
            itr->exec_time, cummulative_job_count / (double) total_job_count);

    itr = itr->next;
  }
}

static void print_matlab_cdf(struct exec_time_list *smallest_exec_time,
                             unsigned total_job_count, FILE *report)
{
  fprintf(report, "plot([");
  struct exec_time_list *itr = smallest_exec_time;
  while (itr != NULL) {
    fprintf(report, " %llu", itr->exec_time);

    itr = itr->next;
  }
  fprintf(report, "], [");
  itr = smallest_exec_time;
  unsigned cummulative_job_count = 0;
  while (itr != NULL) {
    cummulative_job_count += itr->job_count;

    fprintf(report, " %.06f", cummulative_job_count / (double) total_job_count);

    itr = itr->next;
  }
  fprintf(report, "]);\n");
}

enum cdf_format {
  NO_CDF,
  GNUPLOT,
  MATLAB,
};
static void print_exec_time_cdf(struct exec_time_list *smallest_exec_time,
                                unsigned total_job_count, FILE *report,
                                enum cdf_format cdf_fmt)
{
  if (total_job_count == 0) {
    return;
  }

  switch(cdf_fmt) {
  case GNUPLOT:
    print_gnuplot_cdf(smallest_exec_time, total_job_count, report);
    break;
  case MATLAB:
    print_matlab_cdf(smallest_exec_time, total_job_count, report);
    break;
  case NO_CDF:
    return;
  }
}

static void print_utility_time(FILE *report,
                               const utility_time *t, const char *label)
{
  char *t_str = to_string_dyn_gc(t);
  fprintf(report, "%s: %s\n", label, t_str);
  free(t_str);
}

struct task_stats
{
  FILE *report;
  relative_time period;
  relative_time deadline;
  absolute_time t_0;
  relative_time offset;
  unsigned nth_job;
  unsigned late_count;
  int suppress_printout;

  struct exec_time_list *exec_times;
};

static int print_task_stats(task *tau, void *args)
{
  struct task_stats *prms = args;

  if (!prms->suppress_printout) {
    fprintf(prms->report, "Name: %s\n", task_statistics_name(tau));

    print_utility_time(prms->report, task_statistics_wcet(tau), "WCET");
    print_utility_time(prms->report, task_statistics_period(tau), "Period");
    print_utility_time(prms->report, task_statistics_deadline(tau), "Deadline");
    print_utility_time(prms->report, task_statistics_t0(tau), "t_0");
    print_utility_time(prms->report, task_statistics_offset(tau), "Offset");
    print_utility_time(prms->report,
                       task_statistics_job_statistics_overhead(tau),
                       "Job statistics overhead");
    print_utility_time(prms->report,
                       task_statistics_finish_to_start_overhead(tau),
                       "Task finish-to-start overhead");
    fprintf(prms->report, "Task is %s\n",
            task_statistics_aperiodic(tau) ? "aperiodic" : "periodic");
    fprintf(prms->report, "Timing recording is %s\n",
            task_statistics_job_statistics_disabled(tau)
            ? "disabled" : "enabled");
  }

  utility_time_to_utility_time_gc(task_statistics_period(tau),
                                  &prms->period);
  utility_time_to_utility_time_gc(task_statistics_deadline(tau),
                                  &prms->deadline);
  utility_time_to_utility_time_gc(task_statistics_t0(tau),
                                  &prms->t_0);
  utility_time_to_utility_time_gc(task_statistics_offset(tau),
                                  &prms->offset);
  
  return 0;
}

static int print_job_stats(job_statistics *stats, void *args)
{
  struct task_stats *prms = args;

  if (!prms->suppress_printout) {
    if (prms->nth_job == 1) {
      fprintf(prms->report, "%5s%15s%15s%15s%15s%15s%15s%15s\n",
              "#job", "release", "delta_s", "start", "deadline", "delta_f",
              "finish", "exec_time");
    }

    fprintf(prms->report, "%5d", prms->nth_job);
  }

  /* Start time */
  absolute_time *t_release
    = utility_time_add_dyn_gc(&prms->offset,
                              utility_time_mul_dyn(&prms->period,
                                                   prms->nth_job - 1));
  char *t_str;
  if (!prms->suppress_printout) {
    t_str = to_string_dyn(t_release);
    fprintf(prms->report, "%15s", t_str);
    free(t_str);
  }

  absolute_time *t_start
    = utility_time_sub_dyn_gc(job_statistics_time_start(stats), &prms->t_0);
  if (utility_time_lt(t_release, t_start)) {
    relative_time *t_start_rel = utility_time_sub_dyn(t_start, t_release);

    if (!prms->suppress_printout) {
      t_str = to_string_dyn_gc(t_start_rel);
      char *t_str_new = malloc(strlen(t_str) + 2);
      t_str_new[0] = '+';
      strcpy(t_str_new + 1, t_str);
      free(t_str);
      fprintf(prms->report, "%15s", t_str_new);
      free(t_str_new);
    } else {
      utility_time_gc(t_start_rel);
    }
  } else {
    relative_time *t_start_rel = utility_time_sub_dyn(t_release, t_start);

    if (!prms->suppress_printout) {
      t_str = to_string_dyn_gc(t_start_rel);
      char *t_str_new = malloc(strlen(t_str) + 2);
      t_str_new[0] = '-';
      strcpy(t_str_new + 1, t_str);
      free(t_str);
      fprintf(prms->report, "%15s", t_str_new);
      free(t_str_new);
    } else {
      utility_time_gc(t_start_rel);
    }
  }

  if (!prms->suppress_printout) {
    t_str = to_string_dyn(t_start);
    fprintf(prms->report, "%15s", t_str);
    free(t_str);
  }
  /* End of start time */

  /* Finishing time */
  absolute_time *t_deadline = utility_time_add_dyn_gc(t_release,
                                                      &prms->deadline);
  if (!prms->suppress_printout) {
    t_str = to_string_dyn(t_deadline);
    fprintf(prms->report, "%15s", t_str);
    free(t_str);
  }

  int is_late = 0;
  absolute_time *t_finish
    = utility_time_sub_dyn_gc(job_statistics_time_finish(stats), &prms->t_0);
  if (utility_time_lt(t_deadline, t_finish)) {
    relative_time *t_finish_rel = utility_time_sub_dyn(t_finish, t_deadline);

    if (!prms->suppress_printout) {
      t_str = to_string_dyn_gc(t_finish_rel);
      char *t_str_new = malloc(strlen(t_str) + 2);
      t_str_new[0] = '+';
      strcpy(t_str_new + 1, t_str);
      free(t_str);
      fprintf(prms->report, "%15s", t_str_new);
      free(t_str_new);
    } else {
      utility_time_gc(t_finish_rel);
    }

    is_late = 1;
    prms->late_count++;
  } else {
    relative_time *t_finish_rel = utility_time_sub_dyn(t_deadline, t_finish);

    if (!prms->suppress_printout) {
      t_str = to_string_dyn_gc(t_finish_rel);
      char *t_str_new = malloc(strlen(t_str) + 2);
      t_str_new[0] = '-';
      strcpy(t_str_new + 1, t_str);
      free(t_str);
      fprintf(prms->report, "%15s", t_str_new);
      free(t_str_new);
    } else {
      utility_time_gc(t_finish_rel);
    }
  }
  utility_time_gc(t_deadline);

  if (!prms->suppress_printout) {
    t_str = to_string_dyn(t_finish);
    fprintf(prms->report, "%15s", t_str);
    free(t_str);
  }
  /* End of finishing time */

  /* Execution time */
  relative_time *exec_time = utility_time_sub_dyn_gc(t_finish, t_start);

  struct timespec exec_time_duration;
  to_timespec(exec_time, &exec_time_duration);
  prms->exec_times = insert_exec_time(exec_time_duration.tv_sec * 1000000000ULL
                                      + exec_time_duration.tv_nsec,
                                      prms->exec_times);
  if (!prms->suppress_printout) {
    t_str = to_string_dyn_gc(exec_time);
    fprintf(prms->report, "%15s", t_str);
    free(t_str);
  } else {
    utility_time_gc(exec_time);
  }
  /* End of execution time */

  if (!prms->suppress_printout) {
    fprintf(prms->report, "%s\n", is_late ? " LATE" : "");
  }

  prms->nth_job++;

  return 0;
}

const char prog_name[] = "read_task_stats_file";
FILE *log_stream;

int main(int argc, char **argv, char **envp)
{
  log_stream = stderr;

  enum cdf_format cdf_fmt = NO_CDF;
  const char *task_stat_file = NULL;
  {
    int optchar;
    opterr = 0;
    while ((optchar = getopt(argc, argv, ":hc:")) != -1) {
      switch (optchar) {
      case 'c':
        if (strcasecmp("gnuplot", optarg) == 0) {
          cdf_fmt = GNUPLOT;
        } else if (strcasecmp("matlab", optarg) == 0) {
          cdf_fmt = MATLAB;
        } else {
          fatal_error("Unrecognized CDF format: '%s'", optarg);
        }
        break;
      case 'h':
        printf("Usage: %s [-c {GNUPLOT|MATLAB}] TASK_STAT_FILE\n"
               "\n"
               "This program reads a task statistics file produced by\n"
               "function task_create of task.h.\n"
               "Instead of listing all task statistics, option -c can be used\n"
               "to obtain the CDF (Cumulative Distribution Function) of the\n"
               "job response times in several formats:\n"
               "  - GNUPLOT produces two columns of numbers. The first column\n"
               "    is the response time rho in microsecond while the second\n"
               "    one is the probability of obtaining a job whose response\n"
               "    time is less than or equal to rho.\n"
               "  - MATLAB producs a single line: plot(X, Y) where X is the\n"
               "    vector containing the response times while Y is the\n"
               "    vector containing the probabilities.\n",
               prog_name);
        return EXIT_SUCCESS;
      case '?':
        fatal_error("Unrecognized option character -%c", optopt);
      case ':':
        fatal_error("Option -%c needs an argument", optopt);
      default:
        fatal_error("Unexpected return value of fn getopt");
      }
    }
  }
  if (argv[optind] == NULL) {
    fatal_error("TASK_STAT_FILE must be specified (-h for help)");
  } else {
    task_stat_file = argv[optind];
  }

  FILE *stats_file = utility_file_open_for_reading_bin(task_stat_file);
  if (stats_file == NULL) {
    fatal_error("Cannot open task stat file '%s'", task_stat_file);
  }

  struct task_stats stats_prms = {
    .report = stdout,
    .nth_job = 1,
    .late_count = 0,
    .exec_times = NULL,
    .suppress_printout = (cdf_fmt != NO_CDF),
  };
  utility_time_init(&stats_prms.period);
  utility_time_init(&stats_prms.deadline);
  utility_time_init(&stats_prms.t_0);
  utility_time_init(&stats_prms.offset);

  if (task_statistics_read(stats_file, print_task_stats, &stats_prms,
                           print_job_stats, &stats_prms) != 0) {
    fatal_error("Cannot read task stat file '%s'", argv[1]);
  }

  utility_file_close(stats_file, argv[1]);

  print_exec_time_cdf(stats_prms.exec_times, stats_prms.nth_job - 1,
                      stats_prms.report, cdf_fmt);

  free_exec_time_list(stats_prms.exec_times);

  return EXIT_SUCCESS;
}
