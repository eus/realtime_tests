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
   * Open a text file for reading.
   *
   * @param path a pointer to the string containing the file path.
   *
   * @return a pointer to the opened file stream. If there is an I/O
   * error to open the file for reading, the error is @ref
   * utility_log.h "logged" and NULL is returned.
   */
  FILE *utility_file_open_for_reading(const char *path);

  /**
   * Open a text file for writing.
   *
   * @param path a pointer to the string containing the file path.
   *
   * @return a pointer to the opened file stream. If there is an I/O
   * error to open the file for writing, the error is @ref
   * utility_log.h "logged" and NULL is returned.
   */
  FILE *utility_file_open_for_writing(const char *path);

  /**
   * Close a file stream.
   *
   * @param file_stream a pointer to the file stream to be closed.
   * @param path a pointer to the string containing the file path
   * associated with file_stream. This is only used for logging when
   * the file stream cannot be closed successfully. And as such, there
   * is no hard requirement that the given path is really the path of
   * the file_stream.
   *
   * @return 0 if file_stream is successfully closed. If the file
   * stream cannot be closed successfully, the error is @ref
   * utility_log.h "logged" and -1 is returned.
   */
  int utility_file_close(FILE *file_stream, const char *path);

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

  /**
   * Use utility_file_readln() to read a textual file stream
   * line-by-line at each line of which read_fn is invoked to process
   * the read line. To stop reading the file, read_fn can return a
   * non-zero value. Otherwise, read_fn must return zero.
   *
   * @param file_stream a pointer to the file stream to read.
   * @param buffer_inc the desired increment in bytes when the
   * internal buffer needs to be enlarged.
   * @param read_fn a pointer to the callback function to process the
   * just read line.
   * @param args a pointer to an object to be passed as the second
   * argument of read_fn
   *
   * @return zero if all lines can be read successfully, -1 if read_fn
   * returns non-zero preventing the processing of the remaining
   * lines, or -2 if there is an I/O error while reading (the error
   * itself is @ref utility_log.h "logged" directly).
   */
  int utility_file_read(FILE *file_stream, size_t buffer_inc,
			int (*read_fn)(const char *line, void *args),
			void *args);
  /** @} End of collection of functions to deal with textual files */

  /* II */
  /**
   * @name Collection of functions to deal with binary files.
   * @{
   */

  /**
   * Open a binary file for reading.
   *
   * @param path a pointer to the string containing the file path.
   *
   * @return a pointer to the opened file stream. If there is an I/O
   * error to open the file for reading, the error is @ref
   * utility_log.h "logged" and NULL is returned.
   */
  FILE *utility_file_open_for_reading_bin(const char *path);

  /**
   * Open a binary file for writing.
   *
   * @param path a pointer to the string containing the file path.
   *
   * @return a pointer to the opened file stream. If there is an I/O
   * error to open the file for writing, the error is @ref
   * utility_log.h "logged" and NULL is returned.
   */
  FILE *utility_file_open_for_writing_bin(const char *path);

  /**
   * Read the specified amount of byte(s) from the binary file stream
   * and store the bytes at the located pointed by buffer.
   *
   * @param file_stream a pointer to the file stream to read.
   * @param buffer the location at which the read byte(s) should be stored.
   * @param len a pointer to the location storing the amount of
   * byte(s) to be read from the stream. The value in the pointed
   * location is only changed when -2 is returned.
   *
   * @return zero if the specified amount of byte(s) can be read and
   * stored successfully, -1 if the end of the stream has been
   * reached, -2 if the number of bytes read is less than the one
   * specified in len (the actual number of bytes read is stored in
   * the location specified in len), or -3 if there is an I/O error
   * while reading (the error itself is @ref utility_log.h "logged"
   * directly).
   */
  int utility_file_read_bin(FILE *file_stream, void *buffer, size_t *len);
  /** @} End of collection of functions to deal with binary files */

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_FILE_H */
