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
#include "stm.h"
#include "port.h"

typedef struct {
  const char* name;
  volatile unsigned long state;
  volatile unsigned long statistic;
  volatile unsigned long triesCommits;
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
  Thread* stmThread;
  al_t* lock;
  long nestLevel;
#ifdef HAVE_GETHRTIME
  hrtime_t timeRaw;
  hrtime_t timeSTM;
#else
  struct timeval timeRaw;
  struct timeval timeSTM;
#endif
} thread_t;

#define AL_INITIALIZER  {0,0,0,0}

int al_pthread_create(pthread_t*,const pthread_attr_t*,void* (*)(void*),void*);
#define pthread_create al_pthread_create

thread_t* thread_self(void);
void setAdaptMode(int);
void setTranxOvhd(double);
int enterCritical_0(al_t*);
int enterCritical_1(al_t*);
void exitCritical_0(al_t*);
void exitCritical_1(al_t*);
unsigned long Random(unsigned long*);
intptr_t LocalStore(intptr_t*,intptr_t);
intptr_t LocalLoad(intptr_t*);
float LocalStoreF(float*,float);
float LocalLoadF(float*);
void TxStoreSized(Thread*,intptr_t*,intptr_t*,size_t);
void TxLoadSized(Thread*,intptr_t*,intptr_t*,size_t);
#ifdef HAVE_GETHRTIME
void timer_start(hrtime_t*);
void timer_stop(hrtime_t*,hrtime_t*,al_t*,int);
#else
void timer_start(struct timeval*);
void timer_stop(struct timeval*,struct timeval*,al_t*,int);
#endif
void dump_profile(al_t*);

#endif
