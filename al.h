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

typedef struct {
  Thread* stmThread;
  int nestLevel;
} _thread_t;

typedef struct {
  const char* name;
  volatile intptr_t lockHeld;
  volatile intptr_t threadsWaiting;
  volatile intptr_t tries;
  volatile intptr_t commit;
  volatile intptr_t invokeInLockMode;
  volatile intptr_t waitLock;
} _profile_t;

#define PROFILE_INITIALIZER  {0,0,0,0,0,0,0}

int al_pthread_create(pthread_t*,const pthread_attr_t*,void* (*)(void*),void*);
#define pthread_create al_pthread_create

extern pthread_key_t _al_key;
int setAdaptMode(int);
double setTransactOvhd(double);
int transactMode(_profile_t*);
void busy(void);
void dump_profile(_profile_t*);

#endif
