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
 * @file utility_log.h
 * @brief Various functionalities to conveniently log various kind of
 * messages and to conveniently exit the program with an error message
 * in case of fatal error.
 * @author Tadeus Prastowo <eus@member.fsf.org>
 *
 * All log messages take the following format:
 * @code
 * PROGRAM_NAME[THREAD_ID_OR_PROCESS_ID]: LOG_MESSAGE
 * @endcode
 *
 * When a program uses multithreading where the use of function
 * <code>pthread_self</code> makes sense, and the program uses this
 * header, it must be compiled by defining <code>_REENTRANT</code> so
 * that the logging component can use function
 * <code>pthread_self</code> instead of function <code>getpid</code>
 * when printing log messages so that one can properly identify the
 * source of a log message. The constant <code>_REENTRANT</code> is
 * automatically defined when compiling using GCC by specifying option
 * <code>-pthread</code>.
 *
 * The logging functions are synchronized to the logging stream.
 */

#ifndef UTILITY_LOG_H
#define UTILITY_LOG_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _REENTRANT
#include <pthread.h>
#define log_hdr() fprintf(log_stream, "%s[%lu]: ", prog_name, \
			  (unsigned long) pthread_self())
#else
#include <unistd.h>
#define log_hdr() fprintf(log_stream, "%s[%lu]: ", prog_name, \
			  (unsigned long) getpid())
#endif

/**
 * Log the given message as it is to the logging stream. This is a
 * wrapper over printf-like function and so accepts things like
 * conversion specifier in the message and additional arguments to
 * resolve the conversion specifier.
 *
 * @param msg the message in printf format string to be logged.
 * @param ... the arguments to resolve the conversion specifiers in the message.
 *
 * @hideinitializer
 */
#define log_verbose(msg, ...)			\
  do {						\
    flockfile(log_stream);			\
    log_hdr();					\
    fprintf(log_stream, msg , ## __VA_ARGS__);	\
    funlockfile(log_stream);			\
  } while (0)

/**
 * Log the given message by appending a newline at the end of the
 * message to the logging stream. This is a wrapper over printf-like
 * function and so accepts things like conversion specifier in the
 * message and additional arguments to resolve the conversion
 * specifier.
 *
 * @param msg the message in printf format string to be logged.
 * @param ... the arguments to resolve the conversion specifiers in
 * the message.
 *
 * @hideinitializer
 */
#define log_error(msg, ...)				\
  do {							\
    flockfile(log_stream);				\
    log_hdr();						\
    fprintf(log_stream, msg "\n" , ## __VA_ARGS__);	\
    funlockfile(log_stream);				\
  } while (0)

/**
 * Work just like log_error() but exit the program with function
 * <code>exit</code> supplying error code <code>EXIT_FAILURE</code>
 * after logging the message.
 *
 * @hideinitializer
 */
#define fatal_error(msg, ...)				\
  do {							\
    log_error(msg , ## __VA_ARGS__);			\
    exit(EXIT_FAILURE);					\
  } while (0)

/**
 * Log the given message by appending the result of
 * <code>strerror(errno)</code> and a newline at the end of the
 * message to the logging stream. This is a wrapper over printf-like
 * function and so accepts things like conversion specifier in the
 * message and additional arguments to resolve the conversion
 * specifier.
 *
 * @param msg the message in printf format string to be logged.
 * @param ... the arguments to resolve the conversion specifiers in the message.
 *
 * @hideinitializer
 */
#define log_syserror(msg, ...)						\
  do {									\
    const char *err_msg = strerror(errno);				\
    flockfile(log_stream);						\
    log_hdr();								\
    fprintf(log_stream, msg " (%s)\n" , ## __VA_ARGS__, err_msg);	\
    funlockfile(log_stream);						\
  } while (0)

/**
 * This works just like log_syserror() but exit the program with
 * function <code>exit</code> supplying error code
 * <code>EXIT_FAILURE</code>.
 *
 * @hideinitializer
 */
#define fatal_syserror(msg, ...)					\
  do {									\
    log_syserror(msg , ## __VA_ARGS__);					\
    exit(EXIT_FAILURE);							\
  } while (0)

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * Each file defining the main function must define a program name.
   */
  extern const char *prog_name;

  /**
   * Each file defining the main function must provide an output
   * stream open for logging.
   */
  extern FILE *log_stream;

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_LOG_H */
