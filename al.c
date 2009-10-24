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
static double transactOvhd = 25.0;
static double timeSTM = 0.0;
static double timeRaw = 0.0;

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
  long lockHeld = prof->lockHeld;
  long threadsWaiting = prof->threadsWaiting;
  unsigned long triesCommit = prof->triesCommit;
  double tries = (double)high(triesCommit);
  double commit = (double)low(triesCommit);
  double avgTries;

  if (adaptiveMode == -1) return 0;
  if (adaptiveMode == 1) return 1;
  if (0 < lockHeld) threadsWaiting += lockHeld;
  if (low(triesCommit) == 0) avgTries =  1.0;
  else avgTries = tries / commit;
  if (threadsWaiting <= 0)  threadsWaiting = 0;
  return (avgTries * transactOvhd) < ((double)threadsWaiting);
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
timer_start(struct timeval* tv)
{
  assert(gettimeofday(tv,0) == 0);
}

void
timer_stop(struct timeval* start,struct timeval* acc)
{
  struct timeval stop;

  assert(gettimeofday(&stop,0) == 0);
  stop.tv_sec -= start->tv_sec;
  stop.tv_usec -= start->tv_usec;
  if (stop.tv_usec < 0) {
    stop.tv_sec--;
    stop.tv_usec += 1000000;
  }
  if (!((0 <= stop.tv_sec && 0 < stop.tv_usec) ||
	(0 < stop.tv_sec && 0 <= stop.tv_usec)))
    stop.tv_usec++;
  acc->tv_sec += stop.tv_sec;
  acc->tv_usec += stop.tv_usec;
  if (1000000 <= acc->tv_usec) {
    acc->tv_sec++;
    acc->tv_usec -= 1000000;
  }
}

void
dump_profile(profile_t* prof)
{
  printf("raw=%.3lf[s],stm=%.3lf[s]\n",timeRaw,timeSTM);
}

static void
destroy_thread(void* arg)
{
  thread_t* self = (thread_t*)arg;
  static pthread_mutex_t l = PTHREAD_MUTEX_INITIALIZER;  

  pthread_mutex_lock(&l);
  timeRaw += (double)self->timeRaw.tv_sec;
  timeRaw += ((double)self->timeRaw.tv_usec) / 1000000;
  timeSTM += (double)self->timeSTM.tv_sec;
  timeSTM += ((double)self->timeSTM.tv_usec) / 1000000;
  TxFreeThread(self->stmThread);
  while (!SLIST_EMPTY(&self->prof_list)) {
    nest_t* nest;
    nest = SLIST_FIRST(&self->prof_list);
    SLIST_REMOVE_HEAD(&self->prof_list,next);
    free(nest);
  }
  free(arg);
  pthread_mutex_unlock(&l);
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
