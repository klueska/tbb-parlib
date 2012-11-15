/* Copyright (c) 2012 The Regents of the University of California
 * Kevin Klues <klueska@cs.berkeley.edu>
 */

#ifndef tbb_tbb_lithe_H
#define tbb_tbb_lithe_H

#include <parlib/tls.h>
#include <parlib/mcs.h>
#include <parlib/queue.h>
#include <lithe/mutex.h>
#include <lithe/condvar.h>
#include <lithe/lithe.hh>

namespace tbb {
namespace lithe {

typedef void (*start_routine_t)(void*);

struct context: public lithe_context {
  STAILQ_ENTRY(context) link;
  void (*start_routine)(void *);
  void *arg;
  int id;
};
STAILQ_HEAD(context_list, context);
typedef struct context context_t;
typedef struct context_list context_list_t;

struct child_sched {
  STAILQ_ENTRY(child_sched) link;
  lithe_sched_t *sched;
  int requested_harts;
};
STAILQ_HEAD(child_sched_list, child_sched);
typedef struct child_sched child_sched_t;
typedef struct child_sched_list child_sched_list_t;

static context_t *allocate_context(size_t stack_size)
{
  context_t *context = (context_t*)malloc(sizeof(context_t));
  assert(context);

  context->stack.size = stack_size;
  context->stack.bottom = malloc(context->stack.size);
  assert(context->stack.bottom);
  return context;
}

static void free_context(context_t *context)
{
  assert(context);
  assert(context->stack.bottom);
  free(context->stack.bottom);
  free(context);
}

class scheduler: public ::lithe::Scheduler {
public:
  scheduler()
  {
    num_contexts = 0;
    lithe_mutex_init(&mutex, NULL);
    lithe_condvar_init(&condvar);
    mcs_lock_init(&this->qlock);
    STAILQ_INIT(&this->context_list);
    STAILQ_INIT(&this->child_sched_list);
  }

  void context_create(context_t **__context, 
    size_t stack_size, void (*start_routine)(void*), void *arg)
  {
    context_t *context = allocate_context(stack_size);
    lithe_context_init(context, 
      (start_routine_t)&start_routine_wrapper, context);
    context->start_routine = start_routine;
    context->arg = arg;
    lithe_mutex_lock(&mutex);
    context->id = num_contexts++;
    lithe_mutex_unlock(&mutex);
    schedule_context(context);
    *__context = context;
  }

  void joinAll()
  {
    lithe_mutex_lock(&mutex);
    while(num_contexts > 0) 
      lithe_condvar_wait(&condvar, &mutex);
    lithe_mutex_unlock(&mutex);
  }
  
private:
  int num_contexts;
  lithe_mutex_t mutex;
  lithe_condvar_t condvar;
  mcs_lock_t qlock;
  context_list_t context_list;
  child_sched_list_t child_sched_list;

  static void start_routine_wrapper(void *__arg)
  {
    context_t *self = (context_t*)__arg;
    scheduler *sched = (scheduler*)lithe_sched_current();
  
    lithe_mutex_lock(&sched->mutex);
    lithe_mutex_unlock(&sched->mutex);
    self->start_routine(self->arg);
    destroy_dtls();
  
    lithe_mutex_lock(&sched->mutex);
    sched->num_contexts--;
    if(sched->num_contexts == 0)
      lithe_condvar_signal(&sched->condvar);
    lithe_mutex_unlock(&sched->mutex);
  }
  
  static void unlock_mcs_lock(void *arg) {
    struct lock_data {
      mcs_lock_t *lock;
      mcs_lock_qnode_t *qnode;
    } *real_lock = (struct lock_data*)arg;
    mcs_lock_unlock(real_lock->lock, real_lock->qnode);
  }

  void schedule_context(context_t *context)
  {
    mcs_lock_qnode_t qnode = MCS_QNODE_INIT;
    mcs_lock_lock(&this->qlock, &qnode);
      STAILQ_INSERT_TAIL(&this->context_list, context, link);
    mcs_lock_unlock(&this->qlock, &qnode);
    lithe_hart_request(1);
  }

