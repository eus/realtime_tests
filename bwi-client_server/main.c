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

#include <limits.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <string.h>
#include <strings.h>
#include "../utility_experimentation.h"
#include "../utility_log.h"
#include "../utility_sched_deadline.h"
#include "../utility_subprocess.h"
#include "../utility_file.h"

struct proc {
  struct proc *next;
  char *args;
  char **argv;
  pid_t proc_id;
  pid_t *server_pid; /* Only used by client process */
  unsigned long *duration_ms; /* Only used by client and hrt_cbs processes */
  int *ftrace_start; /* Only used by client process */
  int *ftrace_stop; /* Only used by client process */
};

static struct proc *proc_new(void)
{
  struct proc *res = malloc(sizeof(*res));
  if (res == NULL) {
    return NULL;
  }

  memset(res, 0, sizeof(*res));
  res->proc_id = -1;
  return res;
}

static void proc_free(struct proc *proc)
{
  if (proc->argv != NULL) {
    free(proc->argv);
  }
  if (proc->args != NULL) {
    free(proc->args);
  }
  free(proc);
}

struct proc_head {
  struct proc *first;
  struct proc *last;
};

#define PROC_HEAD(name) struct proc_head name = {NULL, NULL}

static void procs_add(struct proc_head *head, struct proc *new)
{
  new->next = NULL;
  if (head->first == NULL) {
    head->first = new;
  }
  if (head->last != NULL) {
    head->last->next = new;
  }
  head->last = new;
}

#define foreach_proc(itr, head)                         \
  for (itr = head->first; itr != NULL; itr = itr->next)

static void procs_free(struct proc_head *head)
{
  struct proc *prev = NULL, *itr;

  foreach_proc(itr, head) {
    if (prev != NULL) {
      proc_free(prev);
    }
    prev = itr;
  }
  if (prev != NULL) {
    proc_free(prev);
  }

  head->first = NULL;
  head->last = NULL;
}

static int procs_kill_and_wait(struct proc_head *head, int signal)
{
  struct proc *itr;
  int rc = 0;
  char pid_buffer[32];

  foreach_proc(itr, head) {
    if (itr->proc_id == -1) {
      continue;
    }

    snprintf(pid_buffer, sizeof(pid_buffer), "%d", itr->proc_id);
    rc += kill_and_wait_proc_fn(itr->proc_id, signal, pid_buffer);
  }

  return rc ? -1 : 0;
}

static int procs_kill(struct proc_head *head, int signal)
{
  struct proc *itr;
  int rc = 0;
  char pid_buffer[32];

  foreach_proc(itr, head) {
    if (itr->proc_id == -1) {
      continue;
    }

    snprintf(pid_buffer, sizeof(pid_buffer), "%d", itr->proc_id);
    rc += kill_proc_fn(itr->proc_id, signal, pid_buffer);
  }

  return rc ? -1 : 0;
}

static int procs_wait(struct proc_head *head)
{
  struct proc *itr;
  int rc = 0;
  char pid_buffer[32];

  foreach_proc(itr, head) {
    if (itr->proc_id == -1) {
      continue;
    }

    snprintf(pid_buffer, sizeof(pid_buffer), "%d", itr->proc_id);
    rc += wait_proc_fn(itr->proc_id, pid_buffer);
  }

  return rc ? -1 : 0;
}

static void procs_print_args(const struct proc_head *head)
{
  const struct proc *itr;
  foreach_proc(itr, head) {
    if (itr->args == NULL) {
      continue;
    }

    printf("%s\n", itr->args);
  }
}

static void procs_print_argv(const struct proc_head *head)
{
  const struct proc *itr;
  foreach_proc(itr, head) {
    int i = 0;

    if (itr->argv == NULL) {
      continue;
    }

    printf("%s", itr->argv[i++]);
    for (; itr->argv[i] != NULL; i++) {
      printf(" %s", itr->argv[i]);
    }
    printf("\n");
  }
}

static int procs_create(struct proc_head *head)
{
  struct proc *itr;

  foreach_proc(itr, head) {
    itr->proc_id = fork();
    if (itr->proc_id == -1) {
      log_syserror("Cannot fork");
      return -1;
    } else if (itr->proc_id == 0) {
      execv(itr->argv[0], itr->argv);
      log_syserror("Cannot exec");
      return -1;
    }

    sleep(1);
  }

  return 0;
}

