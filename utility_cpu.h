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
 * @file utility_cpu.h
 * @brief Various functionalities for a process/thread to deal
 * conveniently with the system's processor(s).
 * @author Tadeus Prastowo <eus@member.fsf.org>
 */

#ifndef UTILITY_CPU
#define UTILITY_CPU

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * Only allow the calling thread to run on the specified CPU.
   *
   * @param which_cpu the ID of the processor to which the calling
   * process/thread will be bound to. Valid CPU ID is between 0 and
   * get_last_cpu(), inclusive.
   *
   * @return zero if there is no error or -1 in case of hard error
   * that requires the investigation of the output of the logging
   * facility to fix the error
   */
  int lock_me_to_cpu(int which_cpu);

  /**
   * Is equivalent to lock_me_to_cpu(0);
   */
  static inline void enter_UP_mode(void)
  {
    lock_me_to_cpu(0);
  }

  /**
   * @return the ID of the last processor in the system as an integer
   * greater than or equal to 0 or -1 in case of hard error that
   * requires the investigation of the output of the logging facility
   * to fix the error.
   */
  int get_last_cpu(void);

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_CPU */
