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
#ifndef _PORT_H
#define _PORT_H
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <inttypes.h>

#define barrier()  __asm__ __volatile__ ("" : : : "memory")
#if defined(__i386__) || defined(__x86_64__)
#define mb()  __asm__ __volatile__ ("" : : : "memory")
#define rmb() __asm__ __volatile__ ("" : : : "memory")
#define wmb() __asm__ __volatile__ ("" : : : "memory")
#endif /* __i386__ */
#ifdef __sparc__
#define membar(type)					\
  do { __asm__ __volatile__("ba,pt    %%xcc,1f\n\t"	\
			    "membar   " type "\n"	\
			    "1:\n"			\
			    : : : "memory");		\
  } while (0)
#define mb()  membar("#LoadLoad | #LoadStore | #StoreStore | #StoreLoad")
#define rmb() membar("#LoadLoad")
#define wmb() membar("#StoreStore")
#endif /* __sparc__ */

intptr_t cmpxchg(volatile intptr_t*,intptr_t,intptr_t);
intptr_t fetch_and_add1(volatile intptr_t*);
intptr_t fetch_and_sub1(volatile intptr_t*);

#define CAS(var,old,new)  cmpxchg((volatile intptr_t*)&(var),(old),(new))
#define INC(var)          fetch_and_add1((volatile intptr_t*)&(var))
#define DEC(var)          fetch_and_sub1((volatile intptr_t*)&(var))
#define inc(var)          (((intptr_t)var)++)
#define dec(var)          (((intptr_t)var)--)

#endif
