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

#include "utility_sched_fifo.h"

int sched_fifo_enter(int prio, struct scheduler *old_scheduler)
{
  /* Save the old scheduler */
  if (sched_save(old_scheduler) != 0) {
    log_error("Cannot save the old scheduler for restoration");
    return -2;
  }
  /* End of saving the old scheduler */

  /* Set SCHED_FIFO RT */
  struct sched_param new_sched = {
    .sched_priority = prio,
  };
  if ((errno = pthread_setschedparam(pthread_self(),
                                     SCHED_FIFO, &new_sched)) != 0) {
    if (errno == EPERM) {
      return -1;
    }
    log_syserror("Cannot schedule using SCHED_FIFO with priority %d", prio);
    return -2;
  }
  /* End of setting SCHED_FIFO RT */

  return 0;
}

int sched_fifo_enter_max(struct scheduler *old_scheduler)
{
  int max_prio = -1;
  if (sched_fifo_prio(0, &max_prio) != 0) {
    log_error("Cannot obtain the maximum priority");
    return -2;
  }

  return sched_fifo_enter(max_prio, old_scheduler);
}

int sched_fifo_leave(struct scheduler *sched_to_be_restored)
{
  return sched_restore(sched_to_be_restored);
}

int sched_fifo_prio(unsigned nth_level, int *result)
{
  int max_prio = sched_get_priority_max(SCHED_FIFO);
  if (max_prio == -1) {
    log_syserror("Cannot obtain the maximum priority of SCHED_FIFO");
    return -2;
  }
  int min_prio = sched_get_priority_min(SCHED_FIFO);
  if (min_prio == -1) {
    log_syserror("Cannot obtain the minimum priority of SCHED_FIFO");
    return -2;
  }

  int actual_prio = max_prio - nth_level;

  if (actual_prio < min_prio) {
    return -1;
  }

  *result = actual_prio;
  return 0;
}
