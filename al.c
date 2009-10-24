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
#include <string.h>
#include <sys/time.h>
#include "al.h"
#ifdef pthread_create
#undef pthread_create
#endif
#include "stm.h"
#include "port.h"
#include "queue.h"

pthread_key_t _al_key;
static int adaptiveMode = 0;
static double transactOvhd = 5.0;
static double timeSTM = 0.0;
static double timeRaw = 0.0;
static pthread_mutex_t timeMutex = PTHREAD_MUTEX_INITIALIZER;  

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
  memset(self,0,sizeof(*self));
  self->stmThread = TxNewThread();
  if (self->stmThread == 0) {
    free(self);
    return (void*)EAGAIN;
  }
  TxInitThread(self->stmThread,(long)pthread_self());
  SLIST_INIT(&self->lock_list);
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
transactMode(al_t* lock)
{
  unsigned long state = lock->state;
  unsigned long statistic = lock->statistic;
  unsigned long triesCommits = lock->triesCommits;
  double thrdsInTransact = (double)thrdsInStmMode(state);
  double thrdsContending = (double)contention(statistic);
  double Tries = (double)tries(triesCommits);
  double Commits = (double)commits(triesCommits);
  double avgTries;

  if (adaptiveMode == -1) return 0;
  if (adaptiveMode == 1) return 1;
  if (0 < thrdsInTransact) thrdsContending += thrdsInTransact;
  if (lockHeld(state)) thrdsContending += 1.0;
  if (!(0 < Commits)) avgTries =  1.0;
  else avgTries = Tries / Commits;
  return (avgTries * transactOvhd) < thrdsContending;
}

void
Yield(void)
{
#if HAVE_SCHED_YIELD	
  sched_yield();
#elif HAVE_PTHREAD_YIELD
  pthread_yield();
#endif
}

int
enterCritical(al_t* lock)
{
  intptr_t prev,next;
  int spins;
  int spin_thrld = 100;
  int useTransact;

  spins = 0;
  while (1) {
    prev = lock->state;
    if (transition(prev) == 0) {
      if ((useTransact = transactMode(lock))) {
	if (lockHeld(prev) == 0) {
	  next = setLockMode(prev,0);
	  next = setThrdsInStmMode(next,thrdsInStmMode(next)+1);
	  assert(lockMode(next) == 0);
	  assert(lockHeld(next) == 0);
	  assert(thrdsInStmMode(next));
	  assert(transition(next) == 0);
	  if (CAS(lock->state,prev,next) == prev) break;
	} else {
	  next = setLockMode(prev,0);
	  next = setTransition(next,1);
	  assert(lockMode(next) == 0);
	  assert(lockHeld(next));
	  assert(thrdsInStmMode(next) == 0);
	  assert(transition(next));
	  CAS(lock->state,prev,next);
	}
      } else {
	if (lockHeld(prev) == 0 && thrdsInStmMode(prev) == 0) {
	  next = setLockMode(prev,1);
	  next = setLockHeld(next,1);
	  assert(lockMode(next));
	  assert(lockHeld(next));
	  assert(thrdsInStmMode(next) == 0);
	  assert(transition(next) == 0);
	  if (CAS(lock->state,prev,next) == prev) break;
	} else if (lockMode(prev) == 0) {
	  next = setLockMode(prev,1);
	  next = setTransition(next,1);
	  assert(lockMode(next));
	  assert(lockHeld(next) == 0);
	  assert(thrdsInStmMode(next));
	  assert(transition(next));
	  CAS(lock->state,prev,next);
	}
      }
    } else {
      if (lockMode(prev) == 0) {
	if (lockHeld(prev) == 0) {
	  useTransact = 1;
	  next = setThrdsInStmMode(prev,thrdsInStmMode(prev)+1);
	  next = setTransition(next,0);
	  assert(lockMode(next) == 0);
	  assert(lockHeld(next) == 0);
	  assert(thrdsInStmMode(next));
	  assert(transition(next) == 0);
	  if (CAS(lock->state,prev,next) == prev) break;
	}
      } else {
	if (lockHeld(prev) == 0 && thrdsInStmMode(prev) == 0) {
	  useTransact = 0;
	  next = setLockHeld(prev,1);
	  next = setTransition(next,0);
	  assert(lockMode(next));
	  assert(lockHeld(next));
	  assert(thrdsInStmMode(next) == 0);
	  assert(transition(next) == 0);
	  if (CAS(lock->state,prev,next) == prev) break;
	}
      }
    }
    if (spins == 0) INC(lock->statistic);
    if (spin_thrld < ++spins) Yield();
  }
  if (0 < spins) DEC(lock->statistic);
  return useTransact;
}

