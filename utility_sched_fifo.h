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
 * @file utility_sched_fifo.h
 * @brief Various convenient functions to use SCHED_FIFO of POSIX RT API.
 * @author Tadeus Prastowo <eus@member.fsf.org>
 */

#ifndef UTILITY_SCHED_FIFO
#define UTILITY_SCHED_FIFO

#include <sched.h>

#ifdef __cplusplus
extern "C" {
#endif

  /* I. Main data structures */
  /** Wrapper over arguments to sched_setscheduler. */
  struct scheduler
  {
    int policy; /**< POSIX RT scheduling policy */
    struct sched_param param; /**< POSIX RT scheduling parameter */
  };
  /* End of main data structures */

  /* II */
  /**
   * @name Collection of functions to enter and leave SCHED_FIFO real-time mode.
   * @{
   */

  /**
   * Schedule the caller as a SCHED_FIFO real-time thread with the
   * given priority.
   *
   * @param prio the desired priority (c.f., sched_fifo_prio()).
   * @param old_scheduler if this is not NULL, the current scheduler
   * is stored in the pointed location so that it can be restored
   * using sched_fifo_leave().
   *
   * @return zero if there is no error and the thread is now a
   * SCHED_FIFO thread having the specified priority, -1 if the caller
   * has no sufficient privilege to perform this operation, or -2 in
   * case of hard error that requires the investigation of the output
   * of the logging facility to fix the error.
   */
  int sched_fifo_enter(int prio, struct scheduler *old_scheduler);

  /**
   * Schedule the caller as a SCHED_FIFO real-time thread with the
   * highest priority.
   *
   * @param old_scheduler if this is not NULL, the current scheduler
   * is stored in the pointed location so that it can be restored
   * using sched_fifo_leave().
   *
   * @return zero if there is no error and the thread is now a
   * SCHED_FIFO thread having the highest priority, -1 if the caller
   * has no sufficient privilege to perform this operation, or -2 in
   * case of hard error that requires the investigation of the output
   * of the logging facility to fix the error.
   */
  int sched_fifo_enter_max(struct scheduler *old_scheduler);

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
  int sched_fifo_leave(struct scheduler *sched_to_be_restored);
  /** @} End of collection of functions to enter and leave real-time mode */

  /* III */
  /**
   * @name Collection of functions to deal with SCHED_FIFO RT priority.
   * @{
   */

  /**
   * Return SCHED_FIFO RT priority nth_level lower than the maximum
   * one. So, <code>sched_fifo_prio(0, &result)</code> is equivalent
   * to <code>result = sched_get_priority_max(SCHED_FIFO)</code>.
   *
   * @param nth_level how many steps lower relative to the maximum one
   * the priority should be.
   * @param result a pointer to the location to store the actual priority.
   *
   * @return zero if the actual priority can be successfully stored in
   * result, -1 if nth_level is too low, or -2 in case of hard error
   * that requires the investigation of the output of the logging
   * facility to fix the error.
   */
  int sched_fifo_prio(unsigned nth_level, int *result);
  /** @} End of collection of functions to deal with RT priority. */

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_SCHED_FIFO */
