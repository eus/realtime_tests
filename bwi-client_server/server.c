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

static void sighand(int signo)
{
  if (signo == SIGTERM) {
    terminated = 1;
  }
  else if (signo == SIGUSR1) {
    if (old_scheduler_set) {
      if (sched_fifo_leave(&old_scheduler) == 0) {
        log_verbose("SCHED_FIFO is turned off\n");
        old_scheduler_set = 0;  
      } else {
        log_error("Cannot leave SCHED_FIFO");
      }
    } else {
      if (sched_fifo_enter_max(&old_scheduler) == 0) {
        log_verbose("SCHED_FIFO is turned on\n");
        old_scheduler_set = 1;
      } else {
        log_error("Cannot enter SCHED_FIFO");
      }
    }
  }
}

static int server_socket = -1;
static void cleanup(void)
{
  if (server_socket != -1) {
    if (close(server_socket) == -1) {
      log_syserror("Cannot close server socket");
    }
  }
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
  int processing_duration_ms = -1;
  int server_port = -1;
  int cbs_budget_ms = -1;
  int cbs_period_ms = -1;
  {
    int optchar;
    opterr = 0;
    while ((optchar = getopt(argc, argv, ":hd:p:q:t:")) != -1) {
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
      case 'q':
        cbs_budget_ms = atoi(optarg);
        break;
      case 't':
        cbs_period_ms = atoi(optarg);
        break;
      case 'h':
        printf("Usage: %s -d SERVING_DURATION -p PORT\n"
               "       [-q CBS_BUDGET -t CBS_PERIOD]\n"
               "\n"
               "This server listens on a local UDP port. Upon receiving a UDP\n"
               "packet at the port, this server will run for the specified\n"
               "duration without blocking before sending the client-sent data\n"
               "back to the client. SIGTERM should be used to graciously\n"
               "terminate this program. Optionally, this server can be run\n"
               "using a CBS by specifying both the CBS budget and period.\n"
               "\n"
               "-d SERVING_DURATION is the duration in millisecond by which\n"
               "   this server will keep the CPU busy after receiving\n"
               "   a client request but before returning the response.\n"
               "-p PORT is the server UDP port number at which the server\n"
               "   is receiving UDP packets. This port should be unprivileged\n"
               "   to avoid unnecessary security problem.\n"
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
    ssize_t packet_size;

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

    /* Send back a response */
    ssize_t response_size = sendto(server_socket, buffer, packet_size, 0,
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
