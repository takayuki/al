#ifndef _ALX_H
#define _ALX_H
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include "al.h"

static void
_al_template(void)
{
  _profile_t* prof = 0;
  void* (*rawfunc)(void) = 0;
  void* (*stmfunc)(Thread*) = 0;
  int ro = 0;
  _thread_t *self;
  intptr_t tmp;
  sigjmp_buf buf;
  int default_spins = 100;
  int cnt = default_spins;

  assert((self = pthread_getspecific(_al_key)));
  if ((self->nestLevel == 0 && transactMode(prof)) ||
      0 < self->nestLevel) {
    assert(0 <= self->nestLevel);
    if (self->nestLevel == 0) {
      inc(prof->threadsWaiting);
      while ((tmp = load(prof->lockHeld)) == ~0 ||
	     CAS(prof->lockHeld,tmp,tmp+1) != tmp) {
	if (--cnt <= 0) {
	  busy(); cnt = default_spins;
	}
      }
      dec(prof->threadsWaiting);
      if (sigsetjmp(buf,1)) self->nestLevel = 0;
      inc(self->nestLevel);
      inc(prof->tries);
      TxStart(self->stmThread,&buf,&ro);
      stmfunc(self->stmThread);
      TxCommit(self->stmThread);
      inc(prof->commit);
      dec(self->nestLevel);
      DEC(prof->lockHeld);
    } else {
      stmfunc(self->stmThread);
    }
  } else {
    assert(self->nestLevel <= 0);
    if (self->nestLevel == 0) {
      inc(prof->invokeInLockMode);
      inc(prof->threadsWaiting);
      if (load(prof->lockHeld) != 0 ||
	  CAS(prof->lockHeld,0,~0) != 0) {
	inc(prof->waitLock);
	while (load(prof->lockHeld) != 0 ||
	       CAS(prof->lockHeld,0,~0) != 0)
	  if (--cnt <= 0) {
	    busy(); cnt = default_spins;
	  }
      }
      dec(prof->threadsWaiting);
      store(self->nestLevel,-1);
      rawfunc();
      store(self->nestLevel,0);
      INC(prof->lockHeld);
    } else {
      rawfunc();
    }
  }
  return;
}
#endif
