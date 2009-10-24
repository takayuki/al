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

pthread_key_t _al_key;
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
    return (void*)EAGAIN;
  self->stmThread = TxNewThread();
  if (self->stmThread == 0) {
    free(self);
    return (void*)EAGAIN;
  }
  TxInitThread(self->stmThread,(long)pthread_self());
  self->nestLevel = 0;
  pthread_setspecific(_al_key,self);
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
  intptr_t threadsInStmMode = 0 < prof->lockHeld ? prof->lockHeld : 0;
  double avgTries = ((double)prof->tries) / (prof->commit+1);
  double contention = threadsInStmMode + prof->threadsWaiting;
  return (avgTries * transactOvhd) < contention;
}

int
isInStmMode(void)
{
  _thread_t *self;
  assert((self = pthread_getspecific(_al_key)));
  return 1 <= self->nestLevel;
}

int
isInLockMode(void)
{
  _thread_t *self;
  assert((self = pthread_getspecific(_al_key)));
  return self->nestLevel == -1;
}

static void
busy(void)
{
#if HAVE_SCHED_YIELD	
  sched_yield();
#elif HAVE_PTHREAD_YIELD
  pthread_yield();
#endif
}

static const int default_spins = 100;

void*
al(_profile_t* prof,
   void* (*rawfunc)(void*),
   void* (*stmfunc)(Thread*,void*),
   void* arg,
   int ro)
{
  _thread_t *self;
  intptr_t tmp;
  sigjmp_buf buf;
  int cnt = default_spins;
  void* ret;

  assert((self = pthread_getspecific(_al_key)));
  assert(self->nestLevel == 0);
  if (transactMode(prof)) {
    inc(prof->threadsWaiting);
    while ((tmp = load(prof->lockHeld)) == ~0 ||
	   CAS(prof->lockHeld,tmp,tmp+1) != tmp) {
      if (--cnt <= 0) {
	busy(); cnt = default_spins;
      }
    }
    dec(prof->threadsWaiting);
    inc(self->nestLevel);
    sigsetjmp(buf,1);
    inc(prof->tries);
    TxStart(self->stmThread,&buf,&ro);
    ret = stmfunc(self->stmThread,arg);
    TxCommit(self->stmThread);
    inc(prof->commit);
    dec(self->nestLevel);
    DEC(prof->lockHeld);
  } else {
    inc(prof->invokeInLockMode);
    inc(prof->threadsWaiting);
    if (load(prof->lockHeld) != 0 ||
	CAS(prof->lockHeld,0,~0) != 0) {
      inc(prof->waitLock);
      while (load(prof->lockHeld) != 0 ||
	     CAS(prof->lockHeld,0,~0) != 0)
	if (--cnt <= 0) {
	  busy(); cnt = default_spins;
	}
    }
    dec(prof->threadsWaiting);
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
  assert((self = pthread_getspecific(_al_key)));
  return TxAlloc(self->stmThread,size);
}

void
freeInStm(void* ptr)
{
  _thread_t *self;
  assert((self = pthread_getspecific(_al_key)));
  TxFree(self->stmThread,ptr);
}

void*
mallocInLock(size_t size)
{
  return tmalloc_reserve(size);
}

void
freeInLock(void* ptr)
{
  return tmalloc_free(ptr);
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
  pthread_key_create(&_al_key,destroy_thread);
  TxOnce();
}

__attribute__((destructor)) void
finish(void)
{
  TxShutdown();
  pthread_key_create(&_al_key,destroy_thread);
}