const char *delimiter = " \t";
static char *find_argv_to_adjust(int target_optchar, struct proc *itr)
{
  int i;

  for (i = 0; itr->argv[i] != NULL; i++) {
    if (itr->argv[i][0] == '-' && itr->argv[i][1] == target_optchar) {
      char *arg_buffer;
      if (itr->argv[i][2] == '\0') {
        arg_buffer = itr->argv[i + 1];
      } else {
        arg_buffer = &itr->argv[i][2];
      }

      return arg_buffer;
    }
  }

  return NULL;
}
static int procs_make_argv(struct proc_head *head)
{
  struct proc *itr;

  foreach_proc(itr, head) {
    /* Count token */
    int argc = 0;
    {
      size_t buflen = strlen(itr->args) + 1;
      char *buffer = malloc(buflen);
      if (buffer == NULL) {
        log_error("Insufficient memory for tokenizing buffer");
        return -1;
      }

      memcpy(buffer, itr->args, buflen);
      char *tok = strtok(buffer, delimiter);
      while (tok != NULL) {
        argc++;
        tok = strtok(NULL, delimiter);
      }

      free(buffer);
    }
    /* END: Count token */

    /* Craft argv */
    {
      itr->argv = malloc(sizeof(*itr->argv) * (argc + 1));
      if (itr->argv == NULL) {
        log_error("Insufficient memory for argv array");
        return -1;
      }

      char *tok = strtok(itr->args, delimiter);
      int i = 0;
      while (tok != NULL) {
        itr->argv[i++] = tok;

        tok = strtok(NULL, delimiter);
      }
      itr->argv[i] = NULL;
    }
    /* END: Craft argv */

    /* Adjust client -v server_pid, -x duration or hrt_cbs -x duration */
    if (itr->server_pid != NULL) {
      char *argv_to_adjust = find_argv_to_adjust('v', itr);
      if (argv_to_adjust != NULL) {
        snprintf(argv_to_adjust, strlen(argv_to_adjust),
                 "%d", *itr->server_pid);
      }
    }
    if (itr->duration_ms != NULL) {
      char *argv_to_adjust = find_argv_to_adjust('x', itr);
      if (argv_to_adjust != NULL) {
        snprintf(argv_to_adjust, strlen(argv_to_adjust),
                 "%lu", *itr->duration_ms);
      }
    }
    if (itr->ftrace_start != NULL) {
      char *argv_to_adjust = find_argv_to_adjust('r', itr);
      if (argv_to_adjust != NULL) {
        snprintf(argv_to_adjust, strlen(argv_to_adjust),
                 "%d", *itr->ftrace_start);
      }
    }
    if (itr->ftrace_stop != NULL) {
      char *argv_to_adjust = find_argv_to_adjust('l', itr);
      if (argv_to_adjust != NULL) {
        snprintf(argv_to_adjust, strlen(argv_to_adjust),
                 "%d", *itr->ftrace_stop);
      }
    }
    /* END: Adjust client -v server_pid, -x duration or hrt_cbs -x duration */
  }

  return 0;
}

/* Procs management section */
enum program_ids_idx {
  SERVER, CLIENT, CPU_HOG, CPU_HOG_CBS, HRT_CBS, SERVER_HOG,
};
static const char *const program_ids[] = {
  "server", "client", "cpu_hog", "cpu_hog_cbs", "hrt_cbs", "server_hog",
  NULL
};
static PROC_HEAD(server_procs);
static PROC_HEAD(client_procs);
static PROC_HEAD(cpu_hog_procs);
static PROC_HEAD(cpu_hog_cbs_procs);
static PROC_HEAD(hrt_cbs_procs);
static PROC_HEAD(server_hog_procs);

