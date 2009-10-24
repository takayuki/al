/* large part of this file comes from linux kernel */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <inttypes.h>
#include <stdlib.h>
#include "port.h"

#if defined(__i386__) || defined(__x86_64__)
#ifdef __LP64__
intptr_t
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
intptr_t
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
intptr_t
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
intptr_t
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

static intptr_t
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

intptr_t
cmpxchg(volatile intptr_t* ptr,intptr_t old,intptr_t new)
{
  return __cmpxchg((ptr),(old),(new),sizeof(intptr_t));
}

intptr_t
fetch_and_add1(volatile intptr_t* ptr)
{
  volatile intptr_t cur = *ptr;
  while (cmpxchg(ptr,cur,cur+1) != cur)
    cur = *ptr;
  return cur;
}

intptr_t
fetch_and_sub1(volatile intptr_t* ptr)
{
  volatile intptr_t cur = *ptr;
  while (cmpxchg(ptr,cur,cur-1) != cur)
    cur = *ptr;
  return cur;
}
