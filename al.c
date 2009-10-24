#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#ifdef HAVE_SCHED_H
#include <sched.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include "al.h"
#ifdef pthread_create
#undef pthread_create
#endif
#include "stm.h"
#include "port.h"
#include "queue.h"

pthread_key_t _al_key;
static int adaptiveMode = 0;
static double transactOvhd = 25.0;

int
setAdaptMode(int mode)
{
  int old = adaptiveMode;
  adaptiveMode = mode;
  return old;
}

double
setTransactOvhd(double ovhd)
{
  double old = transactOvhd;
  transactOvhd = ovhd;
  return old;
}

typedef struct {
  void* (*start)(void*);
  void* arg;
} _dispatch_t;

static void*
_dispatcher(void* _arg)
{
  _dispatch_t* disp = (_dispatch_t*)_arg;
  void* (*start)(void*) = disp->start;
  void* arg = disp->arg;
  thread_t* self;

  free(disp);
  self = malloc(sizeof(*self));
  if (self == 0)
    return (void*)EAGAIN;
  self->stmThread = TxNewThread();
  if (self->stmThread == 0) {
    free(self);
    return (void*)EAGAIN;
  }
  TxInitThread(self->stmThread,(long)pthread_self());
  SLIST_INIT(&self->prof_list);
  pthread_setspecific(_al_key,self);
  return start(arg);
}

int
al_pthread_create(pthread_t* thread,
		  const pthread_attr_t* attr,
		  void* (*start)(void*),
		  void* arg)
{
  _dispatch_t* disp;
  
  disp = (_dispatch_t*)malloc(sizeof(*disp));
  if (disp == 0)
    return EAGAIN;
  disp->start = start;
  disp->arg = arg;
  return pthread_create(thread,attr,_dispatcher,disp);
}

int
transactMode(profile_t* prof)
{
  int threadsInStmMode;
  double avgTries,contention;

  if (adaptiveMode == -1) return 0;
  if (adaptiveMode == 1) return 1;
  if (0 < prof->lockHeld) threadsInStmMode =  prof->lockHeld;
  else threadsInStmMode = 0;
  if (prof->commit == 0) avgTries =  1.0;
  else avgTries = ((double)prof->tries) / prof->commit;
  contention  = threadsInStmMode + prof->threadsWaiting;
  return (avgTries * transactOvhd) < contention;
}

void
busy(void)
{
#if HAVE_SCHED_YIELD	
  sched_yield();
#elif HAVE_PTHREAD_YIELD
  pthread_yield();
#endif
}

/* al() is moved to alx.h */

void
TxStoreSized(Thread* self,intptr_t* dest,intptr_t* src,size_t size)
{
  int n,i;

  assert((size % sizeof(intptr_t)) == 0);
  n = size / sizeof(intptr_t);
  for (i = 0; i < n; i++)
    TxStore(self,dest+i,*(src+i));
  return;
}

void
TxLoadSized(Thread* self,intptr_t* dest,intptr_t* src,size_t size)
{
  int n,i;

  assert((size % sizeof(intptr_t)) == 0);
  n = size / sizeof(intptr_t);
  for (i = 0; i < n; i++)
    *(dest+i) = TxLoad(self,src+i);
  return;
}

void
dump_profile(profile_t* prof)
{
  double r1 = ((double)prof->waitLock)/prof->invokeInLockMode*100.0;
  double r2 = ((double)(prof->tries-prof->commit))/prof->commit*100.0;
  printf("%s:\n"
         "%lu in lock; %lu conflicts(%.2lf%%)\n"
         "%lu in transaction; %lu conflicts(%.2lf%%)\n",
         prof->name,
         (unsigned long)prof->invokeInLockMode,
         (unsigned long)prof->waitLock,
         isnan(r1) ? 0.0 : r1,
         (unsigned long)prof->commit,
         (unsigned long)(prof->tries-prof->commit),
         isnan(r2) ? 0.0 : r2);
}

static void
destroy_thread(void* arg)
{
  thread_t* self = (thread_t*)arg;

  TxFreeThread(self->stmThread);
  while (!SLIST_EMPTY(&self->prof_list)) {
    nest_t* nest;
    nest = SLIST_FIRST(&self->prof_list);
    SLIST_REMOVE_HEAD(&self->prof_list,next);
    free(nest);
  }
  free(arg);
}

__attribute__((constructor)) void
init(void)
{
  pthread_key_create(&_al_key,destroy_thread);
  TxOnce();
}

__attribute__((destructor)) void
finish(void)
{
  TxShutdown();
  pthread_key_create(&_al_key,destroy_thread);
}
