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
 * @file utility_experimentation.h
 * @brief Various functionalities commonly used when making an
 * experimentation component.
 * @author Tadeus Prastowo <eus@member.fsf.org>
 */

#ifndef UTILITY_EXPERIMENTATION_H
#define UTILITY_EXPERIMENTATION_H

#include <stdio.h>
#include <stdlib.h>
#include "utility_log.h"
#include "utility_cpu.h"

/**
 * Conveniently begin the main function of an experimentation
 * utilizing only one CPU core with the maximum frequency.
 *
 * @param experiment_name the name of the experimentation program.
 * @param log_stream_path the path to the file used for logging. To
 * use stderr or stdout, pass "stderr" or "stdout" and set write_mode
 * to NULL. If write_mode is not set to NULL, "stderr" or "stdout" is
 * considered to be the name of a file in the current working
 * directory.
 * @param write_mode whether the log_stream_path should be opened for
 * writing by passing "w" or appending by passing "a" or whether the
 * log_stream_path should be treated as the name of a variable of type
 * <code>FILE *</code> by passing NULL.
 */
#define MAIN_BEGIN(experiment_name, log_stream_path, write_mode)        \
  static cpu_freq_governor *default_gov = NULL;                         \
  static void cleanup_restore_gov(void)                                 \
  {                                                                     \
    if (default_gov != NULL) {                                          \
      if (cpu_freq_restore_governor(default_gov) != 0) {                \
        log_error("You must restore the CPU freq governor yourself");   \
      }                                                                 \
    }                                                                   \
  }                                                                     \
  const char prog_name[] = experiment_name;                             \
  FILE *log_stream;                                                     \
  int main(int argc, char **argv, char **envp)                          \
  {                                                                     \
    log_stream = stderr;                                                \
    if (write_mode == NULL) {                                           \
      if (strcmp(log_stream_path, "stdout") == 0) {                     \
        log_stream = stdout;                                            \
      } else if (strcmp(log_stream_path, "stderr") == 0) {              \
        log_stream = stderr;                                            \
      } else {                                                          \
        fatal_error("Cannot start experiment"                           \
                    " ('%s' is neither stderr nor stdout)\n",           \
                    log_stream_path);                                   \
      }                                                                 \
    } else {                                                            \
      log_stream = fopen(log_stream_path, write_mode);                  \
      if (log_stream == NULL) {                                         \
        fatal_error("Cannot start experiment"                           \
                    " (fail to open log stream '%s')\n",                \
                    log_stream_path);                                   \
      }                                                                 \
    }                                                                   \
    if (atexit(cleanup_restore_gov) != 0) {                             \
      fatal_syserror("Cannot start experiment (fail to register"        \
                     " cleanup_restore_gov at exit)\n");                \
    }                                                                   \
    if (enter_UP_mode_freq_max(&default_gov) != 0) {                    \
      fatal_syserror("Cannot enter UP mode with maximum frequency");    \
    }

/** This must be used to close MAIN_BEGIN(). */
#define MAIN_END }

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_EXPERIMENTATION_H */