  /* Below overwritten from lithe::Scheduler */
  virtual int hart_request(lithe_sched_t *child, int k)
  {
	/* Find the child scheduler associated in our list, and update the number
     * of harts it has requested */
    mcs_lock_qnode_t qnode = MCS_QNODE_INIT;
    mcs_lock_lock(&this->qlock, &qnode);
      child_sched_t *s = STAILQ_FIRST(&this->child_sched_list);
      while(s != NULL) { 
        if(s->sched == child) {
          s->requested_harts += k;
          break;
        }
        s = STAILQ_NEXT(s, link);
      }
    mcs_lock_unlock(&this->qlock, &qnode);
    return lithe_hart_request(k);
  }

  virtual void child_enter(lithe_sched_t *child)
  {
    /* Add this child to our list of child schedulers */
    child_sched_t *child_wrapper = (child_sched_t*)malloc(sizeof(child_sched_t));
    child_wrapper->sched = child;
    child_wrapper->requested_harts = 0;
    mcs_lock_qnode_t qnode = MCS_QNODE_INIT;
    mcs_lock_lock(&this->qlock, &qnode);
      STAILQ_INSERT_TAIL(&this->child_sched_list, child_wrapper, link);
    mcs_lock_unlock(&this->qlock, &qnode);
  }

  virtual void child_exit(lithe_sched_t *child)
  {
	/* Cycle through our child schedulers and find the one corresponding to
     * this child and free it. */
    mcs_lock_qnode_t qnode = MCS_QNODE_INIT;
    mcs_lock_lock(&this->qlock, &qnode);
      child_sched_t *s,*n;
      s = STAILQ_FIRST(&this->child_sched_list); 
      while(s != NULL) { 
        n = STAILQ_NEXT(s, link);
        if(s->sched == child) {
          STAILQ_REMOVE(&this->child_sched_list, s, child_sched, link);
          free(s);
          break;
        }
        s = n;
      }
    mcs_lock_unlock(&this->qlock, &qnode);
  }

  virtual void hart_return(lithe_sched_t *child)
  {
    /* Just call hart_enter() as that is where all of our logic for figuring
     * out what to do with a newly granted hart is. */
    assert(child);
    hart_enter();
  }

  virtual void hart_enter()
  {
    mcs_lock_qnode_t qnode = MCS_QNODE_INIT;
    mcs_lock_lock(&this->qlock, &qnode);
	  /* If we have child schedulers that have requested harts, prioritize them
       * access to this hart before ourselves */
      child_sched_t *s = STAILQ_FIRST(&this->child_sched_list);
      while(s != NULL) { 
        if(s->requested_harts > 0) {
          struct {
            mcs_lock_t *lock;
            mcs_lock_qnode_t *qnode;
          } real_lock = {&this->qlock, &qnode};
          s->requested_harts--;
          lithe_hart_grant(s->sched, unlock_mcs_lock, (void*)&real_lock);
          assert(0);
        }
        s = STAILQ_NEXT(s, link);
      }

	  /* If we ever make it here, we have no child schedulers that have
       * requested harts, so just find one of our own contexts to run. */
      context_t *context = NULL;
      context = STAILQ_FIRST(&this->context_list);
      if(context != NULL) {
        STAILQ_REMOVE_HEAD(&this->context_list, link);
      } 
    mcs_lock_unlock(&this->qlock, &qnode);
  
    /* If there are no contexts to run, we can safely yield this hart */
    if(context == NULL)
      lithe_hart_yield();
    /* Otherwise, run the context that we found */
    else
      lithe_context_run(context);
    assert(0);
  }
  
  virtual void context_unblock(lithe_context_t *context)
  {
    schedule_context((context_t*)context);
  }
  
  virtual void context_yield(lithe_context_t *context)
  {
    schedule_context((context_t*)context);
  }
  
  virtual void context_exit(lithe_context_t *context)
  {
    lithe_context_cleanup(context);
    free_context((context_t*)context);
  }
};

} // namespace lithe
} // namespace tbb

#endif // tbb_tbb_lithe_H
