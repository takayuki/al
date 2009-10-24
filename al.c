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
#ifdef HAVE_LIBCPC_H
#include <libcpc.h>
#endif
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
static long defaultTranxOvhd = 150;
static long tranxOvhdScale = 100;
static long tranxInstrLd = 85;
static long tranxInstrSt = 85 * 2;
static uint64_t instrCntOvhd = 0;
static long txLd;
static long txSt;
#if defined(HAVE_CPC)
static cpc_t* cpc;
static cpc_set_t* set;
static int ind0;
#elif defined(HAVE_OBSOLETE_CPC)
static cpc_event_t cpc;
#endif
static pthread_mutex_t globMutex = PTHREAD_MUTEX_INITIALIZER;  


void
setAdaptMode(int mode)
{
  adaptMode = mode;
}

void
setTranxOvhd(double ovhd)
{
  defaultTranxOvhd = ovhd * tranxOvhdScale;
}

void
setTranxInstr(int ld,int st)
{
  tranxInstrLd = ld;
  tranxInstrSt = st;
}

#if defined(HAVE_CPC)
int
Cpc_init(void)
{
  cpc = cpc_open(CPC_VER_CURRENT);
  if (cpc == 0)
    return -1;
  set = cpc_set_create(cpc);
  if (set == 0)
    return -1;
  ind0 = cpc_set_add_request(cpc,set,"Instr_cnt",0,
			     CPC_COUNT_SYSTEM|CPC_COUNT_USER,0,0);
  if (ind0 == -1)
    return -1;
  return 0;
}

int
Cpc_bind(cpc_t* cpc,cpc_set_t* set)
{
  return cpc_bind_curlwp(cpc,set,0);
}

int
Cpc_unbind(cpc_t* cpc,cpc_set_t* set)
{
  return cpc_unbind(cpc,set);
}

int
Cpc_sample(cpc_t* cpc,cpc_set_t* set,cpc_buf_t* before)
{
  return cpc_set_sample(cpc,set,before);
}

int
Cpc_diff(cpc_t* cpc,cpc_set_t* set,
	 cpc_buf_t* accum,cpc_buf_t* diff,
	 cpc_buf_t* before,cpc_buf_t* after)
{
  if (cpc_set_sample(cpc,set,after) == -1)
    return -1;
  if (diff == 0)
    return 0;
  cpc_buf_sub(cpc,diff,after,before);
  cpc_buf_add(cpc,accum,accum,diff);
  return 0;
}
#elif defined(HAVE_OBSOLETE_CPC)
int
Cpc_init(void)
{
  int cpuver;
  char* setting;

  if (cpc_version(CPC_VER_CURRENT) != CPC_VER_CURRENT)
    return -1;
  if ((cpuver = cpc_getcpuver()) == -1)
    return -1;

  setting = "pic0=Cycle_cnt,pic1=Instr_cnt";
  if (cpc_strtoevent(cpuver,setting,&cpc) != 0)
    return -1;
  if (cpc_access() == -1)
    return -1;
  cpc_count_usr_events(1);
  cpc_count_sys_events(1);
  return 0;
}

int
Cpc_bind(cpc_event_t* cpc)
{
  return cpc_bind_event(cpc,0);
}

int
Cpc_unbind(void)
{
  return cpc_rele();
}

int
Cpc_sample(cpc_event_t* before)
{
  return cpc_take_sample(before);
}

