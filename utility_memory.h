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
 * @file utility_memory.h
 * @brief Various functionalities for a process/thread to deal
 * conveniently with the system's memory.
 * @author Tadeus Prastowo <eus@member.fsf.org>
 */

#ifndef UTILITY_MEMORY
#define UTILITY_MEMORY

#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include "utility_log.h"

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * Lock all memory of the caller that is allocated before and after
   * this call to the main memory avoiding costly page fault.
   *
   * @return 0 if the memory can be locked successfully, -1 if the
   * caller memory exceeds the limit of memory that can be locked, -2
   * if the caller has insufficient privilege, or -3 in case of hard
   * error that requires the investigation of the output of the
   * logging facility to fix the error.
   */
  int memory_lock(void);

  /**
   * Preallocate a number of 4096-bytes blocks so that a real-time
   * thread does not encounter page fault when making function calls.
   *
   * @param stack_block_count the number of 4096-bytes to be
   * pre-allocated.
   */
  void memory_preallocate_stack(size_t stack_block_count);

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_MEMORY */
