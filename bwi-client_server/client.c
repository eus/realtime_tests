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

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include "../utility_experimentation.h"
#include "../utility_log.h"
#include "../utility_cpu.h"
#include "../utility_time.h"
#include "../utility_sched_deadline.h"
#include "../task.h"

struct client_prog_prms;
static void client_prog(void *args);

struct client_thread_prms {
  int wcet_ms;
  int period_ms;
  const char *stats_file_path;
  struct client_prog_prms *client_prog_args;
  const relative_time *job_stats_overhead;
  const relative_time *task_overhead;
  task *client_task;
  int rc;
};
static void *client_thread(void *args)
{
  struct client_thread_prms *prms = args;
  prms->rc = EXIT_FAILURE;

  /* Block all signals */
  {
    sigset_t blocked_signals;
    if (sigfillset(&blocked_signals) != 0) {
      log_error("Cannot fill blocked_signals");
      return &prms->rc;
    }
    if ((errno = pthread_sigmask(SIG_BLOCK, &blocked_signals, NULL)) != 0) {
      log_syserror("Cannot block all signals");
      return &prms->rc;
    }
  }
  /* END: Block signals */

  /* Use CBS server */
  {
    int rc = sched_deadline_enter(to_utility_time_dyn(prms->wcet_ms, ms),
                                  to_utility_time_dyn(prms->period_ms, ms),
                                  NULL);
    if (rc == -1) {
      log_error("Insufficient privilege to use SCHED_DEADLINE");
      return &prms->rc;
    } else if (rc != 0) {
      log_error("Cannot enter SCHED_DEADLINE");
      return &prms->rc;
    }
  }
  /* END: Use CBS server */

  /* Wait for starting signal */
  {
    int signo;
    sigset_t starting_signal;
    sigemptyset(&starting_signal);
    sigaddset(&starting_signal, SIGUSR1);
    sigwait(&starting_signal, &signo);
  }
  /* END: Wait for starting signal */

  /* Create client task */
  {
    struct timespec t_now;
    if (clock_gettime(CLOCK_MONOTONIC, &t_now) != 0) {
      fatal_syserror("Cannot get t_now");
    }
    struct timespec t_release;
    to_timespec_gc(utility_time_add_dyn_gc(timespec_to_utility_time_dyn(&t_now),
                                           to_utility_time_dyn(1, s)),
                   &t_release);

    int rc = task_create("client",
                         to_utility_time_dyn(prms->wcet_ms, ms),
                         to_utility_time_dyn(prms->period_ms, ms),
                         to_utility_time_dyn(prms->wcet_ms, ms),
                         timespec_to_utility_time_dyn(&t_release),
                         to_utility_time_dyn(0, ms),
                         NULL, NULL,
                         prms->stats_file_path,
                         prms->job_stats_overhead,
                         prms->task_overhead,
                         0,
                         client_prog, prms->client_prog_args,
                         &prms->client_task);
    if (rc == -2) {
      fatal_error("Cannot open %s for writing", prms->stats_file_path);
    } else if (rc != 0) {
      fatal_error("Cannot create client task");
    }
  }  
  /* END: Create client task */

  /* Run the task */
  if (task_start(prms->client_task) != 0) {
    log_error("Client task cannot be completed successfully");
    return &prms->rc;
  }
  /* END: Run the task */

  prms->rc = EXIT_SUCCESS;
  return &prms->rc;
}

static void send_recv(int comm_socket, const void *data, size_t data_len,
                      void *buffer, size_t buf_len,
                      ssize_t *byte_sent, ssize_t *byte_rcvd,
                      int *send_errno, int *recv_errno)
{
  *byte_sent = send(comm_socket, data, data_len, 0);
  if (*byte_sent == -1) {
    *send_errno = errno;
  } else {
    *send_errno = 0;
  }

  *byte_rcvd = recv(comm_socket, buffer, buf_len, 0);
  if (*byte_rcvd == -1) {
    *recv_errno = errno;
  } else {
    *recv_errno = 0;
  }
}