int
Cpc_diff(cpc_event_t* accum,cpc_event_t* diff,
	 cpc_event_t* before,cpc_event_t* after)
{
  if (cpc_take_sample(after) == -1)
    return -1;
  if (diff == 0)
    return 0;
  cpc_event_diff(diff,after,before);
  cpc_event_accum(accum,diff);
  return 0;
}
#endif

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
al_init(al_t* lock,char* name)
{
  lock->name = name;
  lock->state = 0;
  lock->statistic = 0;
  lock->triesCommits = 0; 
  lock->tranxOvhd = 0;
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
  self->tl2Thread = TxNewThread();
  if (self->tl2Thread == 0) {
    free(self);
    return (void*)EAGAIN;
  }
  TxInitThread(self->tl2Thread,(long)pthread_self());
  self->lock = 0;
  self->transactMode = 0;
  self->nestLevel = 0;
  pthread_setspecific(_al_key,self);
#ifdef TLS
  _al_self = self;
#endif
#if defined(HAVE_CPC)
  uint64_t cpcAtom[1];

  if (Cpc_bind(cpc,set) == -1)
    return (void*)EAGAIN;
  if ((self->cpcAtom  = cpc_buf_create(cpc,set)) == 0) abort();
  if ((self->cpcStrt0 = cpc_buf_create(cpc,set)) == 0) abort();
#if  defined(HAVE_AND_DEPEND_ON_CPC)
  if ((self->cpcTrnx  = cpc_buf_create(cpc,set)) == 0) abort();
  if ((self->cpcStrt1 = cpc_buf_create(cpc,set)) == 0) abort();
#endif
  if ((self->cpcStop  = cpc_buf_create(cpc,set)) == 0) abort();
  if ((self->cpcDiff  = cpc_buf_create(cpc,set)) == 0) abort();
  cpc_buf_zero(cpc,self->cpcAtom);
  cpc_set_restart(cpc,set);
  Cpc_sample(cpc,set,self->cpcStrt0);
  Cpc_diff(cpc,set,self->cpcAtom,self->cpcDiff,self->cpcStrt0,self->cpcStop);
  cpc_buf_get(cpc,self->cpcAtom,ind0,&cpcAtom[0]);
  instrCntOvhd = cpcAtom[0];
#elif defined(HAVE_OBSOLETE_CPC)
  if (Cpc_bind(&cpc) == -1)
    return (void*)EAGAIN;
  memset(&self->cpcAtom,0,sizeof(cpc_event_t));
  Cpc_sample(&self->cpcStrt0);
  Cpc_diff(&self->cpcAtom,&self->cpcDiff,&self->cpcStrt0,&self->cpcStop);
  instrCntOvhd = self->cpcAtom.ce_pic[0];
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
  unsigned long tranxOvhd;

  if (adaptMode == -1) return 0;
  if (adaptMode == 1) return 1;
  state = lock->state;
  statistic = lock->statistic;
  thrdsContend = contention(statistic);
  if (spins == 0) thrdsContend++;
  thrdsInTranx = thrdsInStmMode(state);
  if (0 < thrdsInTranx) thrdsContend += thrdsInTranx;
#if 0
  else if (lockHeld(state)) thrdsContend++;
#endif

  triesCommits = lock->triesCommits;
  Tries = tries(triesCommits);
  Commits = commits(triesCommits);
  if (0 == Commits) { Commits = 1; Tries = 1; }
  if ((tranxOvhd = lock->tranxOvhd) == 0) {
    // On the first run, tranxOvhd is set to zero which then enforces
    // transaction mode to estimate the initial overhead value.
    lock->tranxOvhd = defaultTranxOvhd;
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
  unsigned long Tries;
  unsigned long Commits;
  unsigned long tranxOvhd;

  if (adaptMode == -1) return 0;
  if (adaptMode == 1) return 1;
  state = lock->state;
  statistic = lock->statistic;
  thrdsContend = contention(statistic);
  thrdsInTranx = thrdsInStmMode(state);
  if (0 < thrdsInTranx) thrdsContend += thrdsInTranx;
#if 0
  else if (lockHeld(state)) thrdsContend++;
#endif
  Tries = tries(statistic);
  Commits = commits(statistic);
  if (0 == Commits) { Commits = 1; Tries = 1; }
  if ((tranxOvhd = lock->tranxOvhd)  == 0)
    lock->tranxOvhd = defaultTranxOvhd;

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
	  next = setThrdsInStmMode(prev,1);				\
	  next = setTransition(next,0);					\
	  assert(lockMode(next) == 0);					\
	  assert(lockHeld(next) == 0);					\
	  assert(thrdsInStmMode(next));					\
	  assert(transition(next) == 0);				\
	  if (CAS(lock->state,prev,next) == prev) break;		\
	}								\
      } else {								\
	if (thrdsInStmMode(prev) == 0) {				\
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
StmStSized(Thread* self,intptr_t* dest,intptr_t* src,size_t size)
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
StmLdSized(Thread* self,intptr_t* dest,intptr_t* src,size_t size)
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

void
StxStart(thread_t* self,sigjmp_buf* buf,int* ro)
{
  self->txLd = 0;
  self->txSt = 0;
#if defined(HAVE_CPC)
#ifdef HAVE_AND_DEPEND_ON_CPC
  cpc_buf_zero(cpc,self->cpcAtom);
  cpc_buf_zero(cpc,self->cpcTrnx);
  cpc_set_restart(cpc,set);
  Cpc_sample(cpc,set,self->cpcStrt0);
  TxStart(self->tl2Thread,buf,ro);
  Cpc_diff(cpc,set,self->cpcTrnx,self->cpcDiff,self->cpcStrt0,self->cpcStop);
#else
  cpc_buf_zero(cpc,self->cpcAtom);
  cpc_set_restart(cpc,set);
  Cpc_sample(cpc,set,self->cpcStrt0);
  TxStart(self->tl2Thread,buf,ro);
#endif
#elif defined(HAVE_OBSOLETE_CPC)
#if defined(HAVE_AND_DEPEND_ON_CPC)
  memset(&self->cpcAtom,0,sizeof(cpc_event_t));
  memset(&self->cpcTrnx,0,sizeof(cpc_event_t));
  Cpc_sample(&self->cpcStrt0);
  TxStart(self->tl2Thread,buf,ro);
  Cpc_diff(&self->cpcTrnx,&self->cpcDiff,&self->cpcStrt0,&self->cpcStop);
#else
  memset(&self->cpcAtom,0,sizeof(cpc_event_t));
  Cpc_sample(&self->cpcStrt0);
  TxStart(self->tl2Thread,buf,ro);
#endif
#else
  TxStart(self->tl2Thread,buf,ro);
#endif
}

void
StxCommit(thread_t* self)
{
#if defined(HAVE_CPC)
  uint64_t cpcAtom[1];
  uint64_t cpcTrnx[1];
  uint64_t atom;
  uint64_t trnx;
  int tranxOvhdNew;

#ifdef HAVE_AND_DEPEND_ON_CPC
  Cpc_sample(cpc,set,self->cpcStrt1);
  TxCommit(self->tl2Thread);
  Cpc_diff(cpc,set,self->cpcTrnx,self->cpcDiff,self->cpcStrt1,self->cpcStop);
  Cpc_diff(cpc,set,self->cpcAtom,self->cpcDiff,self->cpcStrt0,self->cpcStop);
  cpc_buf_get(cpc,self->cpcAtom,ind0,&cpcAtom[0]);
  cpc_buf_get(cpc,self->cpcTrnx,ind0,&cpcTrnx[0]);
  atom = cpcAtom[0];
  trnx = cpcTrnx[0];
#else
  TxCommit(self->tl2Thread);
  Cpc_diff(cpc,set,self->cpcAtom,self->cpcDiff,self->cpcStrt0,self->cpcStop);
  cpc_buf_get(cpc,self->cpcAtom,ind0,&cpcAtom[0]);
  atom = cpcAtom[0] - instrCntOvhd;
  trnx = tranxInstrLd * self->txLd + tranxInstrSt * self->txSt;
#endif
  if (self->lock->tranxOvhd == 0)
    self->lock->tranxOvhd = defaultTranxOvhd;
  if (0 < atom && trnx < atom && 1000 < (atom-trnx)) {
    tranxOvhdNew = (((double)atom) / ((double)(atom-trnx))) * tranxOvhdScale;
    self->lock->tranxOvhd = (self->lock->tranxOvhd + tranxOvhdNew) / 2;
  }
#elif defined(HAVE_OBSOLETE_CPC)
  uint64_t atom;
  uint64_t trnx;
  int tranxOvhdNew;

#ifdef HAVE_AND_DEPEND_ON_CPC
  Cpc_sample(&self->cpcStrt1);
  TxCommit(self->tl2Thread);
  Cpc_diff(&self->cpcTrnx,&self->cpcDiff,&self->cpcStrt1,&self->cpcStop);
  Cpc_diff(&self->cpcAtom,&self->cpcDiff,&self->cpcStrt0,&self->cpcStop);
  atom = self->cpcAtom.ce_pic[0];
  trnx = self->cpcTrnx.ce_pic[0];
#else
  TxCommit(self->tl2Thread);
  Cpc_diff(&self->cpcAtom,&self->cpcDiff,&self->cpcStrt0,&self->cpcStop);
  atom = self->cpcAtom.ce_pic[0] - instrCntOvhd;
  trnx = tranxInstrLd * self->txLd + tranxInstrSt * self->txSt;
#endif
  if (self->lock->tranxOvhd == 0)
    self->lock->tranxOvhd = defaultTranxOvhd;
  if (trnx < atom) {
    tranxOvhdNew = (((double)atom) / ((double)(atom-trnx))) * tranxOvhdScale;
    self->lock->tranxOvhd = (self->lock->tranxOvhd + tranxOvhdNew) / 2;
  }
#else
  TxCommit(self->tl2Thread);
#endif
}

void
StxStore(thread_t* self,intptr_t* dest,intptr_t src)
{
#if defined(HAVE_CPC) && defined(HAVE_AND_DEPEND_ON_CPC)
  Cpc_sample(cpc,set,self->cpcStrt1);
  TxStore(self->tl2Thread,dest,src);
  Cpc_diff(cpc,set,self->cpcTrnx,self->cpcDiff,self->cpcStrt1,self->cpcStop);
#elif defined(HAVE_OBSOLETE_CPC) && defined(HAVE_AND_DEPEND_ON_CPC)
  Cpc_sample(&self->cpcStrt1);
  TxStore(self->tl2Thread,dest,src);
  Cpc_diff(&self->cpcTrnx,&self->cpcDiff,&self->cpcStrt1,&self->cpcStop);
#else
  TxStore(self->tl2Thread,dest,src);
#endif
  self->txSt++;
}

intptr_t
StxLoad(thread_t* self,intptr_t* src)
{
  intptr_t value;
#if defined(HAVE_CPC) && defined(HAVE_AND_DEPEND_ON_CPC)
  Cpc_sample(cpc,set,self->cpcStrt1);
  value = TxLoad(self->tl2Thread,src);
  Cpc_diff(cpc,set,self->cpcTrnx,self->cpcDiff,self->cpcStrt1,self->cpcStop);
#elif defined(HAVE_OBSOLETE_CPC) && defined(HAVE_AND_DEPEND_ON_CPC)
  Cpc_sample(&self->cpcStrt1);
  value = TxLoad(self->tl2Thread,src);
  Cpc_diff(&self->cpcTrnx,&self->cpcDiff,&self->cpcStrt1,&self->cpcStop);
#else
  value = TxLoad(self->tl2Thread,src);
#endif
  self->txLd++;
  return value;
}

void
StxStSized(thread_t* self,intptr_t* dest,intptr_t* src,size_t size)
{
  int n,i;

  if ((size % sizeof(intptr_t)) != 0) {
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
	    __FILE__,__LINE__,__func__);
    abort();
  }
  n = size / sizeof(intptr_t);
  for (i = 0; i < n; i++)
    StxStore(self,dest+i,*(src+i));
  return;
}

void
StxLdSized(thread_t* self,intptr_t* dest,intptr_t* src,size_t size)
{
  int n,i;

  if ((size % sizeof(intptr_t)) != 0) {
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
	    __FILE__,__LINE__,__func__);
    abort();
  }
  n = size / sizeof(intptr_t);
  for (i = 0; i < n; i++)
    *(dest+i) = StxLoad(self,src+i);
  return;
}