static void terminate_driver(void)
{
  procs_kill_and_wait(&server_procs, SIGKILL);
  procs_free(&server_procs);
  procs_kill_and_wait(&client_procs, SIGKILL);
  procs_free(&client_procs);
  procs_kill_and_wait(&cpu_hog_procs, SIGKILL);
  procs_free(&cpu_hog_procs);
  procs_kill_and_wait(&cpu_hog_cbs_procs, SIGKILL);
  procs_free(&cpu_hog_cbs_procs);
  procs_kill_and_wait(&hrt_cbs_procs, SIGKILL);
  procs_free(&hrt_cbs_procs);
  procs_kill_and_wait(&server_hog_procs, SIGKILL);
  procs_free(&server_hog_procs);
}

struct parse_config_file_prms
{
  struct proc *preceding_server;
  unsigned long *duration_ms;
  int *ftrace_start;
  int *ftrace_stop;
};
static int parse_config_file(const char *line, void *args)
{
  struct parse_config_file_prms *prms = (struct parse_config_file_prms *) args;
  char *token;
  size_t buflen = strlen(line) + 1;
  char *buffer = malloc(buflen);
  if (buffer == NULL) {
    log_error("Not enough memory to allocate buffer\n");
    return -1;
  }
  memcpy(buffer, line, buflen);
  buffer[buflen - 1] = '\0';

  token = strtok(buffer, delimiter);
  if (token == NULL || token[0] == '#')
    goto out;

  int i;
  for (i = 0; program_ids[i] != NULL; i++) {
    if (strcasecmp(token, program_ids[i]) == 0) {
      char *remaining_line;
      size_t args_buflen;
      struct proc *p = proc_new();
      if (p == NULL) {
        log_error("Not enough memory to allocate proc node");
        goto error;
      }

      switch (i) {
      case SERVER:
        procs_add(&server_procs, p);
        prms->preceding_server = p;
        break;
      case CLIENT:
        procs_add(&client_procs, p);
        if (prms->preceding_server != NULL) {
          p->server_pid = &prms->preceding_server->proc_id;
        }
        p->duration_ms = prms->duration_ms;
        p->ftrace_start = prms->ftrace_start;
        p->ftrace_stop = prms->ftrace_stop;
        break;
      case CPU_HOG:
        procs_add(&cpu_hog_procs, p);
        break;
      case CPU_HOG_CBS:
        procs_add(&cpu_hog_cbs_procs, p);
        break;
      case HRT_CBS:
        procs_add(&hrt_cbs_procs, p);
        p->duration_ms = prms->duration_ms;
        break;
      case SERVER_HOG:
        procs_add(&server_hog_procs, p);
        break;
      }

#define SNPRINTF_ARGS(fmt_add, ...)                                     \
      "./%s %s" fmt_add, program_ids[i], remaining_line, ## __VA_ARGS__

      remaining_line = token + strlen(token) + 1;
      if (remaining_line >= buffer + buflen) { /* No remaining part */
        remaining_line = "";
      }

      if (i == CLIENT) {
        if (prms->preceding_server == NULL) {
          if (prms->ftrace_start != NULL && prms->ftrace_stop != NULL) {
            args_buflen = snprintf(NULL, 0,
                                   SNPRINTF_ARGS(" -x %lu -r %d -l %d",
                                                 ULONG_MAX, INT_MAX,
                                                 INT_MAX)) + 1;
          } else {
            args_buflen = snprintf(NULL, 0,
                                   SNPRINTF_ARGS(" -x %lu", ULONG_MAX)) + 1;
          }
        } else {
          if (prms->ftrace_start != NULL && prms->ftrace_stop != NULL) {
            args_buflen = snprintf(NULL, 0,
                                   SNPRINTF_ARGS(" -v %d -x %lu -r %d -l %d",
                                                 INT_MAX, ULONG_MAX,
                                                 INT_MAX, INT_MAX)) + 1;
          } else {
            args_buflen = snprintf(NULL, 0,
                                   SNPRINTF_ARGS(" -v %d -x %lu",
                                                 INT_MAX, ULONG_MAX)) + 1;
          }
        }
      } else if (i == HRT_CBS) {
        args_buflen = snprintf(NULL, 0,
                               SNPRINTF_ARGS(" -x %lu", ULONG_MAX)) + 1;
      } else {
        args_buflen = snprintf(NULL, 0, SNPRINTF_ARGS("")) + 1;
      }

      p->args = malloc(args_buflen);
      if (p->args == NULL) {
        log_error("Not enough memory to allocate proc args");
        goto error;
      }

      if (i == CLIENT) {
        if (prms->preceding_server == NULL) {
          if (prms->ftrace_start != NULL && prms->ftrace_stop != NULL) {
            args_buflen = snprintf(p->args, args_buflen,
                                   SNPRINTF_ARGS(" -x %lu -r %d -l %d",
                                                 ULONG_MAX, INT_MAX,
                                                 INT_MAX)) + 1;
          } else {
            args_buflen = snprintf(p->args, args_buflen,
                                   SNPRINTF_ARGS(" -x %lu", ULONG_MAX)) + 1;
          }
        } else {
          if (prms->ftrace_start != NULL && prms->ftrace_stop != NULL) {
            args_buflen = snprintf(p->args, args_buflen,
                                   SNPRINTF_ARGS(" -v %d -x %lu -r %d -l %d",
                                                 INT_MAX, ULONG_MAX,
                                                 INT_MAX, INT_MAX)) + 1;
          } else {
            args_buflen = snprintf(p->args, args_buflen,
                                   SNPRINTF_ARGS(" -v %d -x %lu",
                                                 INT_MAX, ULONG_MAX)) + 1;
          }
        }
      } else if (i == HRT_CBS) {
        args_buflen = snprintf(p->args, args_buflen,
                               SNPRINTF_ARGS(" -x %lu", ULONG_MAX)) + 1;
      } else {
        args_buflen = snprintf(p->args, args_buflen, SNPRINTF_ARGS("")) + 1;
      }
#undef SNPRINTF_ARGS

      break;
    }
  }
  if (program_ids[i] == NULL) {
    log_error("Unrecognized program ID '%s'", token);
    goto error;
  }

 out:
  free(buffer);
  return 0;

 error:
  free(buffer);
  return -1;
}

