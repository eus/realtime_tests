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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "../utility_experimentation.h"
#include "../utility_log.h"
#include "../utility_cpu.h"
#include "../utility_time.h"
#include "../utility_sched.h"
#include "../utility_sched_fifo.h"
#include "../utility_sched_deadline.h"

static volatile int terminated = 0;
static int old_scheduler_set = 0;
static struct scheduler old_scheduler;
static int subserver_pid = -1;

static void sighand(int signo)
{
  if (signo == SIGTERM) {
    terminated = 1;
  }
  else if (signo == SIGUSR1) {
    if (old_scheduler_set) {

      /* Signal subserver as necessary */
      if (subserver_pid != -1) {
        int counter;
        if (kill(subserver_pid, SIGUSR1) != 0) {
          fatal_error("Cannot signal subserver %d to leave SCHED_FIFO",
                      subserver_pid);
        }
        counter = 500;
        while (sched_getscheduler(subserver_pid) == SCHED_FIFO
               && counter != 0) {
          usleep(1000);
          counter--;
        }
        if (counter == 0) {
          fatal_error("Subserver %d does not leave SCHED_FIFO after 500 ms",
                      subserver_pid);
        }
      }
      /* END: Signal subserver as necessary */

      /* Leave SCHED_FIFO */
      if (sched_fifo_leave(&old_scheduler) == 0) {
        log_verbose("SCHED_FIFO is turned off\n");
        old_scheduler_set = 0;  
      } else {
        log_error("Cannot leave SCHED_FIFO");
      }      
      /* END: Leave SCHED_FIFO */
    } else {
      /* Signal subserver as necessary */
      if (subserver_pid != -1) {
        int counter;

        if (kill(subserver_pid, SIGUSR1) != 0) {
          fatal_error("Cannot signal subserver %d to enter SCHED_FIFO",
                      subserver_pid);
        }
        counter = 500;
        while (sched_getscheduler(subserver_pid) != SCHED_FIFO
               && counter != 0) {
          usleep(1000);
          counter--;
        }
        if (counter == 0) {
          fatal_error("Subserver %d does not enter SCHED_FIFO after 500 ms",
                      subserver_pid);
        }
      }      
      /* END: Signal subserver as necessary */

      /* Enter SCHED_FIFO */
      if (sched_fifo_enter_max(&old_scheduler) == 0) {
        log_verbose("SCHED_FIFO is turned on\n");
        old_scheduler_set = 1;
      } else {
        log_error("Cannot enter SCHED_FIFO");
      }      
      /* END: Enter SCHED_FIFO */
    }
  }
}

static int server_socket = -1;
static int subserver_socket = -1;
static void cleanup(void)
{
  if (server_socket != -1) {
    if (close(server_socket) == -1) {
      log_syserror("Cannot close server socket");
    }
  }
  if (subserver_socket != -1) {
    if (close(subserver_socket) == -1) {
      log_syserror("Cannot close subserver socket");
    }
  }
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

  if (*byte_sent == -1) {
    *byte_rcvd = 0;
    *recv_errno = 0;
    return;
  }

  memset(buffer, 0, buf_len);

  *byte_rcvd = recv(comm_socket, buffer, buf_len, 0);
  if (*byte_rcvd == -1) {
    *recv_errno = errno;
  } else {
    *recv_errno = 0;
  }
}

static int subserver_send_recv(int subserver_pid, int subserver_socket,
                               void *buffer, size_t buffer_size,
                               ssize_t *packet_size)
{
  int send_errno, recv_errno, bwi_key, bwi_active = 0;
  ssize_t byte_sent, byte_rcvd;

  /* BWI */
  if (subserver_pid != -1) {
    if (syscall(344, subserver_pid, &bwi_key) != 0) {
      if (errno != EAGAIN) {
        log_syserror("Cannot perform BWI");
      }
    } else {
      bwi_active = 1;
    }
  }  
  /* END: BWI */

  send_recv(subserver_socket, buffer, *packet_size, buffer, buffer_size,
            &byte_sent, &byte_rcvd, &send_errno, &recv_errno);

  /* BWI revocation */
  if (subserver_pid != -1 && bwi_active) {
    if (syscall(345, bwi_key) != 0) {
      log_syserror("Cannot revoke BWI");
    }
  }
  /* END: BWI revocation */

  if (byte_sent == -1) {
    if (send_errno != EINTR) {
      log_syserror("Cannot forward request to subserver %d", subserver_pid);
      return 0;
    }
    return 1;
  }
  if (byte_sent != *packet_size) {
    log_error("Forwarded packet to subserver %d is truncated", subserver_pid);
  }

  if (byte_rcvd == -1) {
    if (recv_errno != EINTR) {
      log_syserror("Cannot retrieve response from subserver %d", subserver_pid);
      return 0;
    }
    return 1;
  }

  *packet_size = byte_rcvd;
  return 0;
}

