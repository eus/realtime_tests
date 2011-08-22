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

#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "../utility_experimentation.h"
#include "../utility_log.h"
#include "../utility_sched_deadline.h"
#include "../utility_subprocess.h"

MAIN_BEGIN("SCHED_DEADLINE-and-special-kthreads", "stderr", NULL)
{
  pid_t s1 = -1, s2 = -1;
  int s1_pipe[2], s2_pipe[2];
  char buffer[32], buffer_2[32];
  int rc = EXIT_SUCCESS;

  /* Phase 1 */
  if (pipe(s1_pipe) != 0) {
    fatal_syserror("Cannot pipe CBS S1 in the first phase");
  }
  fork_proc(s1) {
    if (close(s1_pipe[0]) != 0) {
      fatal_syserror("Cannot close s1_pipe[READ_PIPE]");
    }
    if (dup2(s1_pipe[1], 1) == -1) {
      fatal_syserror("Cannot redirect stdout to s1_pipe[WRITE_PIPE]");
    }
    if (dup2(open("/dev/null", O_WRONLY), 2) == -1) {
      fatal_syserror("Cannot redirect stderr to /dev/null");
    }
    exec_proc("./cpu_hog_cbs", make_opt("e", "9"), make_opt("b", "1"),
              make_opt("q", "10"), make_opt("t", "30"), make_opt("s", "3000"));
  }
  if (close(s1_pipe[1]) != 0) {
    fatal_syserror("Cannot close s1_pipe[WRITE_PIPE]");
  }

  sleep(1);
  rc = kill_proc(s1, SIGUSR1) ? EXIT_FAILURE : rc;
  rc = wait_proc(s1) ? EXIT_FAILURE : rc;

  memset(buffer, 0, sizeof(buffer));
  if (read(s1_pipe[0], buffer, sizeof(buffer) - 1) == -1) {
    fatal_error("Cannot read cycle count from s1_pipe");
  }
  if (close(s1_pipe[0]) != 0) {
    fatal_syserror("Cannot close s1_pipe[READ_PIPE]");
  }
  *strchr(buffer, '\n') = '\0';
  printf("1. Cycle count (S1): %s\n", buffer);
  /* END: Phase 1 */

  /* Phase 2 */
  if (pipe(s1_pipe) != 0) {
    fatal_syserror("Cannot pipe CBS S1 in the second phase");
  }
  if (pipe(s2_pipe) != 0) {
    fatal_syserror("Cannot pipe CBS S2 in the second phase");
  }
  fork_proc(s1) {
    if (close(s1_pipe[0]) != 0) {
      fatal_syserror("Cannot close s1_pipe[READ_PIPE]");
    }
    if (dup2(s1_pipe[1], 1) == -1) {
      fatal_syserror("Cannot redirect stdout to s1_pipe[WRITE_PIPE]");
    }
    if (dup2(open("/dev/null", O_WRONLY), 2) == -1) {
      fatal_syserror("Cannot redirect stderr to /dev/null");
    }
    exec_proc("./cpu_hog_cbs", make_opt("e", "9"), make_opt("b", "1"),
              make_opt("q", "10"), make_opt("t", "30"), make_opt("s", "3000"));
  }
  if (close(s1_pipe[1]) != 0) {
    fatal_syserror("Cannot close s1_pipe[WRITE_PIPE]");
  }

  fork_proc(s2) {
    if (close(s2_pipe[0]) != 0) {
      fatal_syserror("Cannot close s2_pipe[READ_PIPE]");
    }
    if (dup2(s2_pipe[1], 1) == -1) {
      fatal_syserror("Cannot redirect stdout to s2_pipe[WRITE_PIPE]");
    }
    if (dup2(open("/dev/null", O_WRONLY), 2) == -1) {
      fatal_syserror("Cannot redirect stderr to /dev/null");
    }
    exec_proc("./cpu_hog_cbs", make_opt("e", "50"), make_opt("b", "0"),
              make_opt("q", "10"), make_opt("t", "30"), make_opt("s", "3000"));
  }
  if (close(s2_pipe[1]) != 0) {
    fatal_syserror("Cannot close s2_pipe[WRITE_PIPE]");
  }

  sleep(1);
  struct scheduler old_scheduler;
  if (sched_deadline_enter(to_utility_time_dyn(20, ms),
                           to_utility_time_dyn(20, ms),
                           &old_scheduler) != 0) {
    fatal_error("Cannot enter SCHED_DEADLINE (privilege may be insufficient)"
                );
  }
  rc = kill_proc(s1, SIGUSR1) ? EXIT_FAILURE : rc;
  rc = kill_proc(s2, SIGUSR1) ? EXIT_FAILURE : rc;
  sched_deadline_leave(&old_scheduler);
  rc = wait_proc(s1) ? EXIT_FAILURE : rc;
  rc = wait_proc(s2) ? EXIT_FAILURE : rc;

  memset(buffer, 0, sizeof(buffer));
  if (read(s1_pipe[0], buffer, sizeof(buffer) - 1) == -1) {
    fatal_error("Cannot read cycle count from s1_pipe");
  }
  if (close(s1_pipe[0]) != 0) {
    fatal_syserror("Cannot close s1_pipe[READ_PIPE]");
  }
  *strchr(buffer, '\n') = '\0';

  memset(buffer_2, 0, sizeof(buffer_2));
  if (read(s2_pipe[0], buffer_2, sizeof(buffer_2) - 1) == -1) {
    fatal_error("Cannot read cycle count from s2_pipe");
  }
  if (close(s2_pipe[0]) != 0) {
    fatal_syserror("Cannot close s2_pipe[READ_PIPE]");
  }
  *strchr(buffer_2, '\n') = '\0';

  printf("2. Cycle count (S1, S2): %s %s\n", buffer, buffer_2);  
  /* END: Phase 2 */

  return rc;

} MAIN_END
