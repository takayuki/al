#ifndef _ALX_H
#define _ALX_H
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include <sys/time.h>
#include "al.h"

static int spin_thrld = 100;

void
_al_template(void)
{
  profile_t* prof = 0;
  void* (*rawfunc)(void) = 0;
  void* (*stmfunc)(Thread*) = 0;
  int ro = 0;
  int spins;
  thread_t* self;
  nest_t* nest;
  nest_t* nested;
  intptr_t prev,next;
  int useTransact;
  volatile unsigned long tries;
  sigjmp_buf buf;
#ifdef HAVE_GETHRTIME
  hrtime_t start;
#else
  struct timeval start;
#endif

  self = pthread_getspecific(_al_key);
  if (self == 0) abort();
  SLIST_FOREACH(nest,&self->prof_list,next) {
    if (nest->prof == prof) break;
  }
  if (nest == 0) {
    nest = malloc(sizeof(*nest));
    if (nest == 0) abort();
    nest->prof = prof;
    nest->level = 0;
    SLIST_INSERT_HEAD(&self->prof_list,nest,next);
  }
  SLIST_FOREACH(nested,&self->prof_list,next) {
    if (0 < nested->level) break;
  }
  if (nested != 0) {
    assert(0 <= nest->level);
    stmfunc(self->stmThread);
  } else if (nest->level < 0) {
    rawfunc();
  } else {
    assert(nest->level == 0);
    spins = 0;
    while (1) {
      prev = prof->state;
      if (transition(prev) == 0) {
	if ((useTransact = transactMode(prof))) {
	  if (lockHeld(prev) == 0) {
	    next = setLockMode(prev,0);
	    next = setThrdsInStmMode(next,thrdsInStmMode(next)+1);
	    assert(lockMode(next) == 0);
	    assert(lockHeld(next) == 0);
	    assert(thrdsInStmMode(next));
	    assert(transition(next) == 0);
	    if (CAS(prof->state,prev,next) == prev) break;
	  } else {
	    next = setLockMode(prev,0);
	    next = setTransition(next,1);
	    assert(lockMode(next) == 0);
	    assert(lockHeld(next));
	    assert(thrdsInStmMode(next) == 0);
	    assert(transition(next));
	    CAS(prof->state,prev,next);
	  }
	} else {
	  if (lockHeld(prev) == 0 && thrdsInStmMode(prev) == 0) {
	    next = setLockMode(prev,1);
	    next = setLockHeld(next,1);
	    assert(lockMode(next));
	    assert(lockHeld(next));
	    assert(thrdsInStmMode(next) == 0);
	    assert(transition(next) == 0);
	    if (CAS(prof->state,prev,next) == prev) break;
	  } else if (lockMode(prev) == 0) {
	    next = setLockMode(prev,1);
	    next = setTransition(next,1);
	    assert(lockMode(next));
	    assert(lockHeld(next) == 0);
	    assert(thrdsInStmMode(next));
	    assert(transition(next));
	    CAS(prof->state,prev,next);
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
	    if (CAS(prof->state,prev,next) == prev) break;
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
	    if (CAS(prof->state,prev,next) == prev) break;
	  }
	}
      }
      if (spins == 0) INC(prof->statistic);
      if (spin_thrld < ++spins) Yield();
    }
    if (0 < spins) DEC(prof->statistic);
    if (useTransact) {
      tries = 0;
      if (sigsetjmp(buf,1)) nest->level = 0;
      timer_start(&start);
      inc(nest->level);
      inc(tries);
      TxStart(self->stmThread,&buf,&ro);
      stmfunc(self->stmThread);
      TxCommit(self->stmThread);
      dec(nest->level);
      prof->triesCommits = setTriesCommits(prof->triesCommits,tries);
      while (1) {
	prev = prof->state;
	assert(lockHeld(prev) == 0);
	assert(thrdsInStmMode(prev));
	next = setThrdsInStmMode(prev,thrdsInStmMode(prev)-1);
	if (CAS(prof->state,prev,next) == prev) break;
      }
      timer_stop(&start,&self->timeSTM);
    } else {
      timer_start(&start);
      nest->level = -1;
      rawfunc();
      nest->level = 0;
      while (1) {
	prev = prof->state;
	assert(lockHeld(prev));
	assert(thrdsInStmMode(prev) == 0);
	next = setLockHeld(prev,0);
	if (CAS(prof->state,prev,next) == prev) break;
      }
      timer_stop(&start,&self->timeRaw);
    }
  }
  return;
}
#endif
