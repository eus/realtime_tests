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
 * @file utility_file.h
 * @brief Various functionalities to conveniently read and write a file.
 * @author Tadeus Prastowo <eus@member.fsf.org>
 */

#ifndef UTILITY_FILE_H
#define UTILITY_FILE_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

  /* I */
  /**
   * @name Collection of functions to deal with textual files.
   * @{
   */

  /**
   * Read the next line of a file stream to the given buffer enlarging
   * the buffer as necessary in a multiple of the specified
   * increment. The buffer can be preallocated. The caller is
   * responsible for freeing the buffer if the returned buffer pointer
   * is not NULL.
   *
   * Reading stops upon encountering the first newline character or
   * the end of the file. The newline character is not stored in the
   * buffer. The buffer is NULL-character terminated.
   *
   * @param file_stream a pointer to the file stream to read.
   * @param buffer a pointer to the buffer pointer. The buffer pointer
   * must be set to NULL. Otherwise, the buffer pointer is assumed to
   * point to a dynamically preallocated buffer suitable to be passed
   * to realloc().
   * @param buffer_len a pointer to the length in bytes of the
   * buffer. If the buffer pointer is set to NULL, buffer_len only
   * stores the final length of the buffer. Otherwise, buffer_len is
   * also used to decide whether or not to enlarge the buffer.
   * @param buffer_inc the desired increment in bytes when the buffer
   * needs to be enlarged.
   *
   * @return zero if the next line can be read successfully, -1 if the
   * end of file has been reached, or -2 if there is an I/O error
   * while reading (the error itself is @ref utility_log.h "logged"
   * directly).
   */
  int utility_file_readln(FILE *file_stream, char **buffer, size_t *buffer_len,
			  size_t buffer_inc);
  /** @} End of collection of functions to deal with textual files */

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_FILE_H */
