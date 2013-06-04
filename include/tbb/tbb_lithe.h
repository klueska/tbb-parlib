/* Copyright (c) 2012 The Regents of the University of California
 * Kevin Klues <klueska@cs.berkeley.edu>
 */

#ifndef tbb_tbb_lithe_H
#define tbb_tbb_lithe_H

#include <sys/queue.h>
#include <parlib/tls.h>
#include <parlib/mcs.h>
#include <lithe/mutex.h>
#include <lithe/condvar.h>
#include <lithe/lithe.hh>
#include <assert.h>

//#define LITHE_DEBUG
#ifndef LITHE_DEBUG
# undef assert
# define assert(x) ({(void)(x);})
#endif

namespace tbb {
namespace lithe {

typedef struct child_sched {
  TAILQ_ENTRY(child_sched) link;
  lithe_sched_t *sched;
  int requested_harts;
} child_sched_t;
TAILQ_HEAD(child_sched_queue, child_sched);
typedef struct child_sched_queue child_sched_queue_t;

typedef ::lithe::Context Context;

class Scheduler: public ::lithe::Scheduler {
public:
  Scheduler();

  ~Scheduler();

  void context_create(Context **__context, 
    size_t stack_size, void (*start_routine)(void*), void *arg);
  void joinAll();
  
private:
  static const size_t __context_stack_size = 1<<20;

  size_t num_contexts;
  lithe_mutex_t mutex;
  lithe_condvar_t condvar;
  mcs_lock_t qlock;
  lithe_context_queue_t context_queue;
  child_sched_queue_t child_sched_queue;

  static void block_main_context(lithe_context_t *c, void *arg);
  static void unlock_mcs_lock(void *arg);

  void maybe_unblock_main_context();
  int maybe_request_harts(size_t k);
  void schedule_context(Context *c);

  /* Below overwritten from lithe::Scheduler */
  virtual int hart_request(lithe_sched_t *child, size_t k);
  virtual void child_enter(lithe_sched_t *child);
  virtual void child_exit(lithe_sched_t *child);
  virtual void hart_return(lithe_sched_t *child);
  virtual void hart_enter();
  virtual void context_unblock(lithe_context_t *c);
  virtual void context_yield(lithe_context_t *c);
  virtual void context_exit(lithe_context_t *c);
};

} // namespace lithe
} // namespace tbb

#endif // tbb_tbb_lithe_H
