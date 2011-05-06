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
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "utility_time.h"
#include "utility_log.h"

const char prog_name[] = "utility_log_test";
FILE *log_stream;

static int in_parent = 1;
static char tmp_file_name[] = "utility_log_test_XXXXXX";
static void cleanup(void)
{
  if (!in_parent) {
    return;
  }

  if (remove(tmp_file_name) != 0 && errno != ENOENT) {
    log_syserror("Unable to remove the temporary file");
  }
}

/* For testcase involving pthread */
static pthread_barrier_t green_light;
static int green_light_initialized = 0;
/* End of the stuff of testcase involving pthread */

void *run_thread(void *args)
{
  int *exit_status = args;
  *exit_status = EXIT_FAILURE;

  int rc = pthread_barrier_wait(&green_light);
  if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
    errno = rc;
    log_syserror("Fail to wait on thread's green light");
    return exit_status;
  }

  int sleeping_time_max = 100 /* ms */;
  struct timespec sleeping_time;

#define random_wait() do {						\
    to_timespec_gc(to_utility_time_dyn(1 + rand() % sleeping_time_max,	\
				       ms),				\
		   &sleeping_time);					\
    nanosleep(&sleeping_time, NULL);					\
  } while (0)

  random_wait();
  log_verbose("log_verbose\n");

  random_wait();
  log_error("log_error");

  random_wait();
  errno = ENAMETOOLONG;
  log_syserror("log_syserror");

#undef random_wait

  *exit_status = EXIT_SUCCESS;
  return exit_status;
}

int main(int argc, char **argv, char **envp)
{
  log_stream = stderr;

  if (atexit(cleanup) != 0) {
    fatal_syserror("Unable to register cleanup function at exit");
  }

  int tmp_file_fd = mkstemp(tmp_file_name);
  if (tmp_file_fd == -1) {
    fatal_syserror("Unable to create a temporary file");
  }
  if (close(tmp_file_fd) != 0) {
    fatal_syserror("Unable to close the temporary file");
  }

  /* Allow for the invocation of cleanup() */
#define gracious_assert(condition) if (!(condition))		\
    fatal_error("%s:%d: %s: Assertion `" #condition "' failed", \
		__FILE__, __LINE__, __FUNCTION__)

  /* Allow for easy setup of the logging stream of subprocess */
#define setup_subprocess_log_stream() do {				\
    in_parent = 0;							\
    FILE *new_log_stream = fopen(tmp_file_name, "w");			\
    if (new_log_stream == NULL) {					\
      fatal_syserror("Unable to open the temporary file for writing");	\
    }									\
    log_stream = new_log_stream;					\
  } while (0)

  /* Allow for easy inspection of a logged line */
#define cmp_token(rc, token, ...) do {					\
    snprintf(expectation, sizeof(expectation), token, ## __VA_ARGS__);	\
    size_t toklen = strlen(expectation);				\
    rc += strncmp(ptr, expectation, toklen);				\
    ptr = ptr + toklen;							\
  } while (0)
#define skip_line_section(ending_char) do {	\
    while (*ptr != ending_char) {		\
      gracious_assert(*ptr != '\0');		\
      ptr++;					\
    }						\
  } while (0)
#define check_proc_thread_id(rc, id) do {	\
    gracious_assert(*ptr++ == '[');		\
    if (id == -1) {				\
      skip_line_section(']');			\
    } else {					\
      cmp_token(rc, "%u", (unsigned int) id);	\
    }						\
    gracious_assert(*ptr++ == ']');		\
  } while (0)
#define get_next_line(buf, buf_len)				\
  gracious_assert(fgets(buf, buf_len, log_file) != NULL);
#define test_line(rc, buf, proc_id, thread_id, msg, ...) do {	\
    char *ptr = buf;						\
    cmp_token(rc, "utility_log_test");				\
    check_proc_thread_id(rc, proc_id);				\
    check_proc_thread_id(rc, thread_id);			\
    cmp_token(rc, ": " msg, ## __VA_ARGS__);			\
  } while (0)

  /* Conveniences to deal with a subprocess */
  FILE *log_file = NULL;
  char buffer[1024], expectation[1024];
