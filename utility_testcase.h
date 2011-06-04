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
 * @file utility_testcase.h
 * @brief Various functionalities commonly used when making a test unit.
 * @author Tadeus Prastowo <eus@member.fsf.org>
 */

#ifndef UTILITY_TESTCASE_H
#define UTILITY_TESTCASE_H

#include <stdio.h>
#include <stdlib.h>

/**
 * Allow for the invocation of functions registered with atexit() when
 * condition evaluates to false (i.e., equals to 0).
 */
#define gracious_assert(condition) if (!(condition)) {                  \
    fprintf(stderr, "%s@%s:%d: Assertion `" #condition "' failed\n",    \
            __FUNCTION__, __FILE__, __LINE__);                          \
    exit(EXIT_FAILURE);                                                 \
  }

/**
 * Work like gracious_assert() but a printf() format string can be
 * supplied to give an explanation as to why the condition fails as
 * well as a hint to fix the problem.
 */
#define gracious_assert_msg(condition, msg, ...)                \
  if (!(condition)) {                                           \
    fprintf(stderr, "%s@%s:%d: Assertion `"                     \
            #condition "' failed (" msg ")\n",                  \
            __FUNCTION__, __FILE__, __LINE__, ## __VA_ARGS__);  \
    exit(EXIT_FAILURE);                                         \
  }

/**
 * Work like gracious_assert_msg() but the program is not exited.
 */
#define gracious_assert_msg_noquit(condition, msg, ...)         \
  if (!(condition)) {                                           \
    fprintf(stderr, "%s@%s:%d: Assertion `"                     \
            #condition "' failed (" msg ")\n",                  \
            __FUNCTION__, __FILE__, __LINE__, ## __VA_ARGS__);  \
  }

/**
 * Conveniently begin the main function of a unit test.
 *
 * @param unit_name the name of the unit test.
 * @param log_stream_path the path to the file used for logging. To
 * use stderr or stdout, pass "stderr" or "stdout" and set write_mode
 * to NULL. If write_mode is not set to NULL, "stderr" or "stdout" is
 * considered to be the name of a file in the current working
 * directory.
 * @param write_mode whether the log_stream_path should be opened for
 * writing by passing "w" or appending by passing "a" or whether the
 * log_stream_path should be treated as the name of a variable of type
 * <code>FILE *</code> by passing NULL.
 * @param cleanup_fn a function to be registered using atexit(). Set
 * this to NULL if no function needs to be registered using atexit().
 */
#define MAIN_UNIT_TEST_BEGIN(unit_name, log_stream_path,                \
                             write_mode, cleanup_fn)                    \
  const char prog_name[] = unit_name;                                   \
  FILE *log_stream;                                                     \
  int main(int argc, char **argv, char **envp)                          \
  {                                                                     \
    if (write_mode == NULL) {                                           \
      if (strcmp(log_stream_path, "stdout") == 0) {                     \
        log_stream = stdout;                                            \
      } else if (strcmp(log_stream_path, "stderr") == 0) {              \
        log_stream = stderr;                                            \
      } else {                                                          \
        fprintf(stderr,                                                 \
                "Cannot start unit test"                                \
                " ('%s' is neither stderr nor stdout)\n",               \
                log_stream_path);                                       \
        return EXIT_FAILURE;                                            \
      }                                                                 \
    } else {                                                            \
      log_stream = fopen(log_stream_path, write_mode);                  \
      if (log_stream == NULL) {                                         \
        fprintf(stderr,                                                 \
                "Cannot start unit test"                                \
                " (fail to open log stream '%s')\n",                    \
                log_stream_path);                                       \
        return EXIT_FAILURE;                                            \
      }                                                                 \
    }                                                                   \
    if (cleanup_fn != NULL) {                                           \
      int muffle_warning_passing_NULL_to_atexit(void (*function)(void)) \
      {                                                                 \
        return atexit(function);                                        \
      }                                                                 \
      if (muffle_warning_passing_NULL_to_atexit(cleanup_fn) != 0) {     \
        fprintf(stderr,                                                 \
                "Cannot start unit test"                                \
                " (fail to register %s() at exit)\n",                   \
                #cleanup_fn);                                           \
        return EXIT_FAILURE;                                            \
      }                                                                 \
    }

/** This must be used to close MAIN_UNIT_TEST_BEGIN(). */
#define MAIN_UNIT_TEST_END }

/** Conveniences to check the exit status of a subprocess. */
#define check_subprocess_exit_status(expected_exit_code) do {           \
    int child_exit_status = 0;                                          \
    gracious_assert(waitpid(child_pid, &child_exit_status, 0) != -1);   \
    gracious_assert(WIFEXITED(child_exit_status));                      \
    gracious_assert(WEXITSTATUS(child_exit_status)                      \
                    == expected_exit_code);                             \
  } while (0)

/** A statement must be made whether the test unit is run under
    Valgrind or not. */
#define require_valgrind_indicator()                                    \
  do {                                                                  \
    if (argc != 2) {                                                    \
      fprintf(stderr,                                                   \
              "Usage: %s UNDER_VALGRIND\n"                              \
              "Set UNDER_VALGRIND to 1 if this is run under Valgrind.\n" \
              "Otherwise, set it to 0. This is because some testcases\n" \
              "are expected to fail under Valgrind but not when run\n"  \
              "normally.\n", prog_name);                                \
      return EXIT_FAILURE;                                              \
    }                                                                   \
  } while (0)

/** Return non-zero if the test unit uses require_valgrind_indicator()
    and is run under Valgrind. */
#define under_valgrind() (argc == 2 && argv[1][0] != '0')

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_TESTCASE_H */
