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
#include "queue.h"

typedef struct {
  const char* name;
  volatile unsigned long state;
  volatile unsigned long statistic;
  volatile unsigned long triesCommits;
} profile_t;

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

typedef struct _nest_t {
  profile_t* prof;
  long level;
  SLIST_ENTRY(_nest_t) next;
} nest_t;

typedef struct {
  Thread* stmThread;
#ifdef HAVE_GETHRTIME
  hrtime_t timeRaw;
  hrtime_t timeSTM;
#else
  struct timeval timeRaw;
  struct timeval timeSTM;
#endif
  SLIST_HEAD(,_nest_t) prof_list;
} thread_t;

#define PROFILE_INITIALIZER  {0,0,0,0,0,0,0}

int al_pthread_create(pthread_t*,const pthread_attr_t*,void* (*)(void*),void*);
#define pthread_create al_pthread_create

extern pthread_key_t _al_key;
int setAdaptMode(int);
double setTransactOvhd(double);
int transactMode(profile_t*);
void Yield(void);
void TxStoreSized(Thread*,intptr_t*,intptr_t*,size_t);
void TxLoadSized(Thread*,intptr_t*,intptr_t*,size_t);
#ifdef HAVE_GETHRTIME
void timer_start(hrtime_t*);
void timer_stop(hrtime_t*,hrtime_t*);
#else
void timer_start(struct timeval*);
void timer_stop(struct timeval*,struct timeval*);
#endif
void dump_profile(profile_t*);

#endif
