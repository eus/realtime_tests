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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utility_log.h"
#include "utility_file.h"

FILE *utility_file_open_for_reading(const char *path)
{
  FILE *result = fopen(path, "r");
  if (result == NULL) {
    log_syserror("Cannot open %s for textual reading", path);
  }

  return result;
}

FILE *utility_file_open_for_writing(const char *path)
{
  FILE *result = fopen(path, "w");
  if (result == NULL) {
    log_syserror("Cannot open %s for textual writing", path);
  }

  return result;
}

int utility_file_close(FILE *file_stream, const char *path)
{
  if (fclose(file_stream) != 0) {
    log_syserror("Cannot close textual file %s", path);
    return -1;
  }

  return 0;
}

int utility_file_readln(FILE *file_stream, char **buffer, size_t *buffer_len,
			size_t buffer_inc)
{
  /* Detect EOF */
  if (feof(file_stream)) {
    return -1;
  }
  /* End of detecting EOF */

  /* Allocate reading buffer */
  if (*buffer == NULL || *buffer_len == 0) {
    char *initial_buffer = realloc(*buffer, buffer_inc);
    if (initial_buffer == NULL) {
      log_error("Insufficient memory to read the next line");
      return -2;
    }
    *buffer = initial_buffer;
    *buffer_len = buffer_inc;
  }
  /* End of allocating reading buffer */

  /* Read a line from the file */
  int c;
  char *ptr = *buffer;
  while ((c = fgetc(file_stream)) != '\n' && c != EOF) {
    *ptr = c;
    ptr++;

    /* Enlarge the buffer if it is not enough */
    if ((ptr - *buffer) == *buffer_len) {
      size_t enlarged_buffer_len = *buffer_len + buffer_inc;
      char *enlarged_buffer = realloc(*buffer, enlarged_buffer_len);
      if (enlarged_buffer == NULL) {
	log_error("Insufficient memory to read the whole line");
	return -2;
      }
      *buffer = enlarged_buffer;
      ptr = *buffer + *buffer_len;
      *buffer_len = enlarged_buffer_len;
    }
    /* End of enlarging the buffer */
  }
  *ptr = '\0';
  /* End of reading a line from the file */

  /* Check for a read error */
  if (ferror(file_stream)) {
    log_syserror("Error while reading the whole line");
    return -2;
  }
  /* End of checking for a read error */

  return 0;
}

int utility_file_read(FILE *file_stream, size_t buffer_inc,
		      int (*read_fn)(const char *line, void *args),
		      void *args)
{
  unsigned int nth_line = 0;
  char *buffer = NULL;
  size_t buffer_len = 0;

  int exit_status = -3;
  while (exit_status == -3) {
    ++nth_line;

    int rc = utility_file_readln(file_stream, &buffer, &buffer_len, buffer_inc);
    if (rc == -1) {
      exit_status = 0;
      continue;
    } else if (rc == -2) {
      log_error("Fail to read line #%u", nth_line);
      exit_status = -2;
      continue;
    }

    if (read_fn(buffer, args) != 0) {
      exit_status = -1;
      continue;
    }
  }

  if (buffer != NULL) {
    free(buffer);
  }
  return exit_status;
}
