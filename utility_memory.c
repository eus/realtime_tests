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

#include "utility_memory.h"

int memory_lock(void)
{
  if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
    switch (errno) {
    case ENOMEM:
      return -1;
    case EPERM:
      return -2;
    default:
      log_syserror("Cannot lock all memory");
      return -3;
    }
  }

  return 0;
}

void memory_preallocate_stack(size_t stack_block_count)
{
  char memory[4096];

  memset(memory, 0xFF, sizeof(memory));

  if (stack_block_count == 0) {
    return;
  }

  return memory_preallocate_stack(stack_block_count - 1);
}