void*
StxAlloc(thread_t* self,size_t size)
{
  return TxAlloc(self->tl2Thread,size);
}

void
StxFree(thread_t* self,void* ptr)
{
  TxFree(self->tl2Thread,ptr);
}

#ifdef HAVE_GETHRTIME
void
timer_start(hrtime_t* start)
{
  *start = gethrtime();
}

void
timer_stop(hrtime_t* start,hrtime_t* accum)
{
  hrtime_t stop;

  stop = gethrtime();
  stop -= *start;
  *accum += stop;
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
timer_stop(struct timeval* start,struct timeval* accum)
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
  accum->tv_sec += stop.tv_sec;
  accum->tv_usec += stop.tv_usec;
  if (1000000 <= accum->tv_usec) {
    accum->tv_sec++;
    accum->tv_usec -= 1000000;
  }
}
#endif

void
al_dump(al_t* lock)
{
  printf("%s:tranxOvhd=%d/%d,tries=%ld,commits=%ld\n",
	 lock->name,lock->tranxOvhd,tranxOvhdScale,
	 tries(lock->triesCommits),commits(lock->triesCommits));
}

static void
destroy_thread(void* arg)
{
  thread_t* self = (thread_t*)arg;

#if 0
#if defined(HAVE_CPC)
  Cpc_unbind(cpc,set);
#elif defined(HAVE_OBSOLETE_CPC)
  Cpc_unbind();
#endif
#endif
  pthread_mutex_lock(&globMutex);
  txLd += self->txLd;
  txSt += self->txSt;
  pthread_mutex_unlock(&globMutex);
  TxFreeThread(self->tl2Thread);
  free(arg);
}

__attribute__((constructor)) void
init(void)
{
  TxOnce();
  pthread_key_create(&_al_key,destroy_thread);
#if defined(HAVE_CPC)
  if (Cpc_init() == -1)
    abort();
#elif defined(HAVE_OBSOLETE_CPC)
  if (Cpc_init() == -1)
    abort();
#endif
}

__attribute__((destructor)) void
finish(void)
{
  printf("tranxInstrLd=%d,tranxInstrSt=%d,instrCntOvhd=%lld\n",
	 tranxInstrLd,tranxInstrSt,instrCntOvhd);
  TxShutdown();
}
