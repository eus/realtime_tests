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
 * @file utility_sched_deadline.h
 * @brief Various convenient functions to use SCHED_DEADLINE of
 *        git://gitorious.org/sched_deadline/linux-deadline.git.
 *
 *        SCHED_DEADLINE treats each thread as a CBS (Constant
 *        Bandwidth Server) and schedule the threads using EDF
 *        (Earliest Deadline First) scheduling algorithm.
 *
 * @author Tadeus Prastowo <eus@member.fsf.org>
 */

#ifndef UTILITY_SCHED_DEADLINE
#define UTILITY_SCHED_DEADLINE

#include <unistd.h>
#include "utility_sched.h"
#include "utility_log.h"
#include "utility_time.h"

#ifdef __cplusplus
extern "C" {
#endif

  /* I */
  /**
   * @name Collection of functions to enter SCHED_DEADLINE real-time mode.
   * @{
   */

  /**
   * Schedule the caller as a SCHED_DEADLINE real-time thread with the
   * given worst-case execution time and deadline.
   *
   * @param wcet a pointer to utility_time object specifying the
   * worst-case execution time of the thread. The utility_time object
   * is garbage collected automatically if it is possible.
   * @param deadline a pointer to utility_time object specifying the
   * relative deadline of the thread. The utility_time object is
   * garbage collected automatically if it is possible.
   * @param old_scheduler if this is not NULL, the current scheduler
   * is stored in the pointed location so that it can be restored
   * using sched_leave().
   *
   * @return zero if there is no error and the thread is now a
   * SCHED_DEADLINE thread having the specified WCET and deadline, -1
   * if the caller has no sufficient privilege to perform this
   * operation, or -2 in case of hard error that requires the
   * investigation of the output of the logging facility to fix the
   * error.
   */
  int sched_deadline_enter(const relative_time *wcet,
                           const relative_time *deadline,
                           struct scheduler *old_scheduler);

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
  int sched_deadline_leave(struct scheduler *sched_to_be_restored);
  /** @} End of collection of functions to enter real-time mode */

  /* II */
  /**
   * @name Collection of functions to work with SCHED_DEADLINE parameters.
   * @{
   */

  /**
   * @return the scheduling bandwidth of the given scheduling extended
   * parameters.
   */
  double sched_deadline_bandwidth(struct sched_param_ex *param_ex);
  /** @} End of collection of functions to work with SCHED_DEADLINE params. */

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_SCHED_DEADLINE */
