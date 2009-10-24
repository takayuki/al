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
  sigjmp_buf buf;

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
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
            __FILE__,__LINE__,__func__);
    abort();
  } else if (self->nestLevel == 0) {
    self->lock = _lock;
    if (sigsetjmp(buf,1)) self->nestLevel = 0;
    inc(self->nestLevel);
    TxStart(self->stmThread,&buf,&_ro);
    _stmfunc(self->stmThread);
    TxCommit(self->stmThread);
    dec(self->nestLevel);
  } else {
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
	    __FILE__,__LINE__,__func__);
    abort();
  }
  return;
}
#endif