struct send_recv_overhead_measurement_prms {
  int rc;
  int server_pid;
  int comm_socket;
  const void *request;
  void *response;
  size_t len;
  relative_time *overhead;
};
static void *send_recv_overhead_measurement_thread(void *args)
{
  struct send_recv_overhead_measurement_prms *prms = args;
  struct timespec t1, t2;
  ssize_t byte_sent, byte_rcvd;
  int send_errno, recv_errno;
  int counter;

  memset(prms->response, 0, prms->len);

  if (kill(prms->server_pid, SIGUSR1) != 0) {
    fatal_error("Cannot signal the server to enter SCHED_FIFO");
  }
  counter = 500;
  while (sched_getscheduler(prms->server_pid) != SCHED_FIFO
         && counter != 0) {
    usleep(1000);
    counter--;
  }
  if (counter == 0) {
    fatal_error("Server does not enter SCHED_FIFO after 500 ms");
  }

  if (sched_fifo_enter_max(NULL) != 0) {
    fatal_error("Fail to become highest SCHED_FIFO thread to measure"
                " send-receive overhead");
  }

  if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
    fatal_error("Cannot record starting time");
  }

  send_recv(prms->comm_socket, prms->request, prms->len,
            prms->response, prms->len,
            &byte_sent, &byte_rcvd, &send_errno, &recv_errno);

  if (clock_gettime(CLOCK_MONOTONIC, &t2) != 0) {
    fatal_error("Cannot record finishing time");
  }

  if (kill(prms->server_pid, SIGUSR1) != 0) {
    fatal_error("Cannot signal the server to leave SCHED_FIFO");
  }
  counter = 500;
  while (sched_getscheduler(prms->server_pid) == SCHED_FIFO
         && counter != 0) {
    usleep(1000);
    counter--;
  }
  if (counter == 0) {
    fatal_error("Server does not leave SCHED_FIFO after 500 ms");
  }

  prms->overhead = utility_time_sub_dyn_gc(timespec_to_utility_time_dyn(&t2),
                                           timespec_to_utility_time_dyn(&t1));

  if (byte_sent == -1) {
    errno = send_errno;
    fatal_syserror("Cannot send request");
  } else if (byte_sent != prms->len) {
    fatal_error("Sent request is truncated");
  }

  if (byte_rcvd == -1) {
    errno = recv_errno;
    fatal_syserror("Cannot retrieve server response");
  } else if (byte_rcvd != prms->len) {
    fatal_error("Server response is corrupted");
  } else if (memcmp(prms->request, prms->response, prms->len) != 0) {
    fatal_error("Request & server response do not match");
  }

  prms->rc = 0;
  return &prms->rc;
}

static relative_time *measure_send_recv_overhead(int server_pid,
                                                 int comm_socket,
                                                 const void *request,
                                                 void *response,
                                                 size_t len)
{
  pthread_t measurement_thread;
  struct send_recv_overhead_measurement_prms args = {
    .server_pid = server_pid,
    .comm_socket = comm_socket,
    .request = request,
    .response = response,
    .len = len,
  };

  if (pthread_create(&measurement_thread, NULL,
                     send_recv_overhead_measurement_thread, &args) != 0) {
    fatal_error("Cannot create send-receive overhead measurement thread");
  }
  if (pthread_join(measurement_thread, NULL) != 0) {
    fatal_error("Cannot join send-receive overhead measurement thread");
  }

  return args.overhead;
}

