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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "utility_testcase.h"
#include "utility_log.h"
#include "utility_file.h"

static char tmp_file_name[] = "utility_file_test_XXXXXX";
static void cleanup(void)
{
  if (remove(tmp_file_name) != 0 && errno != ENOENT) {
    log_syserror("Unable to remove the temporary file");
  }
}

MAIN_UNIT_TEST_BEGIN("utility_file_test", "stderr", NULL, cleanup)
{
  int tmp_file_fd = mkstemp(tmp_file_name);
  if (tmp_file_fd == -1) {
    fatal_syserror("Unable to open a temporary file");
  }
  if (close(tmp_file_fd) != 0) {
    fatal_syserror("Unable to close the temporary file");
  }

  /* Start of testcases */

#define write_to_test_in_stream(content) do {		\
    FILE *test_out_stream				\
      = utility_file_open_for_writing(tmp_file_name);	\
    if (test_out_stream == NULL) {			\
      fatal_error("Unable to open test_out_stream");	\
    }							\
    fprintf(test_out_stream, "%s", content);		\
    if (utility_file_close(test_out_stream,		\
			   tmp_file_name) != 0) {	\
      fatal_error("Unable to close test_out_stream");	\
    }							\
  } while (0)

#define begin_testcase(content) do {				\
    write_to_test_in_stream(content);				\
    test_in_stream = fopen(tmp_file_name, "r");			\
    if (test_in_stream == NULL) {				\
      fatal_syserror("Unable to open test_in_stream");		\
    }								\
  } while (0)

#define free_buffer() do {						\
    if (buffer != NULL) { /* According to the doc, buffer must be freed \
			     if it is not NULL and no longer in use. */ \
      free(buffer);							\
      buffer = NULL;							\
    }									\
  } while (0)

