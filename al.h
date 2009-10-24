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
#ifndef _AL_H
#define _AL_H
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdlib.h>
#include <sys/time.h>
#ifdef HAVE_LIBCPC_H
#include <libcpc.h>
#endif
#include "stm.h"
#include "port.h"

typedef struct {
  const char* name;
  volatile unsigned long state;
  volatile unsigned long statistic;
  volatile unsigned long triesCommits;
  volatile long tranxOvhd;
  volatile unsigned long lockDone;
} al_t;

#define lockMode(x)		((x)&0x80000000UL)
#define lockHeld(x)		((x)&0x40000000UL)
#define transition(x)		((x)&0x20000000UL)
#define thrdsInStmMode(x)	((x)&0x1FFFFFFFUL)
#define setLockMode(x,y)	(((x)&(~0x80000000UL))|((y)<<31))
#define setLockHeld(x,y)	(((x)&(~0x40000000UL))|((y)<<30))
#define setTransition(x,y)	(((x)&(~0x20000000UL))|((y)<<29))
#define setThrdsInStmMode(x,y)	(((x)&(~0x1FFFFFFFUL))|((y)))
#define contention(x)		((x)&0x000003FFUL)
#define commits(x)		(((x)&0x000FFC00UL)>>10)
#define tries(x)		(((x)&0x7FF00000UL)>>20)
#define setContention(x,y)	(((x)&(~0x000003FFUL))|((y)))
#define setTriesCommits(x,y)					\
  ({unsigned long _x = x;					\
    unsigned long _tries,_commits;				\
    unsigned long _c = contention(_x);				\
    _tries = tries(_x) + (y); _commits = commits(_x) + 1;	\
    while (_tries > 0x7FFUL || _commits > 0x3FFUL) {		\
      _tries >>= 1; _commits >>= 1;				\
    }								\
    _x = (_tries<<20) | (_commits<<10);				\
    if (tries(_x) != 0 && commits(_x) == 0) {			\
      _x = (0x7FFUL<<20) | (0x1UL<<10);				\
    }								\
    (_x|_c);})

typedef struct {
  Thread* tl2Thread;
  al_t* lock;
  long enableStatistic;
  long nestLevel;
  long txLd;
  long txSt;
#if defined(HAVE_CPC)
  cpc_buf_t* cpcAtom;
  cpc_buf_t* cpcStrt0;
#ifdef HAVE_AND_DEPEND_ON_CPC
  cpc_buf_t* cpcTrnx;
  cpc_buf_t* cpcStrt1;
#endif
  cpc_buf_t* cpcStop;
  cpc_buf_t* cpcDiff;
#elif defined(HAVE_OBSOLETE_CPC)
  cpc_event_t cpcAtom;
  cpc_event_t cpcStrt0;
#ifdef HAVE_AND_DEPEND_ON_CPC
  cpc_event_t cpcTrnx;
  cpc_event_t cpcStrt1;
#endif
  cpc_event_t cpcStop;
  cpc_event_t cpcDiff;
#endif
} thread_t;

#define AL_INITIALIZER(name)  {(name),0,0,0,0,0}

int al_pthread_create(pthread_t*,const pthread_attr_t*,void* (*)(void*),void*);
#define pthread_create al_pthread_create
thread_t* thread_self(void);
void al_init(al_t*,char*);
void setAdaptMode(int);
void setTranxOvhd(double);
void setTranxInstr(int,int);
int enterCritical_0(al_t*);
int enterCritical_1(al_t*);
void exitCritical_0(al_t*);
void exitCritical_1(al_t*);
intptr_t LocalStore(intptr_t*,intptr_t);
intptr_t LocalLoad(intptr_t*);
float LocalStoreF(float*,float);
float LocalLoadF(float*);
void StmStSized(Thread*,intptr_t*,intptr_t*,size_t);
void StmLdSized(Thread*,intptr_t*,intptr_t*,size_t);
void StxStart(thread_t*,sigjmp_buf*,int*);
void StxCommit(thread_t*);
void StxStore(thread_t*,intptr_t*,intptr_t);
intptr_t StxLoad(thread_t*,intptr_t*);
void StxStSized(thread_t*,intptr_t*,intptr_t*,size_t);
void StxLdSized(thread_t*,intptr_t*,intptr_t*,size_t);
void* StxAlloc (thread_t*,size_t);
void StxFree(thread_t*,void*);
void RaxStart(thread_t*,sigjmp_buf*,int*);
void RaxCommit(thread_t*);
void RaxStore(thread_t*,intptr_t*,intptr_t);
intptr_t RaxLoad(thread_t*,intptr_t*);
void RaxStSized(thread_t*,intptr_t*,intptr_t*,size_t);
void RaxLdSized(thread_t*,intptr_t*,intptr_t*,size_t);
void* RaxAlloc (thread_t*,size_t);
void RaxFree(thread_t*,void*);
#ifdef HAVE_GETHRTIME
void timer_start(hrtime_t*);
void timer_stop(hrtime_t*,hrtime_t*);
#else
void timer_start(struct timeval*);
void timer_stop(struct timeval*,struct timeval*);
#endif
void al_dump(al_t*);

#endif
