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
  al_t* _lock = 0;
  void* (*_rawfunc)(void) = 0;
  void* (*_stmfunc)(Thread*) = 0;
  int _ro = 0;
  thread_t* self;
  nest_t* nest;
  nest_t* nested;
  unsigned long tries;
  unsigned long prev,next;
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
    if (nest->lock == _lock) break;
  }
  if (nest == 0) {
    nest = malloc(sizeof(*nest));
    if (nest == 0) abort();
    nest->lock = _lock;
    nest->level = 0;
    SLIST_INSERT_HEAD(&self->lock_list,nest,next);
  }
  SLIST_FOREACH(nested,&self->lock_list,next) {
    if (0 < nested->level) break;
  }
  if (nested != 0) {
    assert(0 <= nest->level);
    _stmfunc(self->stmThread);
  } else if (nest->level < 0) {
    _rawfunc();
  } else {
    assert(nest->level == 0);
    switch (getLockScheme()) {
    case 1:
      if (enterCritical_1(_lock)) {
	tries = 0;
	if (sigsetjmp(buf,1)) nest->level = 0;
	timer_start(&start);
	inc(nest->level);
	inc(tries);
	TxStart(self->stmThread,&buf,&_ro);
	_stmfunc(self->stmThread);
	TxCommit(self->stmThread);
	dec(nest->level);
	_lock->triesCommits = setTriesCommits(_lock->triesCommits,tries);
	exitCritical_1(_lock);
	timer_stop(&start,&self->timeSTM);
      } else {
	timer_start(&start);
	nest->level = -1;
	_rawfunc();
	nest->level = 0;
	exitCritical_1(_lock);
	timer_stop(&start,&self->timeRaw);
      }
      break;
    default:
      if (enterCritical_0(_lock)) {
	tries = 0;
	if (sigsetjmp(buf,1)) nest->level = 0;
	timer_start(&start);
	inc(nest->level);
	inc(tries);
	TxStart(self->stmThread,&buf,&_ro);
	_stmfunc(self->stmThread);
	TxCommit(self->stmThread);
	dec(nest->level);
	while (1) {
	  prev = _lock->statistic;
	  next = setTriesCommits(prev,tries);
	  if (CAS(_lock->statistic,prev,next) == prev) break;
	}
	exitCritical_0(_lock);
	timer_stop(&start,&self->timeSTM);
      } else {
	timer_start(&start);
	nest->level = -1;
	_rawfunc();
	nest->level = 0;
	exitCritical_0(_lock);
	timer_stop(&start,&self->timeRaw);
      }
      break;
    }
  }
  return;
}
#endif
