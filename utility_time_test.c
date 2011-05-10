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

#define _GNU_SOURCE /* fmemopen() */

#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utility_testcase.h"
#include "utility_time.h"
#include "utility_log.h"

MAIN_UNIT_TEST_BEGIN("utility_time_test", "stderr", NULL, NULL)
{
  utility_time internal_t;
  utility_time_init(&internal_t);

  /* Testcase 1 */
  struct timespec t;
  to_utility_time(3, s, &internal_t);
  to_timespec(&internal_t, &t);
  gracious_assert((t.tv_sec == 3) && (t.tv_nsec == 0));

  /* Testcase 2 */
  to_utility_time(4, us, &internal_t);
  to_timespec(&internal_t, &t);
  gracious_assert((t.tv_sec == 0) && (t.tv_nsec == 4000));

  /* Testcase 3 */
  to_utility_time(7, ms, &internal_t);
  to_timespec(&internal_t, &t);
  gracious_assert((t.tv_sec == 0) && (t.tv_nsec == 7000000));

  /* Testcase 4 */
  to_timespec_gc(to_utility_time_dyn(1273822, us), &t);
  gracious_assert((t.tv_sec == 1) &&(t.tv_nsec == 273822000));

  /* Testcase 5 */
  to_timespec_gc(to_utility_time_dyn(1273822, ns), &t);
  gracious_assert((t.tv_sec == 0) &&(t.tv_nsec == 1273822));

  /* Testcase 6 */
  t.tv_sec = 4;
  t.tv_nsec = 1;
  utility_time_add_gc(to_utility_time_dyn(4300, ms),
		      timespec_to_utility_time_dyn(&t), &internal_t);
  gracious_assert((t.tv_sec == 4) &&(t.tv_nsec == 1));
  to_timespec(&internal_t, &t);
  gracious_assert((t.tv_sec == 8) &&(t.tv_nsec == 300000001));

  /* Testcase 7 */
  struct timespec t_f;
  t_f.tv_sec = 0;
  t_f.tv_nsec = 999999999;
  t.tv_sec = 0;
  t.tv_nsec = 2;
  utility_time_add_gc(timespec_to_utility_time_dyn(&t_f),
		      timespec_to_utility_time_dyn(&t), &internal_t);
  gracious_assert((t_f.tv_sec == 0) &&(t_f.tv_nsec == 999999999));
  gracious_assert((t.tv_sec == 0) &&(t.tv_nsec == 2));
  to_timespec(&internal_t, &t);
  gracious_assert((t.tv_sec == 1) &&(t.tv_nsec == 1));

  /* Testcase 8 */
  t_f.tv_sec = 45694;
  t_f.tv_nsec = 74494892;
  t.tv_sec = 1;
  t.tv_nsec = 0;
  gracious_assert((t_f.tv_sec == 45694) &&(t_f.tv_nsec == 74494892));
  gracious_assert((t.tv_sec == 1) &&(t.tv_nsec == 0));
  utility_time_add_gc(timespec_to_utility_time_dyn(&t_f),
		      timespec_to_utility_time_dyn(&t), &internal_t);
  to_timespec(&internal_t, &t);
  gracious_assert((t.tv_sec == 45695) &&(t.tv_nsec == 74494892));

  /* Testcase 9 */
  utility_time *internal_t_dyn
    = utility_time_add_dyn_gc(to_utility_time_dyn(7777, ns),
			      utility_time_add_dyn_gc(to_utility_time_dyn(8888,
									  us),
						      to_utility_time_dyn(1,
									  ms)));
  to_timespec_gc(internal_t_dyn, &t);
  gracious_assert((t.tv_sec == 0) &&(t.tv_nsec == 9895777));

  /* Testcase 10 */
  internal_t_dyn = utility_time_add_dyn_gc(to_utility_time_dyn(7777, ns),
  					   to_utility_time_dyn(8888, us));
  utility_time *internal_t_dyn_2
    = utility_time_add_dyn_gc(to_utility_time_dyn(1, s),
  			      to_utility_time_dyn(1, ns));
  utility_time_set_gc_manual(internal_t_dyn);
  to_timespec_gc(utility_time_add_dyn_gc(internal_t_dyn, internal_t_dyn_2), &t);
  gracious_assert((t.tv_sec == 1) &&(t.tv_nsec == 8895778));
  utility_time_gc(internal_t_dyn);

  /* Testcase 11 */
  to_utility_time(8000, us, &internal_t);
  utility_time_inc_gc(&internal_t, to_utility_time_dyn(24, ms));
  to_timespec(&internal_t, &t);
  gracious_assert((t.tv_sec == 0) && (t.tv_nsec == 32000000));

  /* Testcase 12 */
  gracious_assert(utility_time_eq_gc(to_utility_time_dyn(4, ms),
				     to_utility_time_dyn(4, ms)));

  /* Testcase 13 */
  gracious_assert(utility_time_ne_gc(to_utility_time_dyn(4, s),
				     to_utility_time_dyn(4, ms)));

  /* Testcase 14 */
  gracious_assert(utility_time_gt_gc(to_utility_time_dyn(4, s),
				     to_utility_time_dyn(4, ms)));

  /* Testcase 15 */
  gracious_assert(utility_time_ge_gc(to_utility_time_dyn(4, s),
				     to_utility_time_dyn(4, ms)));

  /* Testcase 16 */
  gracious_assert(utility_time_le_gc(to_utility_time_dyn(4, us),
				     to_utility_time_dyn(4, us)));

  /* Testcase 17 */
  gracious_assert(utility_time_lt_gc(to_utility_time_dyn(4, ns),
				     to_utility_time_dyn(4, us)));

  /* Testcase 18 */
  to_timespec_gc(utility_time_sub_dyn_gc(to_utility_time_dyn(3, us),
					 to_utility_time_dyn(3, us)), &t);
  gracious_assert((t.tv_sec == 0) && (t.tv_nsec == 0));

  /* Testcase 19 */
  to_timespec_gc(utility_time_sub_dyn_gc(to_utility_time_dyn(3, ns),
					 to_utility_time_dyn(3, us)), &t);
  gracious_assert((t.tv_sec == 0) && (t.tv_nsec == 0));

  /* Testcase 20 */
  to_timespec_gc(utility_time_sub_dyn_gc(to_utility_time_dyn(3, us),
					 to_utility_time_dyn(3, ns)), &t);
  gracious_assert((t.tv_sec == 0) && (t.tv_nsec == 2997));

  /* Testcase 21 */
  to_timespec_gc(utility_time_sub_dyn_gc(to_utility_time_dyn(3000300999ULL, ns),
					 to_utility_time_dyn(3, s)), &t);
  gracious_assert((t.tv_sec == 0) && (t.tv_nsec == 300999));

  /* Testcase 22 */
  internal_t_dyn = to_utility_time_dyn(3000300999ULL, ns);
  utility_time_sub(internal_t_dyn, internal_t_dyn, internal_t_dyn);
  to_timespec_gc(internal_t_dyn, &t);
  gracious_assert((t.tv_sec == 0) && (t.tv_nsec == 0));

  /* Testcase 23 */
  char test_out_stream_buffer[1024];
  FILE *test_out_stream = fmemopen(test_out_stream_buffer,
				   sizeof(test_out_stream_buffer), "w");
  if (test_out_stream == NULL) {
    fatal_syserror("Unable to open test_out_stream");
  }
  to_file_string_gc(to_utility_time_dyn(4004, ms), test_out_stream);
  if (fclose(test_out_stream) != 0) {
    fatal_syserror("Unable to close test_out_stream");
  }
  gracious_assert(strcmp(test_out_stream_buffer, "4.004000000") == 0);

  /* Testcase 24 */
  char *endptr;
  char *str = "45694";
  internal_t_dyn = string_to_utility_time_dyn(str, &endptr);
  gracious_assert(utility_time_eq_gc(to_utility_time_dyn(45694, s),
				     internal_t_dyn));
  gracious_assert(endptr == &str[strlen(str)]);

  /* Testcase 25 */
  str = ".45694";
  internal_t_dyn = string_to_utility_time_dyn(str, &endptr);
  gracious_assert(utility_time_eq_gc(to_utility_time_dyn(0, s),
				     internal_t_dyn));
  gracious_assert(endptr == str);

  /* Testcase 26 */
  str = "0.45694";
  internal_t_dyn = string_to_utility_time_dyn(str, &endptr);
  gracious_assert(utility_time_eq_gc(to_utility_time_dyn(456940, us),
				     internal_t_dyn));
  gracious_assert(endptr == &str[strlen(str)]);

  /* Testcase 26 */
  str = "9999.999999999";
  internal_t_dyn = string_to_utility_time_dyn(str, &endptr);
  gracious_assert(utility_time_eq_gc(to_utility_time_dyn(9999999999999ULL, ns),
				     internal_t_dyn));
  gracious_assert(endptr == &str[strlen(str)]);

  /* Testcase 27 */
  str = "45. It is the fastest.";
  internal_t_dyn = string_to_utility_time_dyn(str, &endptr);
  gracious_assert(utility_time_eq_gc(to_utility_time_dyn(45, s),
				     internal_t_dyn));
  gracious_assert(endptr == &str[2]);

  /* Testcase 28 */
  to_utility_time(7777, ms, &internal_t);
  to_timespec_gc(utility_time_to_utility_time_dyn(&internal_t), &t);
  gracious_assert((t.tv_sec == 7) && (t.tv_nsec == 777000000));

  return EXIT_SUCCESS;

} MAIN_UNIT_TEST_END
