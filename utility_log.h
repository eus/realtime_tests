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
 * PROGRAM_NAME[PROCESS_ID][THREAD_ID][FUNCTION@FILE:LINE]: LOG_MESSAGE
 * @endcode
 *
 * The logging functions are synchronized to the logging stream.
 *
 * User is responsible for opening the logging stream as well as
 * closing it to avoid any loss of log message.
 */

#ifndef UTILITY_LOG_H
#define UTILITY_LOG_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

/**
 * @name Collection of functions that should not be used in normal circumstances
 * @{
 */

/**
 * Print "PROGRAM_NAME[PROCESS_ID][THREAD_ID][FUNCTION@FILE:LINE]: "
 * to the specified stream. This should not be used in normal logging
 * operation because one has to go through the hassle of synchronizing
 * to the log stream.
 *
 * @hideinitializer
 */
#define log_hdr(stream) fprintf(stream,					\
				"%s[%lu][%lu][%s@%s:%u]: ",		\
				prog_name,				\
				(unsigned long) getpid(),		\
				(unsigned long) pthread_self(),		\
				__FUNCTION__, __FILE__, __LINE__)

/**
 * Print "PROGRAM_NAME[PROCESS_ID][THREAD_ID][FUNCTION@FILE:LINE]: [ERROR] "
 * to the specified stream. This should not be used in normal logging
 * operation because one has to go through the hassle of synchronizing
 * to the log stream.
 *
 * @hideinitializer
 */
#define log_hdr_error(stream)				\
  fprintf(stream, "%s[%lu][%lu]: [ERROR] ", prog_name,	\
	  (unsigned long) getpid(),			\
	  (unsigned long) pthread_self())

/**
 * Print "PROGRAM_NAME[PROCESS_ID][THREAD_ID][FUNCTION@FILE:LINE]: [FATAL] "
 * to the specified stream. This should not be used in normal logging
 * operation because one has to go through the hassle of synchronizing
 * to the log stream.
 *
 * @hideinitializer
 */
#define log_hdr_fatal(stream)				\
  fprintf(stream, "%s[%lu][%lu]: [FATAL] ", prog_name,	\
	  (unsigned long) getpid(),			\
	  (unsigned long) pthread_self())

/** @} End of collection of functions that should not be used in
    normal circumstances */

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
    log_hdr(log_stream);			\
    fprintf(log_stream, msg , ## __VA_ARGS__);	\
    funlockfile(log_stream);			\
  } while (0)

/* Common function to both log_error() and fatal_error() */
#define log_error_core(hdr, msg, ...)			\
  do {							\
    flockfile(log_stream);				\
    hdr(log_stream);					\
    fprintf(log_stream, msg "\n" , ## __VA_ARGS__);	\
    funlockfile(log_stream);				\
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
#define log_error(msg, ...) log_error_core(log_hdr_error, msg, ## __VA_ARGS__)

/**
 * Work just like log_error() but exit the program with function
 * <code>exit</code> supplying error code <code>EXIT_FAILURE</code>
 * after logging the message. The logging stream is flushed just
 * before exiting.
 *
 * @hideinitializer
 */
#define fatal_error(msg, ...)				\
  do {							\
    log_error_core(log_hdr_fatal, msg, ## __VA_ARGS__);	\
    fflush(log_stream);					\
    exit(EXIT_FAILURE);					\
  } while (0)

#define log_syserror_core(hdr, msg, ...)				\
  do {									\
    const char *err_msg = strerror(errno);				\
    flockfile(log_stream);						\
    hdr(log_stream);							\
    fprintf(log_stream, msg " (%s)\n" , ## __VA_ARGS__, err_msg);	\
    funlockfile(log_stream);						\
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
#define log_syserror(msg, ...)				\
  log_syserror_core(log_hdr_error, msg, ## __VA_ARGS__)

/**
 * This works just like log_syserror() but exit the program with
 * function <code>exit</code> supplying error code
 * <code>EXIT_FAILURE</code>. The logging stream is flushed just
 * before exiting.
 *
 * @hideinitializer
 */
#define fatal_syserror(msg, ...)					\
  do {									\
    log_syserror_core(log_hdr_fatal, msg , ## __VA_ARGS__);		\
    fflush(log_stream);							\
    exit(EXIT_FAILURE);							\
  } while (0)

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * Each file defining the main function must define a program name.
   */
  extern const char prog_name[];

  /**
   * Each file defining the main function must provide an output
   * stream open for logging.
   */
  extern FILE *log_stream;

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_LOG_H */
