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

/**
 * @file utility_subprocess.h
 * @brief Various functionalities to conveniently start and end subprocesses.
 * @author Tadeus Prastowo <eus@member.fsf.org>
 */

#ifndef UTILITY_SUBPROCESS
#define UTILITY_SUBPROCESS

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "utility_log.h"

/**
 * Fork a process and store the child ID in id. If forking fails, this
 * macro will call function exit.
 *
 * @param id the variable to store the child process ID.
 */
#define fork_proc(id)                           \
  id = fork();                                  \
  if (id == -1) {                               \
    fatal_syserror("Cannot fork to have " #id); \
  } else if (id == 0)

/**
 * Replace the image of a process with the given executable. If exec
 * fails, this macro will call function exit.
 *
 * @param filename the path of the executable.
 * @param ... the options given to the executable.
 */
#define exec_proc(filename, ...) do {                   \
    char *argv[] = { filename, ## __VA_ARGS__ , NULL }; \
    execv(filename, argv);                              \
    fatal_syserror("Cannot execute " #filename);        \
  } while (0)

/**
 * Specify an option character and the corresponding argument.
 * If the option has no argument, set the argument to empty string.
 *
 * @param opchr the option character.
 * @param oparg the option argument.
 */
#define make_opt(opchr, oparg) "-" opchr, oparg

static inline int wait_proc_fn(int id, const char *id_str)
{
  int child_exit_code;

  if (waitpid(id, &child_exit_code, 0) != id) {
    log_syserror("Cannot reap %s", id_str);
    return -1;
  } else {
    if (!WIFEXITED(child_exit_code)) {
      log_error("%s does not terminate normally", id_str);
      if (WIFSIGNALED(child_exit_code)) {
        log_error("%s got signal %d", id_str, WTERMSIG(child_exit_code));
      }
      return -1;
    } else {
      if (WEXITSTATUS(child_exit_code) != EXIT_SUCCESS) {
        log_error("%s terminates due to an error", id_str);
        return -1;
      }
    }
  }

  return 0;
}

/**
 * Wait for a subprocess to exit.
 *
 * @param id the ID of the subprocess to be waited.
 *
 * @return zero if the subprocess exits without error, -1 otherwise.
 */
#define wait_proc(id) wait_proc_fn(id, #id)

static inline int kill_proc_fn(int id, int sgnl, const char *id_str)
{
  if (kill(id, sgnl) != 0) {
    log_syserror("Cannot kill %s with signal %d", id_str, sgnl);
    return -1;
  }
  return 0;
}

/**
 * Kill a process with the given signal.
 *
 * @param id the process ID to be killed.
 * @param sgnl the signal to be sent.
 *
 * @return zero if the process can be killed successfully, -1 otherwise.
 */
#define kill_proc(id, sgnl) kill_proc_fn(id, sgnl, #id)

static inline int kill_and_wait_proc_fn(int id, int sgnl, const char *id_str)
{
  if (kill_proc_fn(id, sgnl, id_str) != 0) {
    return -1;
  }
  if (wait_proc_fn(id, id_str) != 0) {
    return -2;
  }
  return 0;
}

/**
 * Combine function kill_proc and wait_proc.
 *
 * @param id the process to be killed and then waited.
 * @param sgnl the signal to kill the process.
 *
 * @return zero if both operations are successful, -1 if kill_proc
 * fails and -2 if wait_proc fails.
 */
#define kill_and_wait_proc(id, sgnl) kill_and_wait_proc_fn(id, sgnl, #id)

#endif /* UTILITY_TIME */
