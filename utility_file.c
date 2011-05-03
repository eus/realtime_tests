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
    log_error("Error while reading the whole line");
    return -2;
  }
  /* End of checking for a read error */

  return 0;
}
