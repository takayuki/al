#ifndef _ALX_H
#define _ALX_H
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include <stdio.h>
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
  nest_t* nestedTransact;
  unsigned long tries;
  unsigned long prev,next;
  sigjmp_buf buf;
#ifndef ENABLE_TIMER
#define timer_start(x)
#define timer_stop(w,x,y,z)
#else
#ifdef HAVE_GETHRTIME
  hrtime_t start;
#else
  struct timeval start;
#endif
#endif

  self = pthread_getspecific(_al_key);
  if (self == 0) {
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
	    __FILE__,__LINE__,__func__);
    abort();
  }
  nestedTransact = 0;
  if (!SLIST_EMPTY(&self->lock_list)) {
    nest = SLIST_FIRST(&self->lock_list);
    if (nest->level == 0) {
      if (!SLIST_NEXT(SLIST_FIRST(&self->lock_list),next)) {
	nest->lock = _lock;
	goto reuse;
      }
      SLIST_REMOVE_HEAD(&self->lock_list,next);
      free(nest);
      nest = SLIST_FIRST(&self->lock_list);
    }
    if (0 < nest->level)
      nestedTransact = nest;
    if (nest->lock == _lock)
      goto reuse;
  }
  nest = malloc(sizeof(*nest));
  if (nest == 0) {
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
	    __FILE__,__LINE__,__func__);
    abort();
  }
  nest->lock = _lock;
  nest->level = 0;
  SLIST_INSERT_HEAD(&self->lock_list,nest,next);
 reuse:
  if (nestedTransact != 0) {
    assert(0 <= nest->level);
    _stmfunc(self->stmThread);
  } else if (nest->level < 0) {
    _rawfunc();
  } else {
    assert(nest->level == 0);
    switch (getLockScheme()) {
    case 2:
      do {
        if (sigsetjmp(buf,1)) nest->level = 0;
        timer_start(&start);
	inc(nest->level);
        TxStart(self->stmThread,&buf,&_ro);
        _stmfunc(self->stmThread);
        TxCommit(self->stmThread);
	dec(nest->level);
        exitCritical_2(_lock);
        timer_stop(&start,&self->timeSTM,_lock,0);
      } while (0);
      break;
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
	timer_stop(&start,&self->timeSTM,_lock,0);
      } else {
	timer_start(&start);
	nest->level = -1;
	_rawfunc();
	nest->level = 0;
	exitCritical_1(_lock);
	timer_stop(&start,&self->timeRaw,_lock,0);
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
	timer_stop(&start,&self->timeSTM,_lock,1);
      } else {
	timer_start(&start);
	nest->level = -1;
	_rawfunc();
	nest->level = 0;
	exitCritical_0(_lock);
	timer_stop(&start,&self->timeRaw,_lock,0);
      }
      break;
    }
  }
  return;
}
#endif
