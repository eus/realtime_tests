#define _GNU_SOURCE

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <sched.h>
#include "utility_time.h"

unsigned long cpu_speed(void)
{
  FILE *f;
  char line[160];

  f = fopen("/proc/cpuinfo", "r");
  if (f == NULL) {
    return 0;
  }

  while(!feof(f)) {
    char *res;

    res = fgets(line, 160, f);
    if (res != NULL) {
      if (memcmp(line, "cpu MHz\t\t: ", 11) == 0) {
        float s;
        s = atof(line + 10);
        fclose(f);
        return s * 1000.0;
      }
    }
  }

  fclose(f);

  return 0;
}

int main(int argc, char **argv, char **envp)
{
  FILE *gov_file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "r");
  if (gov_file != NULL) {
    char buffer[1024];
    assert(fgets(buffer, sizeof(buffer), gov_file) != NULL);
    assert(strcmp(buffer, "performance\n") == 0);
    fclose(gov_file);
  } else {
    fprintf(stderr, "Please make sure CPU frequency is fixed (not dynamically scaled)\n");
  }

  cpu_set_t affinity_mask;
  CPU_ZERO(&affinity_mask);
  CPU_SET(0, &affinity_mask);
  assert(sched_setaffinity(0, sizeof(affinity_mask), &affinity_mask) == 0);

  unsigned long hz = cpu_speed() * 1000;

  struct sched_param prm = {
    .sched_priority = sched_get_priority_max(SCHED_FIFO),
  };
  assert(prm.sched_priority != -1);
  assert(sched_setscheduler(0, SCHED_FIFO, &prm) == 0);

  int rc = 0;
  struct timespec t_start, t_finish;
  rc -= clock_gettime(CLOCK_MONOTONIC, &t_start);
  unsigned long i = 0;
  asm("0:\n\t"
      "addl $1, %0\n\t"
      "cmpl %1, %0\n\t"
      "jne 0b" : : "r" (i), "r" (hz));
  rc -= clock_gettime(CLOCK_MONOTONIC, &t_finish);

  relative_time *t
    = utility_time_sub_dyn_gc(timespec_to_utility_time_dyn(&t_finish),
                              timespec_to_utility_time_dyn(&t_start));
  to_file_string_gc(t, stdout);
  printf("\n");

  assert(rc == 0);

  return EXIT_SUCCESS;
}
