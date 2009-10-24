/*
 * Al -- an implementation of the adaptive locks
 *
 * Copyright (C) 2008, University of Oregon
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 * 
 *   * Neither the name of University of Oregon nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF OREGON ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
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
  void* (*_stxfunc)(thread_t*) = 0;
  int _ro = 0;
  thread_t* self;
  volatile unsigned long tries;
  sigjmp_buf buf;

  self = thread_self();
  if (self == 0) {
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
	    __FILE__,__LINE__,__func__);
    abort();
  }
  if (self->lock == _lock && 0 < self->nestLevel) {
    if (self->transactMode == 1) _stmfunc(self->tl2Thread);
    else _stxfunc(self);
  } else if (self->lock == _lock && self->nestLevel < 0) {
    _rawfunc();
  } else if (self->nestLevel == 0) {
    self->lock = _lock;
    if (enterCritical_1(_lock)) {
      if (commits(_lock->triesCommits)%0x3FF) {
	self->transactMode = 1;
	tries = 0;
	if (sigsetjmp(buf,1)) self->nestLevel = 0;
	inc(self->nestLevel);
	inc(tries);
	TxStart(self->tl2Thread,&buf,&_ro);
	_stmfunc(self->tl2Thread);
	TxCommit(self->tl2Thread);
	dec(self->nestLevel);
	_lock->triesCommits = setTriesCommits(_lock->triesCommits,tries);
	exitCritical_1(_lock);
      } else {
	self->transactMode = 2;
	tries = 0;
	if (sigsetjmp(buf,1)) self->nestLevel = 0;
	inc(self->nestLevel);
	inc(tries);
	StxStart(self,&buf,&_ro);
	_stxfunc(self);
	StxCommit(self);
	dec(self->nestLevel);
	_lock->triesCommits = setTriesCommits(_lock->triesCommits,tries);
	exitCritical_1(_lock);
      }
    } else {
      self->nestLevel = -1;
      _rawfunc();
      self->nestLevel = 0;
      exitCritical_1(_lock);
    }
  } else {
    fprintf(stderr,"abort: file \"%s\", line %d, function \"%s\"\n",
	    __FILE__,__LINE__,__func__);
    abort();
  }
  return;
}
#endif
