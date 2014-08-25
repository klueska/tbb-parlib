/* Copyright (c) 2012 The Regents of the University of California
 * Kevin Klues <klueska@cs.berkeley.edu>
 */

#ifndef tbb_tbb_lithe_H
#define tbb_tbb_lithe_H

#include <lithe/mutex.h>
#include <lithe/lithe.h>
#include <lithe/fork_join_sched.h>
#include <assert.h>

//#define LITHE_DEBUG
#ifndef LITHE_DEBUG
# undef assert
# define assert(x) ({(void)(x);})
#endif

#endif // tbb_tbb_lithe_H
