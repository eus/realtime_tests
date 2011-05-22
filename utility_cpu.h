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

#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include "utility_log.h"
#include "utility_file.h"
#include "utility_time.h"
#include "utility_sched_fifo.h"

#ifdef __cplusplus
extern "C" {
#endif

  /* I */
  /**
   * @name Collection of functions to deal with thread's CPU affinity.
   * @{
   */

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
  static inline int enter_UP_mode(void)
  {
    return lock_me_to_cpu(0);
  }

  /**
   * Set the calling thread to be migrateable to all CPUs.
   *
   * @return zero if there is no error or -1 in case of hard error
   * that requires the investigation of the output of the logging
   * facility to fix the error
   */
  int unlock_me(void);

  /**
   * @return the ID of the last processor in the system as an integer
   * greater than or equal to 0 or -1 in case of hard error that
   * requires the investigation of the output of the logging facility
   * to fix the error.
   */
  int get_last_cpu(void);
  /** @} End of collection of functions to deal with thread's CPU affinity */

  /* II */
  /**
   * @name Collection of functions to set a CPU frequency.
   * @{
   */

  /** An opaque data type of a CPU frequency governor object. */
  typedef struct {
    int which_cpu;
    char *linux_cpu_governor_name;
  } cpu_freq_governor;

  /**
   * @return a pointer to the current CPU governor of the specified
   * CPU ID so that the governor can be restored using
   * cpu_freq_restore_governor() because cpu_freq_set() may change the
   * governor of the current CPU. The returned object should be freed
   * with destroy_cpu_freq_governor() if restoration will not be
   * performed. NULL is returned in case of hard error that requires
   * the investigation of the output of the logging facility to fix
   * the error.
   */
  cpu_freq_governor *cpu_freq_get_governor(int which_cpu);

  /**
   * @return zero if the given governor can be restored to the
   * previously governed CPU. Upon successful restoration, the
   * governor object is freed. If restoration cannot be performed due
   * to insufficient privilege, -2 is returned. For other errors, -1
   * is returned and the error is @ref utility_log.h "logged". An
   * already destroyed or restored governor object must not be passed
   * again to this function.
   */
  int cpu_freq_restore_governor(cpu_freq_governor *governor);

  /**
   * Destroy the given cpu_freq_governor object. A destroyed or
   * restored governor object must not be passed again to this
   * function.
   */
  void destroy_cpu_freq_governor(cpu_freq_governor *governor);

  /**
   * List all available frequencies in Hz in descending order of the
   * given CPU.
   *
   * @param which_cpu the CPU ID of the CPU whose frequency is to be
   * listed.
   * @param list_len the number of entries in the returned list.
   *
   * @return a dynamically allocated list of frequencies or NULL if
   * there is an error (the error is @ref utility_log.h "logged"
   * directly) in which case list_len is set to -1. The caller is
   * responsible for freeing the returned list. NULL is also returned
   * if the list is empty in which case list_len is set to 0.
   */
  unsigned long long *cpu_freq_available(int which_cpu, ssize_t *list_len);

  /**
   * Turn off dynamic CPU frequency scaling in the given CPU and set
   * its frequency to the specified one. The frequency must be one of
   * those returned by cpu_freq_available().
   *
   * It is <strong>recommended</strong> to call
   * cpu_freq_get_governor() before calling this function and to call
   * cpu_freq_restore_governor() after the code section that needs a
   * particular CPU frequency because this function may change the
   * governor of the CPU if it returns zero.
   *
   * @param which_cpu the CPU ID of the CPU to be set.
   * @param new_freq the desired frequency.
   *
   * @return zero if the frequency can be successfully set, -2 if the
   * calling thread has insufficient privilege to set the CPU
   * frequency, or -1 for hard error that requires the investigation
   * of the output of the logging facility to fix the error.
   */
  int cpu_freq_set(int which_cpu, unsigned long long new_freq);

  /**
   * @return the CPU frequency of the given CPU. The frequency will be
   * one of those returned by cpu_freq_available(). Zero is returned
   * in case of hard error that requires the investigation of the
   * output of the logging facility to fix the error.
   */
  unsigned long long cpu_freq_get(int which_cpu);
  /** @} End of collection of functions to deal with CPU frequency */

  static inline void busyloop(unsigned long loop_count)
  {
    unsigned long i = 0;
    asm("0:\n\t"
        "addl $1, %0\n\t"
        "cmpl %1, %0\n\t"
        "jne 0b" : : "r" (i), "r" (loop_count));
  }

  /* III */
  /**
   * @name Collection of functions to use a CPU in a certain way.
   * @{
   */

  /** An opaque data type of the object to be passed to run_cpu_busyloop. */
  typedef struct {
    int which_cpu; /* The ID of the CPU on which the measurement took place */
    unsigned long long frequency; /* The frequency at which the
                                     measurement took place */
    unsigned long loop_count; /* The number of loop */
    utility_time duration; /* The duration that the busyloop should yield */
  } cpu_busyloop;

  /**
   * @return the CPU ID that the cpu_busyloop object is associated with.
   */
  int cpu_busyloop_id(const cpu_busyloop *obj);
  /**
   * @return the CPU frequency at which the cpu_busyloop object was
   * created.
   */
  unsigned long long cpu_busyloop_frequency(const cpu_busyloop *obj);
  /**
   * @return a dynamically allocated utility_time object containing
   * the duration of the given cpu_busyloop object.
   */
  utility_time *cpu_busyloop_duration(const cpu_busyloop *obj);

  /**
   * A better way is to only have keep_cpu_busy() that has the goal of
   * keeping the state of the thread to RUNNING so that the kernel
   * scheduler will not let a lower priority thread to run for a
   * certain duration passed as the argument to
   * keep_cpu_busy(). However, such a keep_cpu_busy() is only possible
   * when a busy loop can be performed and interrupted upon the
   * delivery of a SIGALRM. This is because the use of function like
   * clock_nanosleep will change the calling thread status from
   * RUNNING to something like WAIT_INTERRUPTIBLE allowing a lower
   * priority thread to run. Unfortunately, SIGALRM is targeted to a
   * process not a thread necessiting the use of a complicated
   * process-wide signal handling function to deliver the signal to
   * the right thread. The use of function like timerfd_create() does
   * not help because it requires the use of select() that puts the
   * calling thread to something like WAIT_INTERRUPTIBLE for a short
   * time giving a little hole for a lower priority thread to run. A
   * non-portable solution would be to patch the kernel so that the
   * calling thread can be given state RUNNING for a certain
   * duration. Therefore, a quite portable way to keep a CPU busy is
   * to do a busy loop with a certain number of repetitions.
   *
   * Keeping a CPU busy with a busy loop using a certain number of
   * repetitions, however, can only work well when the CPU frequency
   * is fixed. This is the reason why a collection of functions to set
   * the CPU frequency exists. You may want to use cpu_freq_set()
   * before calling this function.
   *
   * Because searching for the right number of repetitions involves a
   * trade off between accuracy and computation time, search_tolerance
   * and search_max_passes are used to bound the search effort. For
   * example, if the desired duration is 1 s, a search_tolerance of 1
   * ms will stop the search once a number of repetitions is found to
   * yield a duration between 0.999 s and 1.001 s, inclusive. Since
   * the search may involve some repetitions of the search algorithm,
   * the search will stop after being repeated for search_max_passes
   * times. That is, the search will stop once either a number of
   * repetitions is found to yield a duration between (duration -
   * search_tolerance) and (duration + search_tolerance), inclusive,
   * or the search algorithm has been repeated for search_max_passes
   * times.
   *
   * Please keep in mind that at worst, this function will
   * approximately last for the desired duration times
   * search_max_passes.
   *
   * To acquire the most accurate measurement, this function will run
   * as a real-time thread with the highest priority possible while
   * disallowing any preemption and CPU migration. Therefore, the
   * caller must have a sufficient privilege to do the aforementioned
   * things and, if care is not taken, the caller may freeze the whole
   * machine.
   *
   * The above behavior implies that the number of repetitions to
   * yield the desired duration is measured without preemption (i.e.,
   * context-switch) as a real-time thread with the highest possible
   * real-time priority.
   *
   * @param which_cpu the CPU ID of the CPU whose number of busy loop
   * repetitions is to be determined.
   * @param duration a pointer to utility_time object specifying the
   * desired duration of the busy loop. If this function returns zero
   * and the utility_time object is marked for automatic garbage
   * collection, it will be garbage collected.
   * @param search_tolerance a pointer to utility_time object
   * specifying the tolerated deviations from the desired duration. If
   * this function returns zero and the utility_time object is marked
   * for automatic garbage collection, it will be garbage collected.
   * @param search_max_passes the maximum number of repetitions that
   * the search algorithm is allowed to make.
   * @param result a pointer to a dynamically allocated object to be
   * passed to run_cpu_busyloop() to keep the CPU busy for the
   * specified duration at the current frequency of the CPU. The
   * caller is responsible to free the object using
   * destroy_cpu_busyloop().
   *
   * @return zero if there is no error and result can be successfully
   * created, -1 if the caller is not privileged to use a real-time
   * scheduler, -2 if the duration is too short, -3 in case of hard
   * error that requires the investigation of the output of the
   * logging facility to fix the error. When the return value is not
   * zero, result is set to NULL, or -4 if the duration is too long.
   */
  int create_cpu_busyloop(int which_cpu, const utility_time *duration,
                          const utility_time *search_tolerance,
                          unsigned search_max_passes,
                          cpu_busyloop **result);

  /**
   * Destroy a cpu_busyloop object. An already destroyed cpu_busyloop
   * object must not be passed again to this function.
   */
  void destroy_cpu_busyloop(cpu_busyloop *arg);

  /**
   * @param arg a pointer to a cpu_busyloop object containing both the
   * CPU ID of the CPU that must be kept busy for a certain time
   * duration and the duration itself. An already destroyed
   * cpu_busyloop object must not be passed to this function.
   */
  static inline void keep_cpu_busy(const cpu_busyloop *arg)
  {
    busyloop(arg->loop_count);
  }

  /**
   * Convenient function to lock the caller to CPU 0 and set the CPU
   * frequency to the maximum.
   *
   * @param default_gov the location of a pointer to the governor of
   * the CPU before the frequency is manually set to the highest. The
   * caller must restore the default_gov using
   * cpu_freq_restore_governor() before ending the program because
   * otherwise, the system governor for the CPU will completely be
   * altered. If the return value is not zero, default_gov is set to
   * NULL.
   *
   * @return zero if the thread is successfully bound to CPU 0 and the
   * CPU frequency can be successfully set to the maximum one, -1 if
   * the caller has insufficient privilege to do one of the
   * aforementioned things, or -2 in case of hard error that requires
   * the investigation of the output of the logging facility to fix
   * the error.
   */
  int enter_UP_mode_freq_max(cpu_freq_governor **default_gov);
  /** @} End of collection of functions to use a CPU in a certain way */

  /* IV */
  /**
   * @name Collection of functions to deal with endianness.
   * @{
   */
  /** Possible byte orders. */
  enum byte_order {
    CPU_LITTLE_ENDIAN,
    CPU_BIG_ENDIAN,
  };
  /**
   * @return the byte order of the host system.
   */
  static inline char host_byte_order(void)
  {
    int probe = 0xBEEF;
    char *ptr = (char *) &probe;
    return (ptr[0] == (char) 0xEF ? CPU_LITTLE_ENDIAN : CPU_BIG_ENDIAN);
  }
  /** @} End of */

#ifdef __cplusplus
}
#endif

#endif /* UTILITY_CPU */