#define end_testcase() do {						\
    free_buffer();							\
    if (fclose(test_in_stream) != 0) {					\
      fatal_syserror("Unable to close test_in_stream");			\
    }									\
  } while (0)

  FILE *test_in_stream;
  char *buffer = NULL;
  size_t buffer_len = 0;
  int rc;

  /* Testcase 1: behavior upon encountering EOF */
  begin_testcase("");
  fgetc(test_in_stream); /* Make it hit EOF: taking out the NULL character */
  fgetc(test_in_stream); /* Make it hit EOF: now it really hits EOF */
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 32);
  gracious_assert(rc == -1);
  end_testcase();

  /* Testcase 2: last line with a newline will still give another empty line */
  begin_testcase("hello there\n");
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 32);
  gracious_assert(rc == 0);
  gracious_assert(strcmp(buffer, "hello there") == 0);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 32);
  gracious_assert(rc == 0);
  gracious_assert(strcmp(buffer, "") == 0);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 32);
  gracious_assert(rc == -1);
  end_testcase();

  /* Testcase 3: necessary enlargement is carried out successfully */
  begin_testcase("hello there\n"
		 "last line");
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 4);
  gracious_assert(rc == 0);
  gracious_assert(strcmp(buffer, "hello there") == 0);
  gracious_assert(buffer_len == 12);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 8);
  gracious_assert(rc == 0);
  gracious_assert(strcmp(buffer, "last line") == 0);
  gracious_assert(buffer_len == 12);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 8);
  gracious_assert(rc == -1);
  gracious_assert(buffer_len == 12);
  end_testcase();

  /* Testcase 4: sufficient preallocated buffer will not be enlarged */
  begin_testcase("hello there\n"
		 "last line");
  buffer_len = 32;
  buffer = malloc(buffer_len);
  gracious_assert(buffer != NULL);
  char *this_buffer = buffer;
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 4);
  gracious_assert(rc == 0);
  gracious_assert(strcmp(buffer, "hello there") == 0);
  gracious_assert(buffer_len == 32);
  gracious_assert(this_buffer == buffer);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 4);
  gracious_assert(rc == 0);
  gracious_assert(strcmp(buffer, "last line") == 0);
  gracious_assert(buffer_len == 32);
  gracious_assert(this_buffer == buffer);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 4);
  gracious_assert(rc == -1);
  gracious_assert(buffer_len == 32);
  gracious_assert(this_buffer == buffer);
  end_testcase();

  /* Testcase 5: opening a text file for reading and closing it */
  write_to_test_in_stream("hello there\n"
			  "last line");
  test_in_stream = utility_file_open_for_reading(tmp_file_name);
  gracious_assert(test_in_stream != NULL);

  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 4);
  gracious_assert(rc == 0);
  gracious_assert(strcmp(buffer, "hello there") == 0);
  gracious_assert(buffer_len == 12);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 8);
  gracious_assert(rc == 0);
  gracious_assert(strcmp(buffer, "last line") == 0);
  gracious_assert(buffer_len == 12);
  rc = utility_file_readln(test_in_stream, &buffer, &buffer_len, 8);
  gracious_assert(rc == -1);
  gracious_assert(buffer_len == 12);

  free_buffer();
  rc = utility_file_close(test_in_stream, tmp_file_name);
  gracious_assert(rc == 0);

  /* Testcase 6: test utility_file_read() */
  write_to_test_in_stream("hello there\n"
			  "last line");
  test_in_stream = utility_file_open_for_reading(tmp_file_name);
  gracious_assert(test_in_stream != NULL);

  unsigned line_counter = 0;
  int read_fn(const char *line, void *args)
  {
    unsigned *line_counter = args;

    switch (*line_counter) {
    case 0:
      gracious_assert(strcmp(line, "hello there") == 0);
      break;
    case 1:
      gracious_assert(strcmp(line, "last line") == 0);
      break;
    default:
      gracious_assert(0);
    }

    *line_counter += 1;

    return 0;
  }
  rc = utility_file_read(test_in_stream, 4, read_fn, &line_counter);
  gracious_assert(rc == 0);

  free_buffer();
  rc = utility_file_close(test_in_stream, tmp_file_name);
  gracious_assert(rc == 0);

  /* Testcase 7: test utility_file_read() with early bailout */
  write_to_test_in_stream("hello there\n"
			  "last line");
  test_in_stream = utility_file_open_for_reading(tmp_file_name);
  gracious_assert(test_in_stream != NULL);

  line_counter = 0;
  int read_fn_bailout(const char *line, void *args)
  {
    read_fn(line, args);

    return 1;
  }
  rc = utility_file_read(test_in_stream, 4, read_fn_bailout, &line_counter);
  gracious_assert(rc == -1);

  /* Continuation must be possible */
  line_counter = 1;
  rc = utility_file_read(test_in_stream, 4, read_fn, &line_counter);
  gracious_assert(rc == 0);

  free_buffer();
  rc = utility_file_close(test_in_stream, tmp_file_name);
  gracious_assert(rc == 0);

  /* Testcase 8: test binary file write and read */
  FILE *binary_file = utility_file_open_for_writing_bin(tmp_file_name);
  gracious_assert(binary_file != NULL);
  double fdata = 777.777;
  gracious_assert(fwrite(&fdata, sizeof(fdata), 1, binary_file) == 1);
  gracious_assert(utility_file_close(binary_file, tmp_file_name) == 0);

  binary_file = utility_file_open_for_reading_bin(tmp_file_name);
  gracious_assert(binary_file != NULL);

  /* Read the data back */
  double fdata_read = 0.0;
  size_t fdata_len = sizeof(fdata_read);
  gracious_assert(utility_file_read_bin(binary_file, &fdata_read, &fdata_len)
		  == 0);
  gracious_assert(fdata_len == sizeof(fdata_read));
  gracious_assert(fdata_read == fdata);
  /* End of reading the data back */

  /* EOF has been reached but the EOF indicator has not been set */
  gracious_assert(utility_file_read_bin(binary_file, &fdata_read, &fdata_len)
		  == -2);
  gracious_assert(fdata_len == 0);
  /* End of reaching EOF */

  /* EOF is hit */
  gracious_assert(utility_file_read_bin(binary_file, &fdata_read, &fdata_len)
		  == -1);
  /* End of hitting EOF */
  
  gracious_assert(utility_file_close(binary_file, tmp_file_name) == 0);

  return EXIT_SUCCESS;

} MAIN_UNIT_TEST_END
