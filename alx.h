#ifndef _ALX_H
#define _ALX_H
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include <sys/time.h>
#include "al.h"

void
_al_template(void)
{
  al_t* lock = 0;
  void* (*rawfunc)(void) = 0;
  void* (*stmfunc)(Thread*) = 0;
  int ro = 0;
  thread_t* self;
  nest_t* nest;
  nest_t* nested;
  unsigned long tries;
  sigjmp_buf buf;
#ifdef HAVE_GETHRTIME
  hrtime_t start;
#else
  struct timeval start;
#endif

  self = pthread_getspecific(_al_key);
  if (self == 0)
    abort();
  SLIST_FOREACH(nest,&self->lock_list,next) {
    if (nest->lock == lock) break;
  }
  if (nest == 0) {
    nest = malloc(sizeof(*nest));
    if (nest == 0) abort();
    nest->lock = lock;
    nest->level = 0;
    SLIST_INSERT_HEAD(&self->lock_list,nest,next);
  }
  SLIST_FOREACH(nested,&self->lock_list,next) {
    if (0 < nested->level) break;
  }
  if (nested != 0) {
    assert(0 <= nest->level);
    stmfunc(self->stmThread);
  } else if (nest->level < 0) {
    rawfunc();
  } else {
    assert(nest->level == 0);
    if (enterCritical(lock)) {
      tries = 0;
      if (sigsetjmp(buf,1)) nest->level = 0;
      timer_start(&start);
      inc(nest->level);
      inc(tries);
      TxStart(self->stmThread,&buf,&ro);
      stmfunc(self->stmThread);
      TxCommit(self->stmThread);
      dec(nest->level);
      lock->triesCommits = setTriesCommits(lock->triesCommits,tries);
      exitCritical(lock);
      timer_stop(&start,&self->timeSTM);
    } else {
      timer_start(&start);
      nest->level = -1;
      rawfunc();
      nest->level = 0;
      exitCritical(lock);
      timer_stop(&start,&self->timeRaw);
    }
  }
  return;
}
#endif