struct client_prog_prms {
  pthread_t main_thread;
  cpu_busyloop *prologue_busyloop;
  void *request;
  void *response;
  size_t len;
  cpu_busyloop *epilogue_busyloop;
  int *client_socket_ptr;
  int server_pid;
  int nth_iteration;
  int stopping_iteration;
  int ftrace_iteration;
  FILE *ktrace_file;
  sigset_t send_recv_interrupt_mask;
};
static void client_prog(void *args)
{
  struct client_prog_prms *prms = args;
  int bwi_key;
  int send_errno, recv_errno;
  ssize_t byte_sent, byte_rcvd;

  ++prms->nth_iteration;

  keep_cpu_busy(prms->prologue_busyloop);

  /* BWI */
  if (prms->nth_iteration == prms->ftrace_iteration) {
    fprintf(prms->ktrace_file, "1");
  }
  if (prms->server_pid != -1) {
    if (syscall(344, prms->server_pid, &bwi_key) != 0) {
      fatal_syserror("Cannot perform BWI");
    }
  }  
  /* END: BWI */

  memset(prms->response, 0, prms->len);

  pthread_sigmask(SIG_UNBLOCK, &prms->send_recv_interrupt_mask, NULL);
  send_recv(*prms->client_socket_ptr, prms->request, prms->len,
            prms->response, prms->len, &byte_sent, &byte_rcvd,
            &send_errno, &recv_errno);
  pthread_sigmask(SIG_BLOCK, &prms->send_recv_interrupt_mask, NULL);

  /* BWI revocation */
  if (prms->server_pid != -1) {
    if (syscall(345, bwi_key) != 0) {
      fatal_syserror("Cannot revoke BWI");
    }
  }
  if (prms->nth_iteration == prms->ftrace_iteration) {
    fprintf(prms->ktrace_file, "0");
  }  
  /* END: BWI revocation */

  keep_cpu_busy(prms->epilogue_busyloop);

  if (byte_sent == -1) {
    errno = send_errno;
    if (errno != EINTR) {
      fatal_syserror("Cannot send request");
    }
  } else if (byte_sent != prms->len) {
    log_error("Sent request is truncated");
  }

  if (byte_rcvd == -1) {
    errno = recv_errno;
    if (errno != EINTR) {
      fatal_syserror("Cannot retrieve server response");
    }
  } else if (byte_rcvd != prms->len) {
    log_error("Server response is corrupted");
  } else if (memcmp(prms->request, prms->response, prms->len) != 0) {
    log_error("Request & server response do not match");
  }

  if (prms->nth_iteration == prms->stopping_iteration) {
    pthread_kill(prms->main_thread, SIGTERM);
  }
}

static int client_socket = -1;
static void cleanup(void)
{
  if (client_socket != -1) {
    if (close(client_socket) == -1) {
      log_syserror("Cannot close client socket");
    }
  }
}

static void signal_handler(int signum)
{
}

