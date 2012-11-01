/* Copyright (c) 2012 The Regents of the University of California
 * Kevin Klues <klueska@cs.berkeley.edu>
 */

#ifndef tbb_tbb_lithe_H
#define tbb_tbb_lithe_H

#include <parlib/mcs.h>
#include <lithe/mutex.h>
#include <lithe/condvar.h>
#include <lithe/lithe.hh>

namespace tbb {
namespace lithe {

typedef void (*start_routine_t)(void*);
typedef struct context: public lithe_context {
  void (*start_routine)(void *);
  void *arg;
} context_t;

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
    thread_count = 0;
    lithe_mutex_init(&mutex, NULL);
    lithe_condvar_init(&condvar);
    mcs_lock_init(&this->qlock);
    lithe_context_deque_init(&this->contextq);
  }

  void context_create(context_t **__context, 
    size_t stack_size, void (*start_routine)(void*), void *arg)
  {
    context_t *context = allocate_context(stack_size);
    lithe_context_init(context, 
      (start_routine_t)&start_routine_wrapper, context);
    context->start_routine = start_routine;
    context->arg = arg;
    thread_count++;
    schedule_context(context);
    *__context = context;
  }

  void joinAll()
  {
    lithe_mutex_lock(&mutex);
    while(thread_count > 0) 
      lithe_condvar_wait(&condvar, &mutex);
    lithe_mutex_unlock(&mutex);
  }
  
private:
  int thread_count;
  lithe_mutex_t mutex;
  lithe_condvar_t condvar;
  mcs_lock_t qlock;
  struct lithe_context_deque contextq;

  static void start_routine_wrapper(void *__arg)
  {
    context_t *self = (context_t*)__arg;
    scheduler *sched = (scheduler*)lithe_sched_current();
  
    self->start_routine(self->arg);
  
    lithe_mutex_lock(&sched->mutex);
    sched->thread_count--;
    lithe_mutex_unlock(&sched->mutex);
    if(sched->thread_count == 0)
      lithe_condvar_signal(&sched->condvar);
  }
  
  void schedule_context(context_t *context)
  {
    mcs_lock_qnode_t qnode ={0,0};
    mcs_lock_lock(&this->qlock, &qnode);
      lithe_context_deque_enqueue(&this->contextq, context);
      lithe_hart_request(max_harts()-num_harts());
    mcs_lock_unlock(&this->qlock, &qnode);
  }

  /* Below overwritten from lithe::Scheduler */
  virtual void hart_enter()
  {
    lithe_context_t *context = NULL;
  
    mcs_lock_qnode_t qnode = {0,0};
    mcs_lock_lock(&this->qlock, &qnode);
      lithe_context_deque_dequeue(&this->contextq, &context);
    mcs_lock_unlock(&this->qlock, &qnode);
  
    if(context == NULL)
      lithe_hart_yield();
    else
      lithe_context_run(context);
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
