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

    /* Adjust client -v server_pid */
    if (itr->server_pid != NULL) {
      int i;
      for (i = 0; itr->argv[i] != NULL; i++) {
        if (itr->argv[i][0] == '-' && itr->argv[i][1] == 'v') {
          char *arg_buffer;
          if (itr->argv[i][2] == '\0') {
            arg_buffer = itr->argv[i + 1];
          } else {
            arg_buffer = &itr->argv[i][2];
          }

          snprintf(arg_buffer, strlen(arg_buffer), "%d", *itr->server_pid);

          break;
        }
      }
    }
    /* END: Adjust client -v server_pid */
  }

  return 0;
}

/* Procs management section */
enum duration_unit {
  UNSPECIFIED,
  S,
  MS,
};

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

static int parse_config_file(const char *line, void *args)
{
  struct proc **preceding_server_ptr = (struct proc **) args;
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
        *preceding_server_ptr = p;
        break;
      case CLIENT:
        procs_add(&client_procs, p);
        if (*preceding_server_ptr != NULL) {
          p->server_pid = &(*preceding_server_ptr)->proc_id;
        }
        break;
      case CPU_HOG:
        procs_add(&cpu_hog_procs, p);
        break;
      case CPU_HOG_CBS:
        procs_add(&cpu_hog_cbs_procs, p);
        break;
      case HRT_CBS:
        procs_add(&hrt_cbs_procs, p);
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

      if (i == CLIENT && *preceding_server_ptr != NULL) {
        args_buflen = snprintf(NULL, 0, SNPRINTF_ARGS(" -v %d", INT_MAX)) + 1;
      } else {
        args_buflen = snprintf(NULL, 0, SNPRINTF_ARGS("")) + 1;
      }

      p->args = malloc(args_buflen);
      if (p->args == NULL) {
        log_error("Not enough memory to allocate proc args");
        goto error;
      }

      if (i == CLIENT && *preceding_server_ptr != NULL) {
        snprintf(p->args, args_buflen, SNPRINTF_ARGS(" -v %d", INT_MAX));
      } else {
        snprintf(p->args, args_buflen, SNPRINTF_ARGS(""));
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
static unsigned long int experiment_duration = -1;
static enum duration_unit experiment_duration_unit = UNSPECIFIED;
static int cbs_budget_period_ms = -1;
static int parse_cmd_line_args(int argc, char **argv)
{
  char *duration_unit;
  int optchar;
  opterr = 0;
  while ((optchar = getopt(argc, argv, ":ht:f:p:")) != -1) {
    switch (optchar) {
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
      experiment_duration = strtoul(optarg, &duration_unit, 10);
      if (experiment_duration <= 0) {
        log_error("Experiment duration must be greater than 0 (-h for help)");
        return -1;
      }

      if (strcasecmp(duration_unit, "s") == 0) {
        experiment_duration_unit = S;
      } else if (strcasecmp(duration_unit, "ms") == 0) {
        experiment_duration_unit = MS;
      } else {
        log_error("Experimentation duration must be directly followed by"
                  " either 's' or 'ms' (-h for help)");
        return -1;
      }
      break;
    case 'h':
      printf("Usage: %1$s -t DURATION -p BUDGET_PERIOD -f CONFIG_FILE\n"
             "       [-r]\n"
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
             "     CLIENT -1 5 -2 9 -3 5 -q 20 -t 30 -p 7777 -s x.bin\n"
             "   Available program IDs: SERVER, CLIENT, CPU_HOG, CPU_HOG_CBS,\n"
             "                          HRT_CBS.\n"
             "   The option arguments must not contain any whitespace.\n"
             "   Blank lines and lines starting with # will be ignored.\n"
             "   Special for CLIENT program ID, if it comes after a SERVER\n"
             "   program ID, the option -v will be forced to have the value\n"
             "   of the preceding SERVER PID. To have a client process that\n"
             "   is not associated with any specified server process,\n"
             "   specify the CLIENT program ID before any SERVER program ID.\n",
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

  if (experiment_duration == -1) {
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
    struct proc *preceding_server = NULL;
    if (utility_file_read(config_file, 1024, parse_config_file,
                          &preceding_server) != 0) {
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
  switch (experiment_duration_unit) {
  case S:
    sleep(experiment_duration);
  case MS:
    usleep(experiment_duration * 1000);
  case UNSPECIFIED:
    break;
  }
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
