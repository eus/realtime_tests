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

#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "../utility_experimentation.h"
#include "../utility_log.h"
#include "../utility_subprocess.h"

MAIN_BEGIN("bwi-client_server-7", "stderr", NULL)
{
  pid_t cpu_hog_proc = -1, server_proc = -1, client_proc = -1;
  char server_pid[32];
  int rc = EXIT_SUCCESS;

  fork_proc(server_proc) {
    exec_proc("./server", make_opt("d", "9"), make_opt("p", "7777"),
              make_opt("q", "4"), make_opt("t", "30"));
  }
  snprintf(server_pid, sizeof(server_pid), "%d", server_proc);
  sleep(1);

  fork_proc(cpu_hog_proc) {
    exec_proc("./cpu_hog");
  }
  sleep(1);

  fork_proc(client_proc) {
    exec_proc("./client", make_opt("1", "5"), make_opt("2", "9"),
              make_opt("3", "5"), make_opt("t", "30"), make_opt("p", "7777"),
              make_opt("s", "subexperiment_07.bin"), make_opt("v", server_pid),
              make_opt("q", "20"));
  }
  sleep(1);

  rc = kill_proc(client_proc, SIGUSR1) ? EXIT_FAILURE : rc;

  sleep(60);

  rc = kill_and_wait_proc(client_proc, SIGTERM) ? EXIT_FAILURE : rc;
  rc = kill_and_wait_proc(cpu_hog_proc, SIGTERM) ? EXIT_FAILURE : rc;
  rc = kill_and_wait_proc(server_proc, SIGTERM) ? EXIT_FAILURE : rc;

  return rc;

} MAIN_END
