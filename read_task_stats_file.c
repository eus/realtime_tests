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
#include "utility_log.h"
#include "utility_file.h"
#include "utility_time.h"
#include "task.h"

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
};

static int print_task_stats(task *tau, void *args)
{
  struct task_stats *prms = args;

  fprintf(prms->report, "Name: %s\n", task_statistics_name(tau));

  print_utility_time(prms->report, task_statistics_wcet(tau), "WCET");
  print_utility_time(prms->report, task_statistics_period(tau), "Period");
  print_utility_time(prms->report, task_statistics_deadline(tau), "Deadline");
  print_utility_time(prms->report, task_statistics_t0(tau), "t_0");
  print_utility_time(prms->report, task_statistics_offset(tau), "Offset");
  print_utility_time(prms->report, task_statistics_job_statistics_overhead(tau),
                     "Job statistics overhead");
  print_utility_time(prms->report,
                     task_statistics_finish_to_start_overhead(tau),
                     "Task finish-to-start overhead");
  fprintf(prms->report, "Task is %s\n",
          task_statistics_aperiodic(tau) ? "aperiodic" : "periodic");
  fprintf(prms->report, "Timing recording is %s\n",
          task_statistics_job_statistics_disabled(tau)
          ? "disabled" : "enabled");

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

  if (prms->nth_job == 1) {
    fprintf(prms->report, "%5s%15s%15s%15s%15s%15s%15s%15s\n",
            "#job", "release", "delta_s", "start", "deadline", "delta_f",
            "finish", "exec_time");
  }

  fprintf(prms->report, "%5d", prms->nth_job);

  /* Start time */
  absolute_time *t_release
    = utility_time_add_dyn_gc(&prms->offset,
                              utility_time_mul_dyn(&prms->period,
                                                   prms->nth_job - 1));
  char *t_str = to_string_dyn(t_release);
  fprintf(prms->report, "%15s", t_str);
  free(t_str);

  absolute_time *t_start
    = utility_time_sub_dyn_gc(job_statistics_time_start(stats), &prms->t_0);
  if (utility_time_lt(t_release, t_start)) {
    t_str = to_string_dyn_gc(utility_time_sub_dyn(t_start, t_release));

    char *t_str_new = malloc(strlen(t_str) + 2);
    t_str_new[0] = '+';
    strcpy(t_str_new + 1, t_str);
    free(t_str);
    fprintf(prms->report, "%15s", t_str_new);
    free(t_str_new);
  } else {
    t_str = to_string_dyn_gc(utility_time_sub_dyn(t_release, t_start));

    char *t_str_new = malloc(strlen(t_str) + 2);
    t_str_new[0] = '-';
    strcpy(t_str_new + 1, t_str);
    free(t_str);
    fprintf(prms->report, "%15s", t_str_new);
    free(t_str_new);
  }

  t_str = to_string_dyn(t_start);
  fprintf(prms->report, "%15s", t_str);
  free(t_str);
  /* End of start time */

  /* Finishing time */
  absolute_time *t_deadline = utility_time_add_dyn_gc(t_release,
                                                      &prms->deadline);
  t_str = to_string_dyn(t_deadline);
  fprintf(prms->report, "%15s", t_str);
  free(t_str);

  int is_late = 0;
  absolute_time *t_finish
    = utility_time_sub_dyn_gc(job_statistics_time_finish(stats), &prms->t_0);
  if (utility_time_lt(t_deadline, t_finish)) {
    t_str = to_string_dyn_gc(utility_time_sub_dyn(t_finish, t_deadline));

    char *t_str_new = malloc(strlen(t_str) + 2);
    t_str_new[0] = '+';
    strcpy(t_str_new + 1, t_str);
    free(t_str);
    fprintf(prms->report, "%15s", t_str_new);
    free(t_str_new);

    is_late = 1;
    prms->late_count++;
  } else {
    t_str = to_string_dyn_gc(utility_time_sub_dyn(t_deadline, t_finish));

    char *t_str_new = malloc(strlen(t_str) + 2);
    t_str_new[0] = '-';
    strcpy(t_str_new + 1, t_str);
    free(t_str);
    fprintf(prms->report, "%15s", t_str_new);
    free(t_str_new);
  }
  utility_time_gc(t_deadline);

  t_str = to_string_dyn(t_finish);
  fprintf(prms->report, "%15s", t_str);
  free(t_str);
  /* End of finishing time */

  /* Execution time */
  t_str = to_string_dyn_gc(utility_time_sub_dyn_gc(t_finish, t_start));
  fprintf(prms->report, "%15s", t_str);
  free(t_str);
  /* End of execution time */

  fprintf(prms->report, "%s\n", is_late ? " LATE" : "");

  prms->nth_job++;

  return 0;
}

const char prog_name[] = "read_task_stats_file";
FILE *log_stream;

int main(int argc, char **argv, char **envp)
{
  log_stream = stderr;

  if (argc != 2) {
    fatal_error("Usage: %s TASK_STAT_FILE", argv[0]);
  }

  FILE *stats_file = utility_file_open_for_reading_bin(argv[1]);
  if (stats_file == NULL) {
    fatal_error("Cannot open task stat file '%s'", argv[1]);
  }

  struct task_stats stats_prms = {
    .report = stdout,
    .nth_job = 1,
    .late_count = 0,
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

  return EXIT_SUCCESS;
}
