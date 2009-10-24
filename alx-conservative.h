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

  self = thread_self();
  if (self == 0) {
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
	    __FILE__,__LINE__,__func__);
    abort();
  }
  if (self->lock == _lock && 0 < self->nestLevel) {
    _stmfunc(self->stmThread);
    return;
  } else if (self->lock == _lock && self->nestLevel < 0) {
    _rawfunc();
  } else if (self->nestLevel == 0) {
    self->lock = _lock;
    if (enterCritical_0(_lock)) {
      tries = 0;
      if (sigsetjmp(buf,1)) self->nestLevel = 0;
      timer_start(&start);
      inc(self->nestLevel);
      inc(tries);
      TxStart(self->stmThread,&buf,&_ro);
      _stmfunc(self->stmThread);
      TxCommit(self->stmThread);
      dec(self->nestLevel);
      while (1) {
	prev = _lock->statistic;
	next = setTriesCommits(prev,tries);
	if (CAS(_lock->statistic,prev,next) == prev) break;
      }
      exitCritical_0(_lock);
      timer_stop(&start,&self->timeSTM,_lock,1);
    } else {
      timer_start(&start);
      self->nestLevel = -1;
      _rawfunc();
      self->nestLevel = 0;
      exitCritical_0(_lock);
      timer_stop(&start,&self->timeRaw,_lock,0);
    }
  } else {
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
	    __FILE__,__LINE__,__func__);
    abort();
  }
  return;
}
#endif
