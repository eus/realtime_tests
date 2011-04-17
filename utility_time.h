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
 * @file utility_time.h
 * @brief various functionalities to deal conveniently with data structures
 * related to various time functions.
 * @author Tadeus Prastowo <eus@member.fsf.org>
 *
 * This file provides various functions to conveniently deal with data
 * structures related to various time functions such as clock_gettime,
 * clock_nanosleep and the like. An example of a convenience that the various
 * functions in this file provides is adding a certain delta expressed in
 * millisecond to a struct timespec object without creating an intermediate
 * struct timespec object as follows:
 * @code
 * to_timespec_gc(utility_time_add_dyn(timespec_to_utility_time_dyn(&t),
 *                                     to_utility_time_dyn(400, ms)),
 *                &t);
 * @endcode
 *
 * To use the various functions, any time object that you have must be converted
 * to the internal representation first. Or, if you define a variable of type
 * utility_time, you must initialize it first. The internal representation is
 * an opaque type. You must not manipulate it directly. Afterwards, you can use
 * the various operational functions before finally converting the internal
 * representation object to a desired time object.
 */

#ifndef UTILITY_TIME
#define UTILITY_TIME

#include <time.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

  /* I. Main data structures */
  /**
   * Various time units.
   */
  enum time_unit {
    s, /**< Second. */
    ms, /**< Millisecond. */
    us, /**< Microsecond. */
    ns, /**< Nanosecond. */
  };

  /**
   * The internal representation of a time.
   * This is an opaque type; do not manipulate any of its instances directly.
   */
  typedef struct {
    struct timespec t; /* The time */
    int dyn_alloc; /* The object was allocated dynamically */
    int manual_gc; /* No automatic garbage collection should be performed */
  } utility_time;
  /* End of main data structures */

  /* II */
  /**
   * @name Collection of maintenance functions.
   * @{
   */
  /**
   * Initialize the internal representation of a time. Every new utility_time
   * object must be initialized. Otherwise, the behaviors of the other functions
   * when applied to the object is undefined. The initialized object will not
   * be garbage collected automatically. If you allocate the object dynamically
   * yourself, you must use utility_time_init_dyn() to initialize the object.
   *
   * @param internal_t a pointer to the object to be initialized.
   */
  static inline void utility_time_init(utility_time *internal_t)
  {
    memset(internal_t, 0, sizeof(*internal_t));
  }
  /**
   * Work just like utility_time_init() but set additional internal states for
   * performing garbage collection.
   *
   * @param internal_t a pointer to the dynamically allocated object to be
   * initialized.
   */
  static inline void utility_time_init_dyn(utility_time *internal_t)
  {
    utility_time_init(internal_t);
    internal_t->dyn_alloc = 1;
  }
  /**
   * Deallocate the given utility_time object if it is applicable (i.e., the
   * object was dynamically allocated).
   *
   * @param internal_t a pointer to the object to be garbage collected.
   */
  static inline void utility_time_gc(const utility_time *internal_t)
  {
    if (internal_t->dyn_alloc) {
      free((void *) internal_t);
    }
  }
  /**
   * Prevent the object from being automatically garbage collected.
   *
   * @param internal_t a pointer to the object to be garbage collected manually.
   */
  static inline void utility_time_set_gc_manual(utility_time *internal_t)
  {
    if (internal_t->dyn_alloc) {
      internal_t->manual_gc = 1;
    }
  }
  /**
   * Make the object subject to automatic garbage collection.
   *
   * @param internal_t a pointer to the object to be garbage collected
   * automatically.
   */
  static inline void utility_time_set_gc_auto(utility_time *internal_t)
  {
    if (internal_t->dyn_alloc) {
      internal_t->manual_gc = 0;
    }
  }
  /** @} End of collection of maintenance functions */

  /* III. Collection of internal helper functions and constants
   * CAUTION: DO NOT use these outside utility_time object; they are subject to
   *          change and deletion
   */
