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
  volatile long lockHeld;
  volatile long threadsWaiting;
  volatile unsigned long triesCommit;
} profile_t;

#ifdef __LP64__
#define high(x)		((unsigned long)((x)>>32))
#define low(x)		((unsigned long)((x)&0xFFFFFFFF))
#define commit(x,y)						\
  do {								\
    volatile unsigned long _x = x;				\
    if ((high(_x) + (y)) > 0xFFFFFFFF)				\
      _x = ((high(_x)>>1)<<32) | (low(_x)>>1);			\
    _x = ((high(_x)+(y))<<32) | (low(_x)+1);			\
    assert(low(_x) <= high(_x));				\
    x = _x;							\
  } while (0)
#else
#define high(x)		((unsigned long)((x)>>16))
#define low(x)		((unsigned long)((x)&0xFFFF))
#define commit(x,y)						\
  do {								\
    volatile unsigned long _x = x;				\
    if ((high(_x) + (y)) > 0xFFFF)				\
      _x = ((high(_x)>>1)<<16) | (low(_x)>>1);			\
    _x = ((high(_x)+(y))<<16) | (low(_x)+1);			\
    assert(low(_x) <= high(_x));				\
    x = _x;							\
  } while (0)
#endif

typedef struct _nest_t {
  profile_t* prof;
  long level;
  SLIST_ENTRY(_nest_t) next;
} nest_t;

typedef struct {
  Thread* stmThread;
  struct timeval timeRaw;
  struct timeval timeSTM;
  SLIST_HEAD(,_nest_t) prof_list;
} thread_t;

#define PROFILE_INITIALIZER  {0,0,0,0,0,0,0}

int al_pthread_create(pthread_t*,const pthread_attr_t*,void* (*)(void*),void*);
#define pthread_create al_pthread_create

extern pthread_key_t _al_key;
int setAdaptMode(int);
double setTransactOvhd(double);
int transactMode(profile_t*);
void busy(void);
void TxStoreSized(Thread*,intptr_t*,intptr_t*,size_t);
void TxLoadSized(Thread*,intptr_t*,intptr_t*,size_t);
void timer_start(struct timeval*);
void timer_stop(struct timeval*,struct timeval*);
void dump_profile(profile_t*);

#endif
