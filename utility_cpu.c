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
#include <stdlib.h>
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

  if (buffer != NULL) {
    free(buffer);
  }
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

#define LINUX_FREQUENCIES_FILE_PATH_FORMAT				\
  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_available_frequencies"

unsigned long long *cpu_freq_available(int which_cpu, ssize_t *list_len)
{
  *list_len = -1;

  /* Set the correct file path */
  char linux_frequencies_file_path[1024];
  snprintf(linux_frequencies_file_path, sizeof(linux_frequencies_file_path),
	   LINUX_FREQUENCIES_FILE_PATH_FORMAT, which_cpu);
  /* End of setting the correct file path */

  /* Open the frequency file */
  FILE *linux_frequencies_file
    = utility_file_open_for_reading(linux_frequencies_file_path);
  if (linux_frequencies_file == NULL) {
    log_syserror("Cannot open '%s' for reading", linux_frequencies_file_path);
    return NULL;
  }
  /* End of opening the frequency file */

  /* Read the available frequencies */
  char *buffer = NULL;
  size_t buffer_len = 0;
  int rc = utility_file_readln(linux_frequencies_file,
			       &buffer, &buffer_len, 256);
  if (rc == -2) {
    log_error("Cannot read '%s'", linux_frequencies_file_path);
    goto error;
  } else if (rc == -1) {
    *list_len = 0;
    goto error;
  } else {
    utility_file_close(linux_frequencies_file, linux_frequencies_file_path);
  }
  /* End of reading the available frequencies */

  /* Count the number of available frequencies */
  size_t frequency_count = 0;
  char *ptr = buffer;
  while (*ptr != '\0') {
    if (isspace(*ptr)) {
      frequency_count++;
    }
    ptr++;
  }
  /* End of counting the number of available frequencies */

  /* Create the result */
  *list_len = frequency_count;
  unsigned long long *result = NULL;
  if (frequency_count != 0) {
    result = malloc(sizeof(unsigned long long) * frequency_count);
    int result_idx = 0;
    char *ptr = buffer;
    char *ptr_start = buffer;
    while (*ptr != '\0') {
      if (isspace(*ptr)) {
	*ptr = '\0';
	result[result_idx++] = strtoull(ptr_start, NULL, 10) * 1000;
	ptr++;
	ptr_start = ptr;
      } else {
	ptr++;
      }
    }
  }
  /* End of creating the result */

  if (buffer != NULL) {
    free(buffer);
  }

  return result;

 error:
  utility_file_close(linux_frequencies_file, linux_frequencies_file_path);
  if (buffer != NULL) {
    free(buffer);
  }
  return NULL;
}
