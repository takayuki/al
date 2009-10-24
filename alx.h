#ifndef _ALX_H
#define _ALX_H
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include <sys/time.h>
#include "al.h"

static int default_spins = 100;

static void
_al_template(void)
{
  profile_t* prof = 0;
  void* (*rawfunc)(void) = 0;
  void* (*stmfunc)(Thread*) = 0;
  int ro = 0;
  int cnt = default_spins;
  thread_t* self;
  nest_t* nest;
  nest_t* nested;
  intptr_t tmp;
  unsigned long tries = 0;
  sigjmp_buf buf;
  struct timeval start;

  assert((self = pthread_getspecific(_al_key)));
  SLIST_FOREACH(nest,&self->prof_list,next) {
    if (nest->prof == prof) break;
  }
  if (nest == 0) {
    assert(nest = malloc(sizeof(*nest)));
    nest->prof = prof;
    nest->level = 0;
    SLIST_INSERT_HEAD(&self->prof_list,nest,next);
  }
  SLIST_FOREACH(nested,&self->prof_list,next) {
    if (0 < nested->level) break;
  }
  if (nested != 0 || (nest->level == 0 && transactMode(prof))) {
    assert(0 <= nest->level);
    if (nested != 0) {
      stmfunc(self->stmThread);
    } else {
      INC(prof->threadsWaiting);
      while ((tmp = prof->lockHeld) == ~0L ||
             CAS(prof->lockHeld,tmp,tmp+1) != tmp) {
	if (--cnt <= 0) { busy(); cnt = default_spins; }
      }
      DEC(prof->threadsWaiting);
      if (sigsetjmp(buf,1)) nest->level = 0;
      timer_start(&start);
      inc(nest->level);
      inc(tries);
      TxStart(self->stmThread,&buf,&ro);
      stmfunc(self->stmThread);
      TxCommit(self->stmThread);
      commit(prof->triesCommit,tries);
      dec(nest->level);
      DEC(prof->lockHeld);
      timer_stop(&start,&self->timeSTM);
    }
  } else {
    assert(nest->level <= 0);
    if (nest->level < 0) {
      rawfunc();
    } else {
      INC(prof->threadsWaiting);
      while (prof->lockHeld != 0 ||
	     CAS(prof->lockHeld,0,~0L) != 0) {
	if (--cnt <= 0) { busy(); cnt = default_spins; }
      }
      DEC(prof->threadsWaiting);
      timer_start(&start);
      nest->level = -1;
      rawfunc();
      nest->level = 0;
      INC(prof->lockHeld);
      timer_stop(&start,&self->timeRaw);
    }
  }
  return;
}
#endif
