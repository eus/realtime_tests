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
 * @file utility_sched.h
 * @brief Various common functions used by utility_sched_*.h.
 * @author Tadeus Prastowo <eus@member.fsf.org>
 */

#ifndef UTILITY_SCHED
#define UTILITY_SCHED

#include <sys/types.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include "utility_log.h"

#ifdef __cplusplus
extern "C" {
#endif

  /* I. Main data structures */
#ifndef SCHED_DEADLINE
  /** SCHED_DEADLINE scheduling policy in the Linux kernel. */
  #define SCHED_DEADLINE 6

  /** Extended scheduling parameter provided by SCHED_DEADLINE. */
  struct sched_param_ex {
    int sched_priority; /**< Static scheduling priority for SCHED_FIFO
                           and SCHED_RR (ignored by
                           SCHED_DEADLINE). */
    struct timespec sched_runtime; /**< The task's WCET. */
    struct timespec sched_deadline; /**< The task's deadline for EDF. */
    struct timespec sched_period; /**< The task's period. */
    unsigned int sched_flags; /**< SCHED_DEADLINE scheduling flags. */

    struct timespec curr_runtime; /**< The CBS's c_s. */
    struct timespec used_runtime; /**< Unused. */
    struct timespec curr_deadline; /**< The CBS's d_{s,k}. */
  };
#endif

  /* Linux system call numbers to communicate with SCHED_DEADLINE. */
  #ifndef __NR_sched_setscheduler_ex
  #define __NR_sched_setscheduler_ex 341
  #endif
  #ifndef SYS_sched_setscheduler_ex
  #define SYS_sched_setscheduler_ex __NR_sched_setscheduler_ex
  #endif

  #ifndef __NR_sched_setparam_ex
  #define __NR_sched_setparam_ex 342
  /**
   * Retrieve the extended scheduling parameters of the given thread ID.
   *
   * @param thread_id the thread ID. Set this to zero to retrieve the
   * extended scheduling parameters of the calling thread.
   * @param param_ex a pointer to an extended scheduling parameters to
   * store the result.
   *
   * @return zero if the operation is successful or -1 in case of
   * error, and errno is set accordingly.
   */
  int sched_getparam_ex(pid_t thread_id, struct sched_param_ex *param_ex);
  #endif
  #ifndef SYS_sched_setparam_ex
  #define SYS_sched_setparam_ex __NR_sched_setparam_ex
  #endif

  #ifndef __NR_sched_getparam_ex
  #define __NR_sched_getparam_ex 343
  #endif
  #ifndef SYS_sched_getparam_ex
  #define SYS_sched_getparam_ex __NR_sched_getparam_ex
  #endif
  /* End of Linux system call numbers to communicate with SCHED_DEADLINE. */

  /** Wrapper over arguments to sched_setscheduler. */
  struct scheduler
  {
    int policy; /**< POSIX RT scheduling policy */
    struct sched_param param; /**< POSIX RT scheduling parameter */
    struct sched_param_ex param_ex; /**< Linux extended scheduling parameter */
  };
  /* End of main data structures */

  /* II */
  /**
   * @name Collection of common functions.
   * @{
   */
  /**
   * Save the current scheduler.
   *
   * @param curr_scheduler if this is not NULL, the current scheduler
   * is stored in the pointed location so that it can be restored
   * using sched_restore().
   *
   * @return zero if there is no error or -1 in case of hard error
   * that requires the investigation of the output of the logging
   * facility to fix the error.
   */
  int sched_save(struct scheduler *curr_scheduler);

  /**
   * Restore the given scheduler.
   *
   * @param sched_to_be_restored a pointer to the struct scheduler
   * object to be restored.
   *
   * @return zero if the scheduler can be restored or -1 in case of
   * hard error that requires the investigation of the output of the
   * logging facility to fix the error.
   */
  int sched_restore(struct scheduler *sched_to_be_restored);
  /** @} End of collection of common functions */

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_SCHED */
