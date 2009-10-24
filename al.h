#ifndef _AL_H
#define _AL_H
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdlib.h>
#include "stm.h"
#include "port.h"
#include "queue.h"

typedef struct {
  const char* name;
  volatile intptr_t lockHeld;
  volatile intptr_t threadsWaiting;
  volatile intptr_t tries;
  volatile intptr_t commit;
  volatile intptr_t invokeInLockMode;
  volatile intptr_t waitLock;
} profile_t;

typedef struct _nest_t {
  profile_t* prof;
  long level;
  SLIST_ENTRY(_nest_t) next;
} nest_t;

typedef struct {
  Thread* stmThread;
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
void dump_profile(profile_t*);

#endif
