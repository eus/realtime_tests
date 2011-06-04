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

#include "utility_sched.h"

int sched_save(struct scheduler *curr_scheduler)
{
  if (curr_scheduler == NULL) {
    return 0;
  }

  if ((errno = pthread_getschedparam(pthread_self(),
                                     &curr_scheduler->policy,
                                     &curr_scheduler->param)) != 0) {
    log_syserror("Cannot save current scheduler");
    return -1;
  }
  if (curr_scheduler->policy == SCHED_DEADLINE) {
    /* This code block pretty much requires Linux */
    if (sched_getparam_ex(0, &curr_scheduler->param_ex) != 0) {
      log_syserror("Cannot save SCHED_DEADLINE scheduler");
      return -1;
    }
  }

  return 0;
}

int sched_restore(struct scheduler *sched)
{
  if (sched->policy == SCHED_DEADLINE) {
    /* This code block pretty much requires Linux */
    pid_t thread_id = syscall(SYS_gettid);

    if (syscall(SYS_sched_setscheduler_ex, thread_id, SCHED_DEADLINE,
                sizeof(sched->param_ex), &sched->param_ex) != 0) {
      log_syserror("Cannot restore SCHED_DEADLINE scheduler");
      return -1;
    }
  } else {
    if ((errno = pthread_setschedparam(pthread_self(),
                                       sched->policy, &sched->param)) != 0) {
      log_syserror("Cannot restore the given scheduler");
      return -1;
    }
  }

  return 0;
}

int sched_getparam_ex(pid_t thread_id, struct sched_param_ex *param_ex)
{
  if (thread_id == 0) {
    thread_id = syscall(SYS_gettid);
  }

  if (syscall(SYS_sched_getparam_ex, thread_id, sizeof(*param_ex), param_ex)
      != 0) {
    return -1;
  }

  return 0;
}