#define BILLION 1000000000
#define MILLION 1000000
#define THOUSAND 1000

  /* Return the second part of the sum of the ns parts of the two internal
   * objects.
   */
  static inline unsigned long combined_ns_in_s(const utility_time *t1,
					       const utility_time *t2)
  {
    return (t1->t.tv_nsec + t2->t.tv_nsec) / BILLION;
  }
  /* Return the ns parts of the sum of the ns parts of the two internal objects.
   */
  static inline unsigned long combined_ns_leftover(const utility_time *t1,
						   const utility_time *t2)
  {
    return (t1->t.tv_nsec + t2->t.tv_nsec) % BILLION;
  }

  /* Garbage collect the given utility_time object if the object allows for
   * automatic garbage collection.
   */
  static inline void utility_time_gc_auto(const utility_time *internal_t)
  {
    if (!internal_t->manual_gc) {
      utility_time_gc(internal_t);
    }
  }

  /* Dynamically allocate a utility_time object. */
  static inline utility_time *utility_time_make_dyn(void)
  {
    utility_time *internal_t = malloc(sizeof(*internal_t));
    if (internal_t != NULL) {
      utility_time_init_dyn(internal_t);
    }
    return internal_t;
  }
  /* End of collection of internal helper functions and constants */

  /* IV */
  /**
   * @name Collection of conversion functions to internal representation.
   * @{
   */
  /**
   * Convert an integer representing a time in the given unit to the internal
   * time representation.
   *
   * @param t the integer representing a time.
   * @param t_unit the unit of the time.
   * @param internal_t a pointer to the utility_time object to store the result.
   */
  static inline void to_utility_time(unsigned long long t,
				     enum time_unit t_unit,
				     utility_time *internal_t)
  {
    switch (t_unit) {
    case s:
      internal_t->t.tv_sec = t;
      internal_t->t.tv_nsec = 0;
      break;
    case ms:
      internal_t->t.tv_sec = t / THOUSAND;
      internal_t->t.tv_nsec = (t % THOUSAND) * MILLION;
      break;
    case us:
      internal_t->t.tv_sec = t / MILLION;
      internal_t->t.tv_nsec = (t % MILLION) * THOUSAND;
      break;
    case ns:
      internal_t->t.tv_sec = t / BILLION;
      internal_t->t.tv_nsec = t % BILLION;
      break;
    }
  }
  /**
   * Work just like to_utility_time() except that it returns the result as
   * a dynamic utility_time object subject to automatic garbage collection.
   */
  static inline utility_time *to_utility_time_dyn(unsigned long long t,
						  enum time_unit t_unit)
  {
    utility_time *internal_t = utility_time_make_dyn();
    if (internal_t != NULL) {
      to_utility_time(t, t_unit, internal_t);
    }
    return internal_t;
  }

  /**
   * Convert a struct timespec object defined in time.h to the internal time
   * representation.
   *
   * @param t a pointer to the struct timespec object.
   * @param internal_t a pointer to the utility_time object to store the result.
   */
  static inline void timespec_to_utility_time(const struct timespec *t,
					      utility_time *internal_t)
  {
    internal_t->t.tv_sec = t->tv_sec;
    internal_t->t.tv_nsec = t->tv_nsec;
  }
  /**
   * Work just like timespec_to_utility_time() except that it returns the result
   * as dynamic utility_time object subject to automatic garbage collection.
   */
  static inline utility_time *timespec_to_utility_time_dyn(const
							   struct timespec *t)
  {
    utility_time *internal_t = utility_time_make_dyn();
    if (internal_t != NULL) {
      timespec_to_utility_time(t, internal_t);
    }
    return internal_t;
  }
  /** @} End of collection of conversion functions to internal representation */

  /* V */
  /**
   * @name Collection of conversion functions to other types.
   * @{
   */
  /**
   * Convert the internal representation of time to a struct timespec object
   * defined in time.h.
   *
   * @param internal_t a pointer to the internal representation of time.
   * @param t a pointer to the timespec object to store the result.
   */
  static inline void to_timespec(const utility_time *internal_t,
				 struct timespec *t)
  {
    t->tv_sec = internal_t->t.tv_sec;
    t->tv_nsec = internal_t->t.tv_nsec;
  }
  /**
   * Work just like to_timespec() but the given internal representation object
   * is garbage collected if automatic garbage collection is permitted.
   */
  static inline void to_timespec_gc(const utility_time *internal_t,
				    struct timespec *t)
  {
    to_timespec(internal_t, t);
    utility_time_gc_auto(internal_t);
  }
  /** @} End of collection of conversion functions to other types */

  /* VI */
  /**
   * @name Collection of operational functions.
   * @{
   */

  /* 1. utility_time_add and variants
   * Overlap is allowed (i.e., res can also be t1 or t2 or both)
   */
  /**
   * Add two utility_time objects and store the result in the given utility_time
   * object. Overlap is allowed (i.e., any of the objects to be summed and the
   * object to store the result can be the same memory location).
   *
   * @param t1 a pointer to the first operand.
   * @param t2 a pointer to the second operand.
   * @param res a pointer to the utility_time object to store the result.
   */
  static inline void utility_time_add(const utility_time *t1,
				      const utility_time *t2,
				      utility_time *res)
  {
    res->t.tv_sec = t1->t.tv_sec + t2->t.tv_sec + combined_ns_in_s(t1, t2);
    res->t.tv_nsec = combined_ns_leftover(t1, t2);
  }
  /**
   * Work just like utility_time_add() except that both the operands are
   * garbage collected if they are subject to automatic garbage collection.
   */
  static inline void utility_time_add_gc(const utility_time *t1,
					 const utility_time *t2,
					 utility_time *res)
  {
    utility_time_add(t1, t2, res);
    utility_time_gc_auto(t1);
    utility_time_gc_auto(t2);
  }
  /**
   * Work just like utility_time_add() except that it returns the result
   * as dynamic utility_time object subject to automatic garbage collection.
   */
  static inline utility_time *utility_time_add_dyn(const utility_time *t1,
						   const utility_time *t2)
  {
    utility_time *internal_t = utility_time_make_dyn();
    if (internal_t != NULL) {
      utility_time_add(t1, t2, internal_t);
    }
    return internal_t;
  }
  /**
   * Work just like utility_time_add_dyn() except that both the operands are
   * garbage collected if they are subject to automatic garbage collection.
   */
  static inline utility_time *utility_time_add_dyn_gc(const utility_time *t1,
						      const utility_time *t2)
  {
    utility_time *internal_t = utility_time_add_dyn(t1, t2);
    utility_time_gc_auto(t1);
    utility_time_gc_auto(t2);
    return internal_t;
  }

  /* 2. utility_time_inc and variants
   * Overlap is allowed (i.e., target can be the same as inc)
   */
  /**
   * Increment a utility_time object by the amount specified in another
   * utility_time object. Overlap is allowed (i.e., any of the objects can be
   * the same memory location).
   *
   * @param target a pointer to the utility_time object to be incremented.
   * @param inc a pointer to the utility_time object storing the amount of the
   * increment.
   */
  static inline void utility_time_inc(utility_time *target,
				      const utility_time *inc)
  {
    utility_time_add(target, inc, target);
  }
  /**
   * Work just like utility_time_inc() except that the object storing the amount
   * of the increment is garbage collected if it is subject to automatic garbage
   * collection.
   */
  static inline void utility_time_inc_gc(utility_time *target,
					 const utility_time *inc)
  {
    utility_time_inc(target, inc);
    utility_time_gc_auto(inc);
  }
  /** @} End of collection of operational functions */

#ifdef __cplusplus
}
#endif

#undef THOUSAND
#undef MILLION
#undef BILLION

#endif /* UTILITY_TIME */
