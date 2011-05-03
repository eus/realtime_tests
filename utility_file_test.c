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

#define _GNU_SOURCE /* fmemopen() */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utility_file.h"

const char prog_name[] = "utility_file_test";
FILE *log_stream;

int main(int argc, char **argv, char **envp)
{
  log_stream = stderr;

#define begin_testcase(content, len) do {			\
    buffer_content = content;					\
    strcpy(test_in_stream_buffer, buffer_content);		\
    test_in_stream = fmemopen(test_in_stream_buffer, len, "r");	\
    if (test_in_stream == NULL) {				\
      fprintf(stderr, "Unable to open test_in_stream (%s)\n",	\
	      strerror(errno));					\
      exit(EXIT_FAILURE);					\
    }								\
  } while (0)

#define end_testcase() do {						\
    if (buffer != NULL) { /* According to the doc, buffer must be freed \
			     if it is not NULL and no longer in use. */ \
      free(buffer);							\
      buffer = NULL;							\
    }									\
    if (fclose(test_in_stream) != 0) {					\
      fprintf(stderr, "Unable to close test_in_stream (%s)\n",		\
	      strerror(errno));						\
      exit(EXIT_FAILURE);						\
    }									\
  } while (0)

  char test_in_stream_buffer[1024];
  char *buffer_content;
  FILE *test_in_stream;
  char *buffer = NULL;
  size_t buffer_len = 0;
  int rc;

  /* Testcase 1: behavior upon encountering EOF */
  begin_testcase("", 1);
  fgetc(test_in_stream); /* Make it hit EOF: taking out the NULL character */
  fgetc(test_in_stream); /* Make it hit EOF: now it really hits EOF */
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 32);
  assert(rc == -1);
  end_testcase();

  /* Testcase 2: last line with a newline will still give another empty line */
  begin_testcase("hello there\n", strlen(buffer_content));
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 32);
  assert(rc == 0);
  assert(strcmp(buffer, "hello there") == 0);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 32);
  assert(rc == 0);
  assert(strcmp(buffer, "") == 0);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 32);
  assert(rc == -1);
  end_testcase();

  /* Testcase 3: necessary enlargement is carried out successfully */
  begin_testcase("hello there\n"
		 "last line", strlen(buffer_content));
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 4);
  assert(rc == 0);
  assert(strcmp(buffer, "hello there") == 0);
  assert(buffer_len == 12);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 8);
  assert(rc == 0);
  assert(strcmp(buffer, "last line") == 0);
  assert(buffer_len == 12);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 8);
  assert(rc == -1);
  assert(buffer_len == 12);
  end_testcase();

  /* Testcase 4: sufficient preallocated buffer will not be enlarged */
  begin_testcase("hello there\n"
		 "last line", strlen(buffer_content));
  buffer_len = 32;
  buffer = malloc(buffer_len);
  assert(buffer != NULL);
  char *this_buffer = buffer;
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 4);
  assert(rc == 0);
  assert(strcmp(buffer, "hello there") == 0);
  assert(buffer_len == 32);
  assert(this_buffer == buffer);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 4);
  assert(rc == 0);
  assert(strcmp(buffer, "last line") == 0);
  assert(buffer_len == 32);
  assert(this_buffer == buffer);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 4);
  assert(rc == -1);
  assert(buffer_len == 32);
  assert(this_buffer == buffer);
  end_testcase();

  return EXIT_SUCCESS;
}
