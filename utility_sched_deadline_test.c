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

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "utility_testcase.h"
#include "utility_log.h"
#include "utility_sched_fifo.h"
#include "utility_sched_deadline.h"
#include "utility_time.h"

MAIN_UNIT_TEST_BEGIN("utility_sched_deadline_test", "stderr", NULL, NULL)
{
  gracious_assert(sched_fifo_enter_max(NULL) == 0);

  int expected_old_policy;
  struct sched_param expected_old_param;
  gracious_assert((expected_old_policy = sched_getscheduler(0)) != -1);
  gracious_assert(sched_getparam(0, &expected_old_param) == 0);

  /* Enter SCHED_DEADLINE */
  struct scheduler old_sched;
  relative_time *expected_wcet = to_utility_time_dyn(25, ms);
  utility_time_set_gc_manual(expected_wcet);
  relative_time *expected_dl = to_utility_time_dyn(200, ms);
  utility_time_set_gc_manual(expected_dl);
  gracious_assert(sched_deadline_enter(expected_wcet, expected_dl, &old_sched)
                  == 0);

  /* Check that SCHED_DEADLINE is really entered */
  gracious_assert(sched_getscheduler(0) == SCHED_DEADLINE);
  struct sched_param_ex new_param_ex;
  gracious_assert(sched_getparam_ex(0, &new_param_ex) == 0);
  utility_time *wcet = timespec_to_utility_time_dyn(&new_param_ex.sched_runtime
                                                    );
  utility_time *dl = timespec_to_utility_time_dyn(&new_param_ex.sched_deadline);
  gracious_assert(utility_time_eq_gc(wcet, expected_wcet));
  gracious_assert(utility_time_eq_gc(dl, expected_dl));
  gracious_assert(sched_deadline_bandwidth(&new_param_ex) - 0.125 <= 1e-15);

  /* Leave SCHED_DEADLINE */
  gracious_assert(sched_deadline_leave(&old_sched) == 0);

  /* Check that SCHED_DEADLINE is really left */
  gracious_assert(sched_getscheduler(0) == expected_old_policy);
  struct sched_param old_param;
  gracious_assert(sched_getparam(0, &old_param) == 0);
  gracious_assert(memcmp(&old_param, &expected_old_param,
                         sizeof(struct sched_param)) == 0);

  /* Clean-up */
  utility_time_gc(expected_wcet);
  utility_time_gc(expected_dl);

  return EXIT_SUCCESS;

} MAIN_UNIT_TEST_END