/* END: Procs management section */

/* Command line args section */
static const char *config_path = NULL;
static unsigned long int experiment_duration_ms = -1;
static int cbs_budget_period_ms = -1;
static int ftrace_start = -1;
static int ftrace_stop = -1;
static int parse_cmd_line_args(int argc, char **argv)
{
  char *duration_unit;
  int optchar;
  opterr = 0;
  while ((optchar = getopt(argc, argv, ":hr:l:t:f:p:")) != -1) {
    switch (optchar) {
    case 'r':
      ftrace_start = atoi(optarg);
      if (ftrace_start <= 0) {
        log_error("-r must be at least 1 (-h for help)");
        return -1;
      }
      break;
    case 'l':
      ftrace_stop = atoi(optarg);
      if (ftrace_stop <= 0) {
        log_error("-l must be at least 1 (-h for help)");
        return -1;
      }
      break;
    case 'p':
      cbs_budget_period_ms = atoi(optarg);
      if (cbs_budget_period_ms <= 0) {
        log_error("-p must be at least 1 ms (-h for help)");
        return -1;
      }
      break;
    case 'f':
      config_path = optarg;
      break;
    case 't':
      experiment_duration_ms = strtoul(optarg, &duration_unit, 10);
      if (experiment_duration_ms <= 0) {
        log_error("Experiment duration must be greater than 0 (-h for help)");
        return -1;
      }

      if (strcasecmp(duration_unit, "s") == 0) {
        experiment_duration_ms *= 1000;
      } else if (strcasecmp(duration_unit, "ms") != 0) {
        log_error("Experimentation duration must be directly followed by"
                  " either 's' or 'ms' (-h for help)");
        return -1;
      }
      break;
    case 'h':
      printf("Usage: %1$s -t DURATION -p BUDGET_PERIOD -f CONFIG_FILE\n"
             "       [-r CLIENT_TRACING_START -l CLIENT_TRACING_LIMIT]\n"
             "\n"
             "This is the experiment driver that will create the necessary\n"
             "programs in the proper order and timing allowing them to\n"
             "calibrate their busy loops properly, signal them to start at\n"
             "approximately the same time, and terminate them after a\n"
             "specified duration has elapsed.\n"
             "This driver read a configuration file to create the desired\n"
             "programs having the desired parameters.\n"
             "SIGINT can be used to terminate this program graciously at any\n"
             "time.\n"
             "\n"
	     "-r CLIENT_TRACING_START is used to start ftrace at the"
	     "   beginning of the n-th period if n > 0, and to stop ftrace at\n"
	     "   the end of the epilogue in that period unless\n"
	     "   CLIENT_TRACING_LIMIT is given in which case stop ftrace at\n"
	     "   (CLIENT_TRACING_START + CLIENT_TRACING_LIMIT - 1)-th\n"
	     "   iteration. The former is useful for debugging kernel BWI\n"
	     "   timing by analyzing ftrace output while the latter is\n"
	     "   useful to get a snapshot of the execution for illustration\n"
	     "   in a journal/book. If this is set to 0, ftrace will be\n"
	     "   enabled continuously up to either the time where the\n"
	     "   response time is more than CLIENT_TRACING_LIMIT or the end\n"
	     "   of this program; when ftrace is stopped, this client program\n"
	     "   will print the job number that turns the ftrace off in\n"
	     "   stdout in the format: Job #JOB_NUMBER\n"
	     "-l CLIENT_TRACING_LIMIT can either be a time in millisecond or\n"
	     "   an iteration count as explained above.\n"
             "-t DURATION is the desired duration of the experiment in\n"
             "   either second (e.g., -t 60s) or millisecond\n"
             "   (e.g., -t 60ms).\n"
             "-p BUDGET_PERIOD is the CBS budget and period in millisecond\n"
             "   of this driver. It should be picked in such a way so that\n"
             "   this driver will have the shorthest period among all CBSes\n"
             "   used in the experiment. In this way, this driver can stop\n"
             "   all CBSes in time.\n"
             "-f CONFIG_FILE is the file listing in each line the program ID\n"
             "   and the executable command line arguments like:\n"
             "     SERVER -d 9 -p 7777\n"
             "     CLIENT -1 5 -2 9 -3 5 -q 20 -t 30 -p 7777 -s x.bin -x 60\n"
             "   Available program IDs: SERVER, CLIENT, CPU_HOG, CPU_HOG_CBS,\n"
             "                          HRT_CBS.\n"
             "   The option arguments must not contain any whitespace.\n"
             "   Blank lines and lines starting with # will be ignored.\n"
             "   Special for CLIENT program ID, if it comes after a SERVER\n"
             "   program ID, the option -v will be forced to have the value\n"
             "   of the preceding SERVER PID. To have a client process that\n"
             "   is not associated with any specified server process,\n"
             "   specify the CLIENT program ID before any SERVER program ID.\n"
             "   Also special for CLIENT and HRT_CBS program ID is that -x\n"
             "   will be forced to have the DURATION value specified to the\n"
             "   driver using -t",
             prog_name);
      return EXIT_SUCCESS;
    case '?':
      log_error("Unrecognized option -%c (-h for help)", optopt);
      return -1;
    case ':':
      log_error("Option -%c needs an argument (-h for help)", optopt);
      return -1;
    default:
      log_error("Unexpected return value of fn getopt");
      return -1;
    }
  }

  if (experiment_duration_ms == -1) {
    log_error("-t must be specified (-h for help)");
    return -1;
  }
  if (cbs_budget_period_ms == -1) {
    log_error("-p must be specified (-h for help)");
    return -1;
  }
  if (config_path == NULL) {
    log_error("-f must be specified (-h for help)");
    return -1;
  }
  if ((ftrace_start != -1 && ftrace_stop == -1)
      || (ftrace_start == -1 && ftrace_stop != -1)) {
    log_error("-r must be specified together with -l (-h for help)");
    return -1;
  }

  return optind;
}
/* END: Command line args section */

