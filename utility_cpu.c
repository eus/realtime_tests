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

#define _GNU_SOURCE /* pthread_setaffinity_np(), CPU_*, etc. */

#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include "utility_cpu.h"
#include "utility_log.h"
#include "utility_file.h"

int lock_me_to_cpu(int which_cpu)
{
  cpu_set_t affinity_mask;

  CPU_ZERO(&affinity_mask);
  CPU_SET(which_cpu, &affinity_mask);

  if (pthread_setaffinity_np(pthread_self(),
			     sizeof(affinity_mask), &affinity_mask) != 0) {
    log_syserror("Cannot set CPU affinity");
    return -1;
  }

  return 0;
}

int get_last_cpu(void)
{
  const char linux_cpuinfo_path[] = "/proc/cpuinfo";
  FILE *linux_cpuinfo = utility_file_open_for_reading(linux_cpuinfo_path);
  if (linux_cpuinfo == NULL) {
    return -1;
  }

  int rc = 0;
  char *buffer = NULL;
  size_t buffer_len = 0;
  int proc_count = 0;
  const char keyword[] = "processor";
  const size_t keyword_len = strlen(keyword);

  while ((rc = utility_file_readln(linux_cpuinfo, &buffer, &buffer_len, 1024))
	 == 0) {
    if (strncmp(buffer, keyword, keyword_len) == 0
	&& isspace(buffer[keyword_len])) {
      proc_count++;
    }
  }

  if (buffer != NULL) {
    free(buffer);
  }

  utility_file_close(linux_cpuinfo, linux_cpuinfo_path);

  if (rc == -2) {
    return -1;
  }

  --proc_count;
  if (proc_count < 0) {
    log_error("/proc/cpuinfo contains no 'processor' key");
  }

  return proc_count;
}