void
exitCritical(al_t* lock)
{
  intptr_t prev,next;

  while (1) {
    prev = lock->state;
    if (lockHeld(prev) == 0) {
      assert(lockHeld(prev) == 0);
      assert(thrdsInStmMode(prev));
      next = setThrdsInStmMode(prev,thrdsInStmMode(prev)-1);
      if (CAS(lock->state,prev,next) == prev) break;
    } else {
      assert(lockHeld(prev));
      assert(thrdsInStmMode(prev) == 0);
      next = setLockHeld(prev,0);
      if (CAS(lock->state,prev,next) == prev) break;
    }
  }
}

void
TxStoreSized(Thread* self,intptr_t* dest,intptr_t* src,size_t size)
{
  int n,i;

  if ((size % sizeof(intptr_t)) != 0)
    abort();
  n = size / sizeof(intptr_t);
  for (i = 0; i < n; i++)
    TxStore(self,dest+i,*(src+i));
  return;
}

void
TxLoadSized(Thread* self,intptr_t* dest,intptr_t* src,size_t size)
{
  int n,i;

  if ((size % sizeof(intptr_t)) != 0)
    abort();
  n = size / sizeof(intptr_t);
  for (i = 0; i < n; i++)
    *(dest+i) = TxLoad(self,src+i);
  return;
}

#ifdef HAVE_GETHRTIME
void
timer_start(hrtime_t* start)
{
  *start = gethrtime();
}

void
timer_stop(hrtime_t* start,hrtime_t* acc)
{
  hrtime_t stop;
  stop = gethrtime();
  *acc += stop - *start;
}
#else
void
timer_start(struct timeval* start)
{
  int status;

  status = gettimeofday(start,0);
  if (status != 0)
    abort();
}

void
timer_stop(struct timeval* start,struct timeval* acc)
{
  int status;
  struct timeval stop;

  status = gettimeofday(&stop,0);
  if (status != 0)
    abort();
  stop.tv_sec -= start->tv_sec;
  stop.tv_usec -= start->tv_usec;
  if (stop.tv_usec < 0) {
    stop.tv_sec--;
    stop.tv_usec += 1000000;
  }
  if (!((0 <= stop.tv_sec && 0 < stop.tv_usec) ||
	(0 < stop.tv_sec && 0 <= stop.tv_usec)))
    if (start->tv_usec%2) stop.tv_usec++;
  acc->tv_sec += stop.tv_sec;
  acc->tv_usec += stop.tv_usec;
  if (1000000 <= acc->tv_usec) {
    acc->tv_sec++;
    acc->tv_usec -= 1000000;
  }
}
#endif

void
dump_profile(al_t* lock)
{
  printf("%s: raw=%.3lf,stm=%.3lf\n",lock->name,timeRaw,timeSTM);
}

static void
destroy_thread(void* arg)
{
  thread_t* self = (thread_t*)arg;

  pthread_mutex_lock(&timeMutex);
#ifdef HAVE_GETHRTIME
  timeRaw += ((double)self->timeRaw) / 1000000000.0;
  timeSTM += ((double)self->timeSTM) / 1000000000.0;
#else
  timeRaw += (double)self->timeRaw.tv_sec;
  timeRaw += ((double)self->timeRaw.tv_usec) / 1000000.0;
  timeSTM += (double)self->timeSTM.tv_sec;
  timeSTM += ((double)self->timeSTM.tv_usec) / 1000000.0;
#endif
  TxFreeThread(self->stmThread);
  while (!SLIST_EMPTY(&self->lock_list)) {
    nest_t* nest;
    nest = SLIST_FIRST(&self->lock_list);
    SLIST_REMOVE_HEAD(&self->lock_list,next);
    free(nest);
  }
  free(arg);
  pthread_mutex_unlock(&timeMutex);
}

__attribute__((constructor)) void
init(void)
{
  TxOnce();
  pthread_key_create(&_al_key,destroy_thread);
}

__attribute__((destructor)) void
finish(void)
{
  TxShutdown();
}
