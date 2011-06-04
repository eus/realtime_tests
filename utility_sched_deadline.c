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

#define _GNU_SOURCE /* For fn syscall */

#include "utility_sched_deadline.h"

int sched_deadline_enter(const relative_time *wcet,
                         const relative_time *deadline,
                         struct scheduler *old_scheduler)
{
  /* Save the old scheduler */
  if (sched_save(old_scheduler) != 0) {
    log_error("Cannot save the old scheduler for restoration");
    return -2;
  }
  /* End of saving the old scheduler */

  /* Set SCHED_DEADLINE RT */
  struct sched_param_ex new_sched;
  to_timespec(wcet, &new_sched.sched_runtime);
  to_timespec(deadline, &new_sched.sched_period);
  to_timespec(deadline, &new_sched.sched_deadline);
  new_sched.sched_flags = 0;

  pid_t thread_id = syscall(SYS_gettid);

  if (syscall(SYS_sched_setscheduler_ex, thread_id, SCHED_DEADLINE,
              sizeof(new_sched), &new_sched) != 0) {
    if (errno == EPERM) {
      return -1;
    }
    log_syserror("Cannot schedule using SCHED_DEADLINE with bandwidth %f",
                 sched_deadline_bandwidth(&new_sched));
    return -2;
  }
  /* End of setting SCHED_DEADLINE RT */

  utility_time_gc_auto(wcet);
  utility_time_gc_auto(deadline);

  return 0;
}

int sched_deadline_leave(struct scheduler *sched)
{
  return sched_restore(sched);
}

double sched_deadline_bandwidth(struct sched_param_ex *param_ex)
{
#define NS_IN_SEC 1000000000.0
  double bandwidth = (param_ex->sched_runtime.tv_sec * NS_IN_SEC
                      + param_ex->sched_runtime.tv_nsec);
  bandwidth /= (param_ex->sched_deadline.tv_sec * NS_IN_SEC
                + param_ex->sched_deadline.tv_nsec);

  return bandwidth;
#undef NS_IN_SEC
}