MAIN_BEGIN("client", "stderr", NULL)
{
  const char *ktrace_path = "/sys/kernel/debug/tracing/tracing_enabled";
  FILE *ktrace_file = NULL;

  if (atexit(cleanup) == -1) {
    log_syserror("Cannot register fn cleanup at exit");
  }

  /* SIGUSR2 is just used to interrupt fn recv to avoid getting stuck */
  {
    struct sigaction act = {
      .sa_handler = signal_handler,
    };
    if (sigaction(SIGUSR2, &act, NULL) != 0) {
      fatal_error("Cannot handle signal SIGUSR2");
    }
  }
  /* END: SIGUSR2 is just used to interrupt fn recv to avoid getting stuck */

  struct sockaddr_in server_addr = {
    .sin_family = AF_INET,
    .sin_addr = {
      .s_addr = htonl(INADDR_LOOPBACK),
    },
  };
  int prologue_duration_ms = -1;
  int epilogue_duration_ms = -1;
  int expected_waiting_ms = -1;
  int period_ms = -1;
  int server_port = -1;
  int server_pid = -1;
  int use_bwi = 0;
  int stopping_iteration = -1;
  int ftrace_iteration = -1;
  const char *stats_file_path = NULL;
  {
    int optchar;
    opterr = 0;
    while ((optchar = getopt(argc, argv, ":hv:s:i:r:1:2:3:t:p:b")) != -1) {
      switch (optchar) {
      case '1':
        prologue_duration_ms = atoi(optarg);
        if (prologue_duration_ms < 0) {
          fatal_error("PROLOGUE_DURATION must be at least 0 (-h for help)");
        }
        break;
      case '2':
        expected_waiting_ms = atoi(optarg);
        if (expected_waiting_ms < 0) {
          fatal_error("EXPECTED_SERVICE_TIME must be at least 0 (-h for help)");
        }
        break;
      case '3':
        epilogue_duration_ms = atoi(optarg);
        if (epilogue_duration_ms < 0) {
          fatal_error("EPILOGUE_DURATION must be at least 0 (-h for help)");
        }
        break;
      case 't':
        period_ms = atoi(optarg);
        if (period_ms < 0) {
          fatal_error("PERIOD must be at least 0 (-h for help)");
        }
        break;
      case 'p':
        server_port = atoi(optarg);
        if (server_port < 1024 || server_port > 65535) {
          fatal_error("PORT must be between 1024 and 65535 (-h for help)");
        }
        server_addr.sin_port = htons(server_port);
        break;
      case 'b':
        use_bwi = 1;
        break;
      case 'v':
        server_pid = atoi(optarg);
        break;
      case 'i':
        stopping_iteration = atoi(optarg);
        break;
      case 'r':
        ftrace_iteration = atoi(optarg);
        break;
      case 's':
        stats_file_path = optarg;
        break;
      case 'h':
        printf("Usage: %s -1 PROLOGUE_DURATION -2 EXPECTED_SERVICE_TIME\n"
               "       -3 EPILOGUE_DURATION -t PERIOD -p SERVER_PORT\n"
               "       -s STATS_FILE_PATH -v SERVER_PID\n"
               "       [-i ITERATION] [-b] [-r NTH_ITERATION]\n"
               "\n"
               "This client periodically sends a message to a local UDP port.\n"
               "In each period, the client will do processing for the given\n"
               "prologue duration before sending a request to the server.\n"
               "After sending the request, this client will block waiting\n"
               "for the response.\n"
               "After receiving a response from the server, the client will\n"
               "do processing for the given epilogue duration before blocking\n"
               "waiting for the next period. SIGTERM should be used to\n"
               "graciously terminate this program if option -i is not given.\n"
               "Signal SIGUSR1 must be sent to this program to start the\n"
               "prologue-service-epilogue cycle and before sending SIGTERM.\n"
               "If SIGTERM is sent before SIGUSR1, this program will SEGFAULT."
               "\n\n"
               "-1 PROLOGUE_DURATION is the duration in millisecond by which\n"
               "   this client will keep the CPU busy before sending a\n"
               "   request to the server.\n"
               "-2 EXPECTED_SERVICE_TIME is the expected duration in\n"
               "   millisecond after this client sends a request to the\n"
               "   server and before this client receives the response.\n"
               "-3 EPILOGUE_DURATION is the duration in millisecond by which\n"
               "   this client will keep the CPU busy before blocking\n"
               "   waiting for the next period.\n"
               "-t PERIOD is the period in millisecond.\n"
               "-p SERVER_PORT is the server UDP port number at which the\n"
               "   server is receiving requests.\n"
               "-s STATS_FILE_PATH is the path to the file to store the\n"
               "   real-time task statistics.\n"
               "-v SERVER_PID is the PID of the server for the purpose of\n"
               "   measuring communication overhead and performing BWI.\n"
               "-i ITERATION is the number of prologue-service-epilogue\n"
               "   cycles to be performed before this client exits.\n"
               "-b is specified when this client should give its bandwidth\n"
               "   to the server (the kernel must support BWI syscalls).\n"
               "-r NTH_ITERATION is used to start ktrace at the beginning of\n"
               "   the n-th period and to stop ktrace at the end of the\n"
               "   epilogue in that period. This is useful for debugging\n"
               "   kernel BWI timing by analyzing ftrace output.\n",
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
  if (prologue_duration_ms == -1) {
    fatal_error("-1 must be specified (-h for help)");
  }
  if (expected_waiting_ms == -1) {
    fatal_error("-2 must be specified (-h for help)");
  }
  if (epilogue_duration_ms == -1) {
    fatal_error("-3 must be specified (-h for help)");
  }
  if (period_ms == -1) {
    fatal_error("-t must be specified (-h for help)");
  }
  if (prologue_duration_ms + expected_waiting_ms + epilogue_duration_ms
      > period_ms) {
    fatal_error("PERIOD is too short to perform prologue-service-epilogue");
  }
  if (server_port == -1) {
    fatal_error("-p must be specified (-h for help)");
  }
  if (stats_file_path == NULL) {
    fatal_error("-s must be specified (-h for help)");
  }
  if (server_pid == -1) {
    fatal_error("-v must be specified (-h for help)");
  }

  /* Prepare for tracing */
  if (ftrace_iteration != -1) {
    ktrace_file = utility_file_open_for_writing(ktrace_path);
    if (ktrace_file == NULL) {
      fatal_error("Cannot open ktrace file");
    }
    errno = 0;
    if (setvbuf(ktrace_file, NULL, _IONBF, 0) != 0) {
      if (errno) {
        fatal_syserror("Cannot unbuffer ktrace_file");
      } else {
        fatal_error("Cannot unbuffer ktrace_file");
      }
    }
  }
  /* END: Prepare for tracing */

  /* Prepare UDP connection */
  {
    client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket == -1) {
      fatal_syserror("Cannot create client UDP socket");
    }
  
    if (connect(client_socket,
                (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
      fatal_syserror("Cannot set default destination to the server address");
    }
  }
  /* END: Prepare UDP connection */

  /* Measure overhead */
  char message_buf[] = "Hello!";
  char response_buf[sizeof(message_buf)];
  relative_time *job_stats_overhead;
  relative_time *task_overhead;
  relative_time *overhead = to_utility_time_dyn(0, ms);
  utility_time_set_gc_manual(overhead);
  {
    char t_str[32];

    /* Job statistics overhead */
    if (job_statistics_overhead(0, &job_stats_overhead) != 0) {
      fatal_error("Cannot obtain job statistics overhead");
    }
    utility_time_set_gc_manual(job_stats_overhead);
    to_string(job_stats_overhead, t_str, sizeof(t_str));
    printf("job_stats_overhead: %s\n", t_str);    
    /* END: Job statistics overhead */    

    /* Task overhead */
    if (finish_to_start_overhead(0, 0, &task_overhead) != 0) {
      fatal_error("Cannot obtain finish to start overhead");
    }
    utility_time_set_gc_manual(task_overhead);
    to_string(task_overhead, t_str, sizeof(t_str));
    printf("     task_overhead: %s\n", t_str);    
    /* END: Task overhead */

    /* Communication overhead */
    relative_time *comm_overhead, *comm_real;
    comm_overhead = measure_send_recv_overhead(server_pid, client_socket,
                                               message_buf, response_buf,
                                               sizeof(message_buf));
    comm_real = to_utility_time_dyn(expected_waiting_ms, ms);
    comm_overhead = utility_time_sub_dyn_gc(comm_overhead, comm_real);
    to_string(comm_overhead, t_str, sizeof(t_str));
    printf("     comm_overhead: %s\n", t_str);
    /* END: Communication overhead */

    /* Sum all overheads up */
    utility_time_inc_gc(overhead, job_stats_overhead);
    utility_time_inc_gc(overhead, task_overhead);
    utility_time_inc_gc(overhead, comm_overhead);
    /* END: Sum all overheads up */
  }
  /* END: Measure overhead */

  /* Prepare busyloops */
  cpu_busyloop *prologue_busyloop = NULL;
  cpu_busyloop *epilogue_busyloop = NULL;
  {
    int search_tolerance_us = 100;
    int search_passes = 10;
    int rc;
    utility_time *prologue = to_utility_time_dyn(prologue_duration_ms, ms);
    utility_time *epilogue = to_utility_time_dyn(epilogue_duration_ms, ms);

    if (utility_time_gt(epilogue, overhead)) {
      epilogue = utility_time_sub_dyn_gc(epilogue, overhead);
      log_verbose("All overheads are charged to epilogue\n");
    } else if (utility_time_gt(prologue, overhead)) {
      prologue = utility_time_sub_dyn_gc(prologue, overhead);
      log_verbose("All overheads are charged to prologue\n");
    } else {
      fatal_error("Both prologue and epilogue cannot accommodate all overheads"
                  );
    }

    rc = create_cpu_busyloop(0, prologue,
                             to_utility_time_dyn(search_tolerance_us, us),
                             search_passes,
                             &prologue_busyloop);
    if (rc == -2) {
      fatal_error("%d is too small for prologue busyloop",
                  prologue_duration_ms);
    } else if (rc == -4) {
      fatal_error("%d is too big for prologue busyloop", prologue_duration_ms);
    } else if (rc != 0) {
      fatal_error("Cannot create prologue busyloop");
    }

    rc = create_cpu_busyloop(0, epilogue,
                             to_utility_time_dyn(search_tolerance_us, us),
                             search_passes,
                             &epilogue_busyloop);
    if (rc == -2) {
      fatal_error("%d is too small for epilogue busyloop",
                  epilogue_duration_ms);
    } else if (rc == -4) {
      fatal_error("%d is too big for epilogue busyloop", epilogue_duration_ms);
    } else if (rc != 0) {
      fatal_error("Cannot create epilogue busyloop");
    }
  }
  /* END: Prepare busyloops */

  /* Prepare client task */
  int wcet_ms = (prologue_duration_ms + expected_waiting_ms
                 + epilogue_duration_ms);
  struct client_prog_prms client_prog_args = {
    .main_thread = pthread_self(),
    .prologue_busyloop = prologue_busyloop,
    .request = message_buf,
    .response = response_buf,
    .len = sizeof(message_buf),
    .epilogue_busyloop = epilogue_busyloop,
    .client_socket_ptr = &client_socket,
    .server_pid = use_bwi ? server_pid : -1,
    .nth_iteration = 0,
    .stopping_iteration = stopping_iteration,
    .ftrace_iteration = ftrace_iteration,
    .ktrace_file = ktrace_file,
  };
  sigemptyset(&client_prog_args.send_recv_interrupt_mask);
  sigaddset(&client_prog_args.send_recv_interrupt_mask, SIGUSR2);
  /* END: Prepare client task */

  /* Launching client task */
  sigset_t waited_signal;
  if (sigemptyset(&waited_signal) != 0) {
    log_syserror("Cannot empty waited_signal");
  }
  if (sigaddset(&waited_signal, SIGTERM) != 0) {
    log_syserror("Cannot add SIGTERM to waited_signal");
  }
  if (sigaddset(&waited_signal, SIGUSR1) != 0) {
    log_syserror("Cannot add SIGUSR1 to waited_signal");
  }
  pthread_sigmask(SIG_BLOCK, &waited_signal, NULL);

  pthread_t client_tid;
  struct client_thread_prms client_thread_args = {
    .wcet_ms = wcet_ms,
    .period_ms = period_ms,
    .stats_file_path = stats_file_path,
    .client_prog_args = &client_prog_args,
    .job_stats_overhead = job_stats_overhead,
    .task_overhead = task_overhead,
    .client_task = NULL,
  };
  if ((errno = pthread_create(&client_tid, NULL,
                              client_thread, &client_thread_args)) != 0) {
    fatal_syserror("Cannot create client thread");
  }

  int rcvd_signo;
  if (sigdelset(&waited_signal, SIGUSR1) != 0) {
    log_syserror("Cannot delete SIGUSR1 from waited_signal");
  }
  sigwait(&waited_signal, &rcvd_signo);
  pthread_sigmask(SIG_UNBLOCK, &waited_signal, NULL);

  task_stop(client_thread_args.client_task);

  /* Ensure that client_task does not get stuck at fn recv */
  pthread_kill(client_tid, SIGUSR2);
  /* END: Ensure that client_task does not get stuck at fn recv */

  if ((errno = pthread_join(client_tid, NULL)) != 0) {
    fatal_syserror("Cannot join client thread");
  }

  if (client_thread_args.rc != EXIT_SUCCESS) {
    fatal_error("Cannot execute client thread successfully");
  }
  /* END: Launching client task */

  /* Clean-up */
  task_destroy(client_thread_args.client_task);
  destroy_cpu_busyloop(epilogue_busyloop);
  destroy_cpu_busyloop(prologue_busyloop);
  utility_time_gc(job_stats_overhead);
  utility_time_gc(task_overhead);
  utility_time_gc(overhead);
  if (ktrace_file != NULL) {
    utility_file_close(ktrace_file, ktrace_path);
  }
  /* END: Clean-up */

  return EXIT_SUCCESS;

} MAIN_END