static void sighandler(int signo)
{
  terminate_driver();
  _exit(EXIT_FAILURE);
}

MAIN_BEGIN("bwi-client_server", "stderr", NULL)
{
  log_stream = stderr;
  struct sigaction sigact = {
    .sa_handler = sighandler,
  };
  int rc = EXIT_SUCCESS;

  sigaction(SIGINT, &sigact, NULL);

  /* Parse command line arguments */
  if (parse_cmd_line_args(argc, argv) == -1) {
    goto error;
  }  
  /* END: Parse command line arguments */

  /* Parse config file */
  {
    FILE *config_file = utility_file_open_for_reading(config_path);
    if (config_file == NULL) {
      goto error;
    }
    struct parse_config_file_prms args = {
      .preceding_server = NULL,
      .duration_ms = &experiment_duration_ms,
      .ftrace_start = ftrace_start == -1 ? NULL : &ftrace_start,
      .ftrace_stop = ftrace_stop == -1 ? NULL : &ftrace_stop,
    };
    if (utility_file_read(config_file, 1024, parse_config_file, &args) != 0) {
      log_error("Cannot completely parse the config file");
      goto error;
    }
    utility_file_close(config_file, config_path);
  }
  /* END: Parse config file */

  /* Create processes */
  if (procs_make_argv(&hrt_cbs_procs) != 0)
    goto error;
  if (procs_make_argv(&cpu_hog_cbs_procs) != 0)
    goto error;
  if (procs_make_argv(&cpu_hog_procs) != 0)
    goto error;
  if (procs_make_argv(&server_hog_procs) != 0)
    goto error;
  if (procs_make_argv(&server_procs) != 0)
    goto error;

  if (procs_create(&server_procs) != 0)
    goto error;
  if (procs_make_argv(&client_procs) != 0)
    goto error;

  if (procs_create(&client_procs) != 0)
    goto error;
  if (procs_create(&hrt_cbs_procs) != 0)
    goto error;
  if (procs_create(&cpu_hog_cbs_procs) != 0)
    goto error;
  if (procs_create(&cpu_hog_procs) != 0)
    goto error;
  if (procs_create(&server_hog_procs) != 0)
    goto error;
  /* END: Create processes */

  /* Enter SCHED_DEADLINE */
  if (sched_deadline_enter(to_utility_time_dyn(cbs_budget_period_ms, ms),
                           to_utility_time_dyn(cbs_budget_period_ms, ms), NULL)
      != 0) {
    log_error("Cannot enter SCHED_DEADLINE (privilege may be insufficient)");
    goto error;
  }
  /* END: Enter SCHED_DEADLINE */

  /* Start processes */
  rc = (procs_kill(&hrt_cbs_procs, SIGUSR1) != 0) ? EXIT_FAILURE : rc;
  rc = (procs_kill(&cpu_hog_cbs_procs, SIGUSR1) != 0) ? EXIT_FAILURE : rc;
  rc = (procs_kill(&client_procs, SIGUSR1) != 0) ? EXIT_FAILURE : rc;
  /* END: Start processes */

  /* Sleep for the duration of the experiment */
  usleep(experiment_duration_ms * 1000);
  /* END: Sleep for the duration of the experiment */

  /* Stop processes */
  rc = (procs_kill(&client_procs, SIGTERM) != 0) ? EXIT_FAILURE : rc;
  rc = (procs_kill(&cpu_hog_cbs_procs, SIGTERM) != 0) ? EXIT_FAILURE : rc;
  rc = (procs_kill(&hrt_cbs_procs, SIGTERM) != 0) ? EXIT_FAILURE : rc;
  rc = (procs_kill(&cpu_hog_procs, SIGTERM) != 0) ? EXIT_FAILURE : rc;
  rc = (procs_kill(&server_hog_procs, SIGTERM) != 0) ? EXIT_FAILURE : rc;
  sleep(1);
  rc = (procs_kill(&server_procs, SIGTERM) != 0) ? EXIT_FAILURE : rc;
  /* END: Stop processes */

  /* Collect processes */
  procs_wait(&client_procs);
  procs_wait(&cpu_hog_cbs_procs);
  procs_wait(&hrt_cbs_procs);
  procs_wait(&cpu_hog_procs);
  procs_wait(&server_hog_procs);
  procs_wait(&server_procs);
  /* END: Collect processes */

  /* Unnecessary clean-up since we're quitting anyway */
  procs_free(&server_procs);
  procs_free(&client_procs);
  procs_free(&cpu_hog_procs);
  procs_free(&server_hog_procs);
  procs_free(&cpu_hog_cbs_procs);
  procs_free(&hrt_cbs_procs);
  /* END: Unnecessary clean-up since we're quitting anyway */

  return rc;

 error:
  terminate_driver();
  return EXIT_FAILURE;

} MAIN_END