MAIN_BEGIN("server", "stderr", NULL)
{
  if (atexit(cleanup) == -1) {
    fatal_syserror("Cannot register fn cleanup at exit");
  }

  struct sigaction sighandler = {
    .sa_handler = sighand,
  };
  if (sigaction(SIGTERM, &sighandler, NULL) != 0) {
    fatal_syserror("Cannot install SIGTERM handler");
  }
  if (sigaction(SIGUSR1, &sighandler, NULL) != 0) {
    fatal_syserror("Cannot install SIGUSR1 handler");
  }

  struct sockaddr_in server_addr = {
    .sin_family = AF_INET,
    .sin_addr = {
      .s_addr = htonl(INADDR_LOOPBACK),
    },
  };
  struct sockaddr_in subserver_addr = {
    .sin_family = AF_INET,
    .sin_addr = {
      .s_addr = htonl(INADDR_LOOPBACK),
    },
  };
  int processing_duration_ms = -1;
  int server_port = -1;
  int cbs_budget_ms = -1;
  int cbs_period_ms = -1;
  int subserver_port = -1;
  {
    int optchar;
    opterr = 0;
    while ((optchar = getopt(argc, argv, ":hd:p:q:t:s:v:")) != -1) {
      switch (optchar) {
      case 'd':
        processing_duration_ms = atoi(optarg);
        if (processing_duration_ms < 0) {
          fatal_error("SERVING_DURATION must be at least 0 (-h for help)");
        }
        break;
      case 'p':
        server_port = atoi(optarg);
        if (server_port < 1024 || server_port > 65535) {
          fatal_error("PORT must be between 1024 and 65535 (-h for help)");
        }
        server_addr.sin_port = htons(server_port);
        break;
      case 's':
        subserver_port = atoi(optarg);
        if (subserver_port < 1024 || subserver_port > 65535) {
          fatal_error("SUBSERVER_PORT must be between 1024 and 65535"
                      " (-h for help)");
        }
        subserver_addr.sin_port = htons(subserver_port);
        break;
      case 'v':
        subserver_pid = atoi(optarg);
        if (kill(subserver_pid, 0) != 0) {
          switch (errno) {
          case EPERM:
            fatal_error("SUBSERVER_PID cannot be signaled due to insufficient"
                        " privilege");
          case ESRCH:
            fatal_error("SUBSERVER_PID does not exist in the system");
          }
        }
        break;
      case 'q':
        cbs_budget_ms = atoi(optarg);
        break;
      case 't':
        cbs_period_ms = atoi(optarg);
        break;
      case 'h':
        printf("Usage: %s -d SERVING_DURATION -p PORT\n"
               "       [-s SUBSERVER_PORT -v SUBSERVER_PID]\n"
               "       [-q CBS_BUDGET -t CBS_PERIOD]\n"
               "\n"
               "This server listens on a local UDP port. Upon receiving a UDP\n"
               "packet at the port, this server will run for the specified\n"
               "duration without blocking before sending the client-sent data\n"
               "back to the client. This behavior changes when -s is used.\n"
               "When -s is used, after running for the specified duration\n"
               "without blocking, the UDP packet will be forwarded to the\n"
               "given subserver port. This server then blocks waiting for a\n"
               "response from the subserver. Once the response is received,\n"
               "it is immediately sent to the client.\n"
               "SIGTERM should be used to graciously terminate this program.\n"
               "Optionally, this server can be run using a CBS by specifying\n"
               "both the CBS budget and period.\n"
               "\n"
               "-d SERVING_DURATION is the duration in millisecond by which\n"
               "   this server will keep the CPU busy after receiving\n"
               "   a client request but before returning the response, or\n"
               "   before forwarding the client packet to the subserver if -s\n"
               "   is given.\n"
               "-p PORT is the server UDP port number at which the server\n"
               "   is receiving UDP packets. This port should be unprivileged\n"
               "   to avoid unnecessary security problem.\n"
               "-s SUBSERVER_PORT is a subserver UDP port number that any\n"
               "   received UDP packet will be forwarded to.\n"
               "-v SUBSERVER_PID is the PID of the subserver for the purpose\n"
               "   of measuring communication overhead and performing BWI.\n"
               "-q CBS_BUDGET is the budget in millisecond of the CBS that\n"
               "   runs this server.\n"
               "-t CBS_PERIOD is the period in millisecond of the CBS that\n"
               "   runs this server.\n",
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
  if (processing_duration_ms == -1) {
    fatal_error("-d must be specified (-h for help)");
  }
  if (server_port == -1) {
    fatal_error("-p must be specified (-h for help)");
  }
  if ((cbs_budget_ms == -1 && cbs_period_ms != -1)
      || (cbs_budget_ms != -1 && cbs_period_ms == -1)) {
    fatal_error("Both -q and -t must be specified (-h for help)");
  }
  if (cbs_budget_ms != -1 && cbs_period_ms != -1) {
    if (cbs_budget_ms <= 0) {
      fatal_error("CBS_BUDGET must be at least 1 (-h for help)");
    }
    if (cbs_budget_ms > cbs_period_ms) {
      fatal_error("CBS_PERIOD must be greater than or equal to CBS_BUDGET"
                  " (-h for help)");
    }
  }
  if (subserver_port != -1 && subserver_pid == -1) {
    fatal_error("SUBSERVER_PID must be specified together with SUBSERVER_PORT"
                " (-h for help)");
  }

  /* Prepare server busyloop */
  cpu_busyloop *server_busyloop = NULL;
  {
    int search_tolerance_us = 100;
    int search_passes = 10;
    int rc = create_cpu_busyloop(0,
                                 to_utility_time_dyn(processing_duration_ms,
                                                     ms),
                                 to_utility_time_dyn(search_tolerance_us, us),
                                 search_passes,
                                 &server_busyloop);
    if (rc == -2) {
      fatal_error("%d is too small for server busyloop",
                  processing_duration_ms);
    } else if (rc == -4) {
      fatal_error("%d is too big for server busyloop", processing_duration_ms);
    } else if (rc != 0) {
      fatal_error("Cannot create server busyloop");
    }
  }
  /* END: Prepare server busyloop */

  /* Prepare UDP connection */
  {
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket == -1) {
      fatal_syserror("Cannot create server UDP socket");
    }
  
    if (bind(server_socket,
             (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
      fatal_syserror("Cannot name server UDP socket");
    }
  }
  /* END: Prepare UDP connection */

  /* Prepare UDP connection to subserver as necessary */
  if (subserver_port != -1) {
    subserver_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (subserver_socket == -1) {
      fatal_syserror("Cannot create subserver %d UDP socket", subserver_pid);
    }
  
    if (connect(subserver_socket, (struct sockaddr *) &subserver_addr,
                sizeof(subserver_addr)) == -1) {
      fatal_syserror("Cannot set default destination to subserver %d address",
                     subserver_pid);
    }
  }
  /* END: Prepare UDP connection to subserver as necessary */

  /* Use CBS if requested */
  if (cbs_budget_ms != -1 && cbs_period_ms != -1) {
    if (sched_deadline_enter(to_utility_time_dyn(cbs_budget_ms, ms),
                             to_utility_time_dyn(cbs_period_ms, ms),
                             NULL) != 0) {
      fatal_error("Cannot use CBS (privilege may be insufficient)");
    }
  }
  /* END: Use CBS if requested */

  /* Serve incoming client UDP packet */
  while (!terminated) {
    struct sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    char buffer[(1 << 16) - 1];
    ssize_t packet_size, response_size;

    memset(buffer, 0, sizeof(buffer));

    /* Accept incoming request */
    packet_size = recvfrom(server_socket, buffer, sizeof(buffer), 0,
                           (struct sockaddr *) &src_addr, &src_addr_len);
    if (packet_size == -1) {
      if (errno != EINTR) {
        log_syserror("Cannot retrieve incoming UDP message");
      }
      continue;
    }
    if (src_addr_len != sizeof(struct sockaddr_in)) {
      log_error("Sender address is not IPv4; cannot response");
      continue;
    }
    /* END: Accept incoming request */

    keep_cpu_busy(server_busyloop);

    /* Send & receive the request to & from the subserver as necessary */
    if (subserver_port != -1) {
      if (subserver_send_recv(subserver_pid, subserver_socket,
                              buffer, sizeof(buffer), &packet_size) != 0) {
        continue;
      }
    }
    /* END: Send & receive the request to & from the subserver as necessary */

    /* Send back a response */
    response_size = sendto(server_socket, buffer, packet_size, 0,
                           (struct sockaddr *) &src_addr, src_addr_len);
    if (response_size == -1) {
      log_syserror("Cannot send back response");
      continue;
    }
    if (response_size != packet_size) {
      log_error("Sent response is truncated");
      continue;
    }
    /* END: Send back a response */
  }  
  /* END: Serve incoming client UDP packet */

  destroy_cpu_busyloop(server_busyloop);

  return EXIT_SUCCESS;

} MAIN_END
