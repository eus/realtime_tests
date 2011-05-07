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
#include <time.h>
#include "utility_cpu.h"
#include "utility_log.h"
#include "utility_file.h"
#include "utility_time.h"

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
    log_error("Cannot open %s for reading", linux_cpuinfo_path);
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
	&& (isspace(buffer[keyword_len]) || buffer[keyword_len] == ':')) {
      proc_count++;
    }
  }

  if (buffer != NULL) {
    free(buffer);
  }

  utility_file_close(linux_cpuinfo, linux_cpuinfo_path);

  if (rc == -2) {
    log_error("Error while reading %s", linux_cpuinfo_path);
    return -1;
  }

  --proc_count;
  if (proc_count < 0) {
    log_error("/proc/cpuinfo contains no 'processor' key");
  }

  return proc_count;
}

#define LINUX_GOVERNOR_FILE_PATH_FORMAT				\
  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor"

cpu_freq_governor *cpu_freq_get_governor(int which_cpu)
{
  cpu_freq_governor *result = malloc(sizeof (cpu_freq_governor));
  if (result == NULL) {
    log_error("Insufficient memory to create cpu_freq_governor object");
    return NULL;
  }
  result->which_cpu = which_cpu;

  char linux_governor_file_path[1024];
  snprintf(linux_governor_file_path, sizeof(linux_governor_file_path),
	   LINUX_GOVERNOR_FILE_PATH_FORMAT, which_cpu);
  FILE *linux_governor_file
    = utility_file_open_for_reading(linux_governor_file_path);
  if (linux_governor_file == NULL) {
    log_syserror("Cannot open '%s' for reading", linux_governor_file_path);
    free(result);
    return NULL;
  }

  char *buffer = NULL;
  size_t buffer_len = 0;
  int rc = utility_file_readln(linux_governor_file, &buffer, &buffer_len, 32);
  utility_file_close(linux_governor_file, linux_governor_file_path);

  if (rc == 0) {
    result->linux_cpu_governor_name = buffer;
    return result;
  } else if (rc == -1) {
    log_error("No governor exists");
  } else {
    log_error("Fail to read '%s'", linux_governor_file_path);
  }

  free(buffer);
  free(result);
  return NULL;
}

int cpu_freq_restore_governor(cpu_freq_governor *governor)
{
  char linux_governor_file_path[1024];
  snprintf(linux_governor_file_path, sizeof(linux_governor_file_path),
	   LINUX_GOVERNOR_FILE_PATH_FORMAT, governor->which_cpu);

  FILE *linux_governor_file = fopen(linux_governor_file_path, "w");
  if (linux_governor_file == NULL) {
    log_syserror("Cannot open '%s' for writing", linux_governor_file_path);
    if (errno == EACCES) {
      return -2;
    } else {
      return -1;
    }
  }

  fprintf(linux_governor_file, "%s", governor->linux_cpu_governor_name);

  if (utility_file_close(linux_governor_file, linux_governor_file_path) != 0) {
    log_error("Cannot close '%s' after writing", linux_governor_file_path);
    return -1;
  }

  destroy_cpu_freq_governor(governor);
  return 0;
}

void destroy_cpu_freq_governor(cpu_freq_governor *governor)
{
  free(governor->linux_cpu_governor_name);
  free(governor);
}

unsigned long long *cpu_freq_available(int which_cpu, size_t *list_len)
{
  return NULL;
}
