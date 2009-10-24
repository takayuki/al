/*
 * Al -- an implementation of the adaptive locks
 *
 * Copyright (C) 2008, University of Oregon
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 * 
 *   * Neither the name of University of Oregon nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF OREGON ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
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

#ifdef TLS
static TLS thread_t* _al_self;
#endif
static pthread_key_t _al_key;
static int adaptMode = 0;
static int tranxOvhd = 50;
static int tranxOvhdScale = 10;
static double timeSTM = 0.0;
static double timeRaw = 0.0;
static pthread_mutex_t timeMutex = PTHREAD_MUTEX_INITIALIZER;  

thread_t*
thread_self(void)
{
#ifdef TLS
  return _al_self;
#else
  return (thread_t*)pthread_getspecific(_al_key);
#endif
}

void
setAdaptMode(int mode)
{
  adaptMode = mode;
}

void
setTranxOvhd(double ovhd)
{
  tranxOvhd = ovhd * tranxOvhdScale;
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
  self->lock = 0;
  self->nestLevel = 0;
  pthread_setspecific(_al_key,self);
#ifdef TLS
  _al_self = self;
#endif
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
transactMode_1(al_t* lock,int spins)
{
  unsigned long state;
  unsigned long statistic;
  unsigned long thrdsInTranx;
  unsigned long thrdsContend;
  unsigned long triesCommits;
  unsigned long Tries;
  unsigned long Commits;

  if (adaptMode == -1) return 0;
  if (adaptMode == 1) return 1;
  state = lock->state;
  statistic = lock->statistic;
  thrdsContend = contention(statistic);
  if (spins == 0) thrdsContend++;
  thrdsInTranx = thrdsInStmMode(state);
  if (0 < thrdsInTranx) thrdsContend += thrdsInTranx;
  else if (lockHeld(state)) thrdsContend++;

  triesCommits = lock->triesCommits;
  Tries = tries(triesCommits);
  Commits = commits(triesCommits);

  if (0 == Commits) {
    Commits = 1; Tries = 1;
  }
  return (Tries * tranxOvhd) < (Commits * tranxOvhdScale * thrdsContend);
}

int
transactMode_0(al_t* lock,int spins)
{
  unsigned long state;
  unsigned long statistic;
  unsigned long thrdsInTranx;
  unsigned long thrdsContend;
  unsigned long triesCommits;
  unsigned long Tries;
  unsigned long Commits;

  if (adaptMode == -1) return 0;
  if (adaptMode == 1) return 1;
  state = lock->state;
  statistic = lock->statistic;
  thrdsContend = contention(statistic);
  if (spins == 0) thrdsContend++;
  thrdsInTranx = thrdsInStmMode(state);
  if (0 < thrdsInTranx) thrdsContend += thrdsInTranx;
  else if (lockHeld(state)) thrdsContend++;

  Tries = tries(statistic);
  Commits = commits(statistic);

  if (0 == Commits) {
    Commits = 1; Tries = 1;
  }
  return (Tries * tranxOvhd) < (Commits * tranxOvhdScale * thrdsContend);
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

#define ACQUIRE(transactMode)						\
  ({intptr_t prev,next;							\
    prev = lock->state;							\
    if (transition(prev) == 0) {					\
      if ((useTransact = transactMode(lock,spins))) {			\
	if (lockHeld(prev) == 0) {					\
	  next = setLockMode(prev,0);					\
	  next = setThrdsInStmMode(next,thrdsInStmMode(next)+1);	\
	  assert(lockMode(next) == 0);					\
	  assert(lockHeld(next) == 0);					\
	  assert(thrdsInStmMode(next));					\
	  assert(transition(next) == 0);				\
	  if (CAS(lock->state,prev,next) == prev) break;		\
	} else {							\
	  next = setLockMode(prev,0);					\
	  next = setTransition(next,1);					\
	  assert(lockMode(next) == 0);					\
	  assert(lockHeld(next));					\
	  assert(thrdsInStmMode(next) == 0);				\
	  assert(transition(next));					\
	  CAS(lock->state,prev,next);					\
	}								\
      } else {								\
	if (lockHeld(prev) == 0 && thrdsInStmMode(prev) == 0) {		\
	  next = setLockMode(prev,1);					\
	  next = setLockHeld(next,1);					\
	  assert(lockMode(next));					\
	  assert(lockHeld(next));					\
	  assert(thrdsInStmMode(next) == 0);				\
	  assert(transition(next) == 0);				\
	  if (CAS(lock->state,prev,next) == prev) break;		\
	} else if (lockMode(prev) == 0) {				\
	  next = setLockMode(prev,1);					\
	  next = setTransition(next,1);					\
	  assert(lockMode(next));					\
	  assert(lockHeld(next) == 0);					\
	  assert(thrdsInStmMode(next));					\
	  assert(transition(next));					\
	  CAS(lock->state,prev,next);					\
	}								\
      }									\
    } else {								\
      if (lockMode(prev) == 0) {					\
	if (lockHeld(prev) == 0) {					\
	  useTransact = 1;						\
	  next = setThrdsInStmMode(prev,thrdsInStmMode(prev)+1);	\
	  next = setTransition(next,0);					\
	  assert(lockMode(next) == 0);					\
	  assert(lockHeld(next) == 0);					\
	  assert(thrdsInStmMode(next));					\
	  assert(transition(next) == 0);				\
	  if (CAS(lock->state,prev,next) == prev) break;		\
	}								\
      } else {								\
	if (lockHeld(prev) == 0 && thrdsInStmMode(prev) == 0) {		\
	  useTransact = 0;						\
	  next = setLockHeld(prev,1);					\
	  next = setTransition(next,0);					\
	  assert(lockMode(next));					\
	  assert(lockHeld(next));					\
	  assert(thrdsInStmMode(next) == 0);				\
	  assert(transition(next) == 0);				\
	  if (CAS(lock->state,prev,next) == prev) break;		\
	}								\
      }									\
    }})

#define RELEASE()							\
  ({intptr_t prev,next;							\
    prev = lock->state;							\
    if (lockHeld(prev) == 0) {						\
      assert(lockHeld(prev) == 0);					\
      assert(thrdsInStmMode(prev));					\
      next = setThrdsInStmMode(prev,thrdsInStmMode(prev)-1);		\
      if (CAS(lock->state,prev,next) == prev) break;			\
    } else {								\
      assert(lockHeld(prev));						\
      assert(thrdsInStmMode(prev) == 0);				\
      next = setLockHeld(prev,0);					\
      if (CAS(lock->state,prev,next) == prev) break;			\
    }})

int
enterCritical_0(al_t* lock)
{
  int spins;
  int spin_thrld = 100;
  int useTransact;

  spins = 0;
  INC(lock->statistic);
  while (1) {
    ACQUIRE(transactMode_0);
    if (spin_thrld < ++spins) Yield();
  }
  DEC(lock->statistic);
  return useTransact;
}

int
enterCritical_1(al_t* lock)
{
  int spins;
  int spin_thrld = 100;
  int useTransact;

  spins = 0;
  while (1) {
    ACQUIRE(transactMode_1);
    if (spins == 0) INC(lock->statistic);
    if (spin_thrld < ++spins) Yield();
  }
  if (0 < spins) DEC(lock->statistic);
  return useTransact;
}

void
exitCritical_0(al_t* lock)
{
  while(1)
    RELEASE();
}

void
exitCritical_1(al_t* lock)
{
  while(1)
    RELEASE();
}

intptr_t LocalStore(intptr_t* dest,intptr_t src) { return (*dest = src); }
intptr_t LocalLoad(intptr_t* src) { return *src; }
float LocalStoreF(float* dest,float src) { return (*dest = src); }
float LocalLoadF(float* src) { return *src; }

void
TxStoreSized(Thread* self,intptr_t* dest,intptr_t* src,size_t size)
{
  int n,i;

  if ((size % sizeof(intptr_t)) != 0) {
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
	    __FILE__,__LINE__,__func__);
    abort();
  }
  n = size / sizeof(intptr_t);
  for (i = 0; i < n; i++)
    TxStore(self,dest+i,*(src+i));
  return;
}

void
TxLoadSized(Thread* self,intptr_t* dest,intptr_t* src,size_t size)
{
  int n,i;

  if ((size % sizeof(intptr_t)) != 0) {
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
	    __FILE__,__LINE__,__func__);
    abort();
  }
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
timer_stop(hrtime_t* start,hrtime_t* acc,al_t* lock,int stmMode)
{
  hrtime_t stop;

  stop = gethrtime();
  stop -= *start;
  *acc += stop;
}
#else
void
timer_start(struct timeval* start)
{
  int status;

  status = gettimeofday(start,0);
  if (status != 0) {
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
	    __FILE__,__LINE__,__func__);
    abort();
  }
}

void
timer_stop(struct timeval* start,struct timeval* acc,al_t* lock,int stmMode)
{
  int status;
  struct timeval stop;

  status = gettimeofday(&stop,0);
  if (status != 0) {
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
	    __FILE__,__LINE__,__func__);
    abort();
  }
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
  printf("tranxOvhd=%d,tranxOvhdScale=%d\n",tranxOvhd,tranxOvhdScale);
  printf("raw=%.3lf,stm=%.3lf\n",timeRaw,timeSTM);
  TxShutdown();
}
