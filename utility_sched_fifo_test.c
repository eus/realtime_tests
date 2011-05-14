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

#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>
#include "utility_testcase.h"
#include "utility_sched_fifo.h"

MAIN_UNIT_TEST_BEGIN("utility_sched_fifo_test", "stderr", NULL, NULL)
{
  const int max_prio = sched_get_priority_max(SCHED_FIFO);
  const int min_prio = sched_get_priority_min(SCHED_FIFO);

  /* Testcase 1 */
  int actual_prio = -1;
  gracious_assert(sched_fifo_prio(max_prio - min_prio + 1,
				  &actual_prio) == -1);

  /* Testcase 2 */
  gracious_assert(sched_fifo_prio(0, &actual_prio) == 0);
  gracious_assert(actual_prio == sched_get_priority_max(SCHED_FIFO));

  /* Testcase 3 */
  int expected_old_policy;
  struct sched_param expected_old_param;
  gracious_assert((expected_old_policy = sched_getscheduler(0)) != -1);
  gracious_assert(sched_getparam(0, &expected_old_param) == 0);

  /* Enter RT */
  struct scheduler old_sched;
  gracious_assert(sched_fifo_enter_max(&old_sched) == 0);

  /* Check that RT is really entered */
  gracious_assert(sched_getscheduler(0) == SCHED_FIFO);
  struct sched_param new_param;
  gracious_assert(sched_getparam(0, &new_param) == 0);
  gracious_assert(new_param.sched_priority
		  == sched_get_priority_max(SCHED_FIFO));

  /* Leave RT */
  gracious_assert(sched_fifo_leave(&old_sched) == 0);

  /* Check that RT is really left */
  gracious_assert(sched_getscheduler(0) == expected_old_policy);
  struct sched_param old_param;
  gracious_assert(sched_getparam(0, &old_param) == 0);
  gracious_assert(memcmp(&old_param, &expected_old_param,
			 sizeof(struct sched_param)) == 0);

  return EXIT_SUCCESS;

} MAIN_UNIT_TEST_END