#define begin_subprocess_log_inspection(expected_exit_code) do {	\
    int child_exit_status = 0;						\
    if (waitpid(child_pid, &child_exit_status, 0) == -1) {		\
      fatal_syserror("Cannot wait for testcase 1 subprocess");		\
    }									\
    if (!WIFEXITED(child_exit_status)) {				\
      fatal_syserror("Unexpected return of testcase 1 subprocess");	\
    }									\
    gracious_assert(WEXITSTATUS(child_exit_status)			\
		    == expected_exit_code);				\
    log_file = fopen(tmp_file_name, "r");				\
    if (log_file == NULL) {						\
      fatal_syserror("Cannot open subprocess log file for reading");	\
    }									\
  } while (0)
#define end_subprocess_log_inspection() do {				\
    if (fclose(log_file) != 0) {					\
      fatal_syserror("Cannot close subprocess log file after reading");	\
    }									\
    log_file = NULL;							\
  } while (0)

  /* Testcase 1: logging from within a process */
  pid_t child_pid = fork();
  if (child_pid == 0) {
    setup_subprocess_log_stream();

    log_verbose("log_verbose\n");
    log_error("log_error");
    errno = ENAMETOOLONG;
    log_syserror("log_syserror");
    errno = ENOEXEC;
    fatal_syserror("fatal_syserror");

    return EXIT_SUCCESS;
  } else if (child_pid == -1) {
    fatal_syserror("Cannot create subprocess for testcase 1");
  } else {
    begin_subprocess_log_inspection(EXIT_FAILURE);

#define assert_line(msg, ...) do {				\
      int rc = 0;						\
      get_next_line(buffer, sizeof(buffer));			\
      test_line(rc, buffer, child_pid, -1,			\
		msg, ## __VA_ARGS__);				\
      gracious_assert(rc == 0);					\
    } while (0)

    assert_line("log_verbose\n");
    assert_line("[ERROR] log_error\n");
    assert_line("[ERROR] log_syserror (%s)\n", strerror(ENAMETOOLONG));
    assert_line("[FATAL] fatal_syserror (%s)\n", strerror(ENOEXEC));
    gracious_assert(fgets(buffer, sizeof(buffer), log_file) == NULL);

    end_subprocess_log_inspection();
  }

  /* Testcase 2: logging from within a thread */
  int pipefd[2]; /* Used by the subprocess to communicate the thread IDs */
  if (pipe(pipefd) == -1) {
    fatal_syserror("Cannot create pipe fd");
  }
  child_pid = fork();
  if (child_pid == 0) {
    /* Keep using parent's logging stream during test setup */

    /* Take care of the communication pipe */
    if (close(pipefd[0]) != 0) {
      fatal_syserror("Cannot close pipe read end");
    }
    /* End of taking care of the communication pipe */

    /* Setting up thread data structures */
    struct {
      pthread_t thread;
      int exit_status;
    } thread_data[2];
    int thread_data_count = sizeof(thread_data) / sizeof(*thread_data);
    /* End of setting up thread data structures */

    /* Initialize the thread's green light */
    if (green_light_initialized) {
      if (pthread_barrier_destroy(&green_light) != 0) {
	fatal_syserror("Cannot destroy thread's green light");
      }
    }
    if (pthread_barrier_init(&green_light, NULL, thread_data_count + 1) != 0) {
      fatal_syserror("Cannot initialize thread's green light");
    }
    /* End of initializing the thread's green light */

    /* Create the thread(s) */
    int idx;
    for (idx = 0; idx < thread_data_count; idx++) {
      if (pthread_create(&thread_data[idx].thread, NULL,
  			 run_thread, &thread_data[idx].exit_status) != 0) {
  	fatal_syserror("Cannot create thread #%d", idx);
      }
      if (write(pipefd[1], &thread_data[idx].thread, sizeof(pthread_t)) == -1) {
  	fatal_syserror("Cannot write thread ID #%d", idx);
      }
    }
    /* End of creating the thread(s) */

    /* Let the logging facility use this subprocess logging stream */
    FILE *parent_log_stream = log_stream;
#define log_to_parent_log_stream(msg, ...) do {		\
      flockfile(parent_log_stream);			\
      log_hdr_fatal(parent_log_stream);			\
      fprintf(parent_log_stream, msg, ## __VA_ARGS__);	\
      funlockfile(parent_log_stream);			\
    } while (0)

    setup_subprocess_log_stream();
    /* End of changing the logging stream */

    /* Let the threads run */
    int rc = pthread_barrier_wait(&green_light);
    if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
      errno = rc;
      log_to_parent_log_stream("Cannot turn on the thread's green light");
      return EXIT_FAILURE;
    }
    /* End of letting the threads run */

    /* Wait for the threads and check their exit statuses */
    for (idx = 0; idx < thread_data_count; idx++) {
      if (pthread_join(thread_data[idx].thread, NULL) != 0) {
  	log_to_parent_log_stream("Cannot join thread #%d", idx);
  	return EXIT_FAILURE;
      }
    }
    for (idx = 0; idx < thread_data_count; idx++) {
      if (thread_data[idx].exit_status != EXIT_SUCCESS) {
	log_to_parent_log_stream("Thread #%d fails", idx);
	return EXIT_FAILURE;
      }
    }
    /* End of waiting for the threads and checking their exit statuses */

    return EXIT_SUCCESS;
  } else if (child_pid == -1) {
    fatal_syserror("Cannot create subprocess for testcase 2");
  } else {
    if (close(pipefd[1]) != 0) {
      fatal_syserror("Cannot close pipe write end");
    }

    /* Read thread IDs from the pipe */
    pthread_t thread_ids[2];
    size_t thread_ids_count = sizeof(thread_ids) / sizeof(*thread_ids);
    int idx;
    for (idx = 0; idx < thread_ids_count; idx++) {
      if (read(pipefd[0], &thread_ids[idx], sizeof(pthread_t)) == -1) {
	fatal_syserror("Cannot read ID of thread #%d", idx);
      }
      /* Ensure that thread IDs are not duplicated */
      int i, j;
      for (i = 0; i <= idx; i++) {
	for (j = i + 1; j <= idx; j++) {
	  gracious_assert(thread_ids[i] != thread_ids[j]);
	}
      }
      /* End of ensuring non-duplication of thread IDs */
    }
    /* End of reading thread IDs from the pipe */

    begin_subprocess_log_inspection(EXIT_SUCCESS);

    const int line_count = 6;
    char buffer_interleaved[line_count][1024];
#define assert_line_interleaved(line_count, thread_id, msg, ...)	\
    do {								\
      int found = 0;							\
      int line_idx;							\
      for (line_idx = 0; line_idx < line_count; line_idx++) {		\
	int rc = 0;							\
	test_line(rc, buffer_interleaved[line_idx],			\
		  child_pid, thread_id,					\
		  msg, ## __VA_ARGS__);					\
	if (rc == 0) {							\
	  found = 1;							\
	  /* Each line can only be checked once */			\
	  buffer_interleaved[line_idx][0] = '\0';			\
	  break;							\
	}								\
      }									\
      gracious_assert(found);						\
    } while (0)

    int line_idx;
    for (line_idx = 0; line_idx < line_count; line_idx++) {
      get_next_line(buffer_interleaved[line_idx], 1024);
    }

    for (idx = 0; idx < thread_ids_count; idx++) {
      assert_line_interleaved(line_count, thread_ids[idx],
			      "log_verbose\n");
      assert_line_interleaved(line_count, thread_ids[idx],
			      "[ERROR] log_error\n");
      assert_line_interleaved(line_count, thread_ids[idx],
			      "[ERROR] log_syserror (%s)\n",
			      strerror(ENAMETOOLONG));
    }
    gracious_assert(fgets(buffer, sizeof(buffer), log_file) == NULL);

    end_subprocess_log_inspection();
  }

  return EXIT_SUCCESS;
}
