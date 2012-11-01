/*
    Copyright 2005-2012 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

#include "harness.h"
#include <cstdlib>
#include <cstdio>

#include "tbb/blocked_range.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/parallel_for.h"

using namespace tbb;

#define NUM_ELEMENTS 10
int array[NUM_ELEMENTS];

void forloop(blocked_range<int>& r) {
  for(int i=r.begin(); i!=r.end(); i++)
    array[i] = i; 
  printf("forloop: %p\n", lithe_context_self());
};

int TestMain () {
  printf("Starting a new test!\n");
  for( int p=1; p<=NUM_ELEMENTS; p++ ) {
    printf("Scheduler with %d threads!\n", p);
    for(int i=0; i<NUM_ELEMENTS; i++)
      array[i] = 0;
    {
      tbb::task_scheduler_init init(p);
      parallel_for(blocked_range<int>(0,NUM_ELEMENTS), &forloop);
    }
    printf("Array contents:\n");
    for(int i=0; i<NUM_ELEMENTS; i++)
      printf("  array[%d] = %d\n", i, array[i]);
  }
  return Harness::Done;
}

