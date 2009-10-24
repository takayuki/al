#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "al.h"
#ifdef pthread_create
#undef pthread_create
#endif
#include "stm.h"
#include "port.h"

pthread_key_t _thread_self_key;
double transactOvhd = 25.0;

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
  _thread_t *self;

  free(disp);
  self = malloc(sizeof(*self));
  if (self == 0)
    return 0;
  self->stmThread = STM_NEW_THREAD();
  if (self->stmThread == 0) {
    free(self);
    return 0;
  }
  STM_INIT_THREAD(self->stmThread);
  self->nestLevel = 0;
  pthread_setspecific(_thread_self_key,self);
  return start(arg);
}

int
_al_pthread_create(pthread_t* thread,
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

static int
transactMode(_profile_t* prof)
{
  double avgTries = ((double)prof->tries) / (prof->commit+1);
  double contention = prof->threadsInStmMode + prof->threadsWaiting;
  return (avgTries * transactOvhd) < contention;
}

int
isInStmMode(void)
{
  _thread_t *self;
  assert((self = pthread_getspecific(_thread_self_key)));
  return 1 <= self->nestLevel;
}

int
isInLockMode(void)
{
  _thread_t *self;
  assert((self = pthread_getspecific(_thread_self_key)));
  return self->nestLevel == -1;
}

void*
al(_profile_t* prof,
   void* (*rawfunc)(void*),
   void* (*stmfunc)(Thread*,void*),
   void* arg)
{
  _thread_t *self;
  intptr_t tmp;
  sigjmp_buf buf;
  int ro = 0;
  void* ret;

  assert((self = pthread_getspecific(_thread_self_key)));
  assert(self->nestLevel == 0);
  if (transactMode(prof)) {
    INC(prof->threadsWaiting);
    while ((tmp = LOAD(prof->lockHeld)) == ~0 ||
	   CAS(prof->lockHeld,tmp,tmp+1) != tmp);
    DEC(prof->threadsWaiting);
    INC(prof->threadsInStmMode);
    self->nestLevel++;
    INC(prof->tries);
    if (sigsetjmp(buf,1)) INC(prof->tries);
    STM_BEGIN(self->stmThread,buf,ro);
    ret = stmfunc(self->stmThread,arg);
    STM_COMMIT(self->stmThread);
    INC(prof->commit);
    self->nestLevel--;
    DEC(prof->threadsInStmMode);
    DEC(prof->lockHeld);
  } else {
    INC(prof->invokeInLockMode);
    INC(prof->threadsWaiting);
    if (LOAD(prof->lockHeld) != 0 ||
	CAS(prof->lockHeld,0,~0) != 0) {
      INC(prof->waitLock);
      while (LOAD(prof->lockHeld) != 0 ||
	     CAS(prof->lockHeld,0,~0) != 0);
    }
    DEC(prof->threadsWaiting);
    self->nestLevel = -1;
    ret = rawfunc(arg);
    self->nestLevel = 0;
    INC(prof->lockHeld);
  }
  return ret;
}

void*
mallocInStm(size_t size)
{
  _thread_t *self;
  assert((self = pthread_getspecific(_thread_self_key)));
  return TxAlloc(self->stmThread,size);
}

void
freeInStm(void* ptr)
{
  _thread_t *self;
  assert((self = pthread_getspecific(_thread_self_key)));
  TxFree (self->stmThread,ptr);
}

void
dump_profile(_profile_t* prof)
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
  _thread_t* self = (_thread_t*)arg;
  TxFreeThread(self->stmThread);
  free(arg);
}

__attribute__((constructor)) void
init(void)
{
  pthread_key_create(&_thread_self_key,destroy_thread);
}
