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
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include <signal.h>
#include "../utility_experimentation.h"
#include "../utility_log.h"

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

static int client_socket = -1;
static void cleanup(void)
{
  if (client_socket != -1) {
    if (close(client_socket) == -1) {
      log_syserror("Cannot close client socket");
    }
  }
}

static int counter = 0;
static volatile int terminated = 0;
static void signal_handler(int signum)
{
  if (signum == SIGUSR1) {
    printf("%d\n", counter);
  } else if (signum == SIGTERM) {
    terminated = 1;
  }
}

MAIN_BEGIN("client", "stderr", NULL)
{
  if (atexit(cleanup) == -1) {
    fatal_syserror("Cannot register fn cleanup at exit");
  }

  struct sigaction sigact = {
    .sa_handler = signal_handler,
  };
  if (sigaction(SIGUSR1, &sigact, NULL) != 0) {
    fatal_syserror("Cannot install SIGUSR1 handler");
  }
  if (sigaction(SIGTERM, &sigact, NULL) != 0) {
    fatal_syserror("Cannot install SIGTERM handler");
  }

  struct sockaddr_in server_addr = {
    .sin_family = AF_INET,
    .sin_addr = {
      .s_addr = htonl(INADDR_LOOPBACK),
    },
  };
  int server_port = -1;
  {
    int optchar;
    opterr = 0;
    while ((optchar = getopt(argc, argv, ":hp:")) != -1) {
      switch (optchar) {
      case 'p':
        server_port = atoi(optarg);
        if (server_port < 1024 || server_port > 65535) {
          fatal_error("PORT must be between 1024 and 65535 (-h for help)");
        }
        server_addr.sin_port = htons(server_port);
        break;
      case 'h':
        printf("Usage: %s -p SERVER_PORT\n"
               "\n"
               "This client sends a message to a local UDP port, waits for\n"
               "the reply and restart the cycle until graciously killed by\n"
               "SIGTERM. To inquire the number of response received so far,\n"
               "send signal SIGUSR1.\n"
               "\n"
               "-p SERVER_PORT is the server UDP port number at which the\n"
               "   server is receiving requests.\n",
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
  if (server_port == -1) {
    fatal_error("-p must be specified (-h for help)");
  }

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

  char message_buf[] = "Hello! I am server hogger!";
  char response_buf[sizeof(message_buf)];
  while (!terminated) {
    ssize_t byte_sent, byte_rcvd;
    int send_errno, recv_errno;

    memset(response_buf, 0, sizeof(response_buf));

    send_recv(client_socket, message_buf, sizeof(message_buf),
              response_buf, sizeof(response_buf), &byte_sent, &byte_rcvd,
              &send_errno, &recv_errno);

    if (byte_sent == -1) {
      errno = send_errno;
      if (errno != EINTR) {
        fatal_syserror("Cannot send request");
      }
    } else if (byte_sent != sizeof(message_buf)) {
      fatal_error("Sent request is truncated");
    }

    if (byte_rcvd == -1) {
      errno = recv_errno;
      if (errno != EINTR) {
        fatal_syserror("Cannot retrieve server response");
      }
    } else if (byte_rcvd != sizeof(response_buf)) {
      fatal_error("Server response is corrupted");
    } else if (memcmp(message_buf, response_buf, sizeof(message_buf)) != 0) {
      fatal_error("Request & server response do not match");
    }

    counter++;
  }

  return EXIT_SUCCESS;

} MAIN_END
