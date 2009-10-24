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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <inttypes.h>
#include <stdlib.h>
#include "port.h"

#if defined(__i386__) || defined(__x86_64__)
#ifdef __LP64__
__inline__ intptr_t
__cmpxchg_u64(volatile intptr_t* ptr,intptr_t old,intptr_t new)
{
  intptr_t prev;
   __asm__ __volatile__("lock; cmpxchgq %1,%2"
			: "=a"(prev)
			: "r"(new), "m"(*ptr), "0"(old)
			: "memory");
   return prev;
}
#else
__inline__ intptr_t
__cmpxchg_u32(volatile intptr_t* ptr,intptr_t old,intptr_t new)
{
  intptr_t prev;
  __asm__ __volatile__("lock; cmpxchgl %1,%2"
		       : "=a"(prev)
		       : "r"(new), "m"(*ptr), "0"(old)
		       : "memory");
  return prev;
}
#endif /* __LP64__ */
#endif /* __i386__ */
#ifdef __sparc__
#ifdef __LP64__
__inline__ intptr_t
__cmpxchg_u64(volatile intptr_t* ptr,intptr_t old,intptr_t new)
{
  __asm__ __volatile__("membar #StoreLoad | #LoadLoad\n"
		       "casx [%2], %3, %0\n\t"
		       "membar #StoreLoad | #StoreStore"
		       : "=&r" (new)
		       : "0" (new), "r" (ptr), "r" (old)
		       : "memory");
  return new;
}
#else
__inline__ intptr_t
__cmpxchg_u32(volatile intptr_t* ptr,intptr_t old,intptr_t new)
{
  __asm__ __volatile__("membar #StoreLoad | #LoadLoad\n"
		       "cas [%2], %3, %0\n\t"
		       "membar #StoreLoad | #StoreStore"
		       : "=&r" (new)
		       : "0" (new), "r" (ptr), "r" (old)
		       : "memory");
  return new;
}
#endif /* __LP64__ */
#endif /* __sparc__ */

__inline__ static intptr_t
__cmpxchg(volatile intptr_t* ptr,intptr_t old,intptr_t new,int size)
{
  switch (size) {
#ifndef __LP64__
  case 4:
    return __cmpxchg_u32(ptr,old,new);
#else
  case 8:
    return __cmpxchg_u64(ptr,old,new);
#endif
  }
  abort();
  return old;
}

__inline__ intptr_t
cmpxchg(volatile intptr_t* ptr,intptr_t old,intptr_t new)
{
  return __cmpxchg((ptr),(old),(new),sizeof(intptr_t));
}

__inline__ intptr_t
fetch_and_add1(volatile intptr_t* ptr)
{
  volatile intptr_t cur = *ptr;
  while (cmpxchg(ptr,cur,cur+1) != cur)
    cur = *ptr;
  return cur;
}

__inline__ intptr_t
fetch_and_sub1(volatile intptr_t* ptr)
{
  volatile intptr_t cur = *ptr;
  while (cmpxchg(ptr,cur,cur-1) != cur)
    cur = *ptr;
  return cur;
}
