/* A little weird to have in a .cc file, but used to avoid compilation if not
 * USE_LITHE. Eventually should move this logic into the Makesystem somewhere */
#ifdef USE_LITHE

#include "tbb/tbb_lithe.h"

using namespace tbb::lithe;

static lithe::ContextFactory<lithe::Context> context_factory;

Scheduler::Scheduler()
{
  main_context = new Context();
  num_contexts = 1;
  lithe_mutex_init(&mutex, NULL);
  lithe_condvar_init(&condvar);
  mcs_lock_init(&this->qlock);
  TAILQ_INIT(&this->context_queue);
  TAILQ_INIT(&this->child_sched_queue);
}

Scheduler::~Scheduler()
{
  delete main_context;
}

void Scheduler::context_create(Context **__context, 
  size_t stack_size, void (*start_routine)(void*), void *arg)
{
  *__context = context_factory.create(__context_stack_size, start_routine, arg);
  __sync_fetch_and_add(&this->num_contexts, 1);

  schedule_context(*__context);
}

void Scheduler::joinAll()
{
  lithe_context_block(block_main_context, this);
}

void Scheduler::maybe_unblock_main_context() {
  if(__sync_add_and_fetch(&this->num_contexts, -1) == 0)
    lithe_context_unblock(this->main_context);
}

void Scheduler::block_main_context(lithe_context_t *c, void *arg) {
  ((Scheduler*)arg)->maybe_unblock_main_context();
}

void Scheduler::unlock_mcs_lock(void *arg)
{
  struct lock_data {
    mcs_lock_t *lock;
    mcs_lock_qnode_t *qnode;
  } *real_lock = (struct lock_data*)arg;
  mcs_lock_unlock(real_lock->lock, real_lock->qnode);
}

int Scheduler::maybe_request_harts(size_t k)
{
  return lithe_hart_request(k);
}

void Scheduler::schedule_context(Context *c)
{
  mcs_lock_qnode_t qnode = MCS_QNODE_INIT;
  mcs_lock_lock(&this->qlock, &qnode);
    TAILQ_INSERT_TAIL(&this->context_queue, c, link);
    maybe_request_harts(1);
  mcs_lock_unlock(&this->qlock, &qnode);
}

int Scheduler::hart_request(lithe_sched_t *child, size_t k)
{
  /* Find the child Scheduler associated in our queue, and update the number
   * of harts it has requested */
  mcs_lock_qnode_t qnode = MCS_QNODE_INIT;
  mcs_lock_lock(&this->qlock, &qnode);
    child_sched_t *s = TAILQ_FIRST(&this->child_sched_queue);
    while(s != NULL) { 
      if(s->sched == child) {
        s->requested_harts += k;
        break;
      }
      s = TAILQ_NEXT(s, link);
    }
    int ret = maybe_request_harts(k);
  mcs_lock_unlock(&this->qlock, &qnode);
  return ret;
}

void Scheduler::child_enter(lithe_sched_t *child)
{
  /* Add this child to our queue of child schedulers */
  child_sched_t *child_wrapper = (child_sched_t*)malloc(sizeof(child_sched_t));
  child_wrapper->sched = child;
  child_wrapper->requested_harts = 0;
  mcs_lock_qnode_t qnode = MCS_QNODE_INIT;
  mcs_lock_lock(&this->qlock, &qnode);
    TAILQ_INSERT_TAIL(&this->child_sched_queue, child_wrapper, link);
  mcs_lock_unlock(&this->qlock, &qnode);
}

void Scheduler::child_exit(lithe_sched_t *child)
{
  /* Cycle through our child schedulers and find the one corresponding to
   * this child and free it. */
  mcs_lock_qnode_t qnode = MCS_QNODE_INIT;
  mcs_lock_lock(&this->qlock, &qnode);
    child_sched_t *s,*n;
    s = TAILQ_FIRST(&this->child_sched_queue); 
    while(s != NULL) { 
      n = TAILQ_NEXT(s, link);
      if(s->sched == child) {
        TAILQ_REMOVE(&this->child_sched_queue, s, link);
        free(s);
        break;
      }
      s = n;
    }
  mcs_lock_unlock(&this->qlock, &qnode);
}

void Scheduler::hart_return(lithe_sched_t *child)
{
  /* Just call hart_enter() as that is where all of our logic for figuring
   * out what to do with a newly granted hart is. */
  assert(child);
  hart_enter();
}

void Scheduler::hart_enter()
{
  mcs_lock_qnode_t qnode = MCS_QNODE_INIT;
  mcs_lock_lock(&this->qlock, &qnode);
    /* If we have child schedulers that have requested harts, prioritize them
     * access to this hart before ourselves */
    child_sched_t *s = TAILQ_FIRST(&this->child_sched_queue);
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
      s = TAILQ_NEXT(s, link);
    }

    /* If we ever make it here, we have no child schedulers that have
     * requested harts, so just find one of our own contexts to run. */
    Context *c = (Context*)TAILQ_FIRST(&this->context_queue);
    if(c != NULL) {
      TAILQ_REMOVE(&this->context_queue, c, link);
    } 
  mcs_lock_unlock(&this->qlock, &qnode);

  /* If there are no contexts to run, we can safely yield this hart */
  if(c == NULL)
    lithe_hart_yield();
  /* Otherwise, run the context that we found */
  else
    lithe_context_run(c);
  assert(0);
}

void Scheduler::context_unblock(lithe_context_t *c)
{
  schedule_context((Context*)c);
}

void Scheduler::context_yield(lithe_context_t *c)
{
  schedule_context((Context*)c);
}

void Scheduler::context_exit(lithe_context_t *c)
{
  if (c != this->main_context) {
    context_factory.destroy((Context*)c);
    maybe_unblock_main_context();
  }
}

#endif  /* USE_LITHE */
