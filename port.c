/* large part of this file comes from linux kernel */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <inttypes.h>
#include <stdlib.h>
#include "port.h"

#if defined(__i386__) || defined(__x86_64__)
#define mb()  __asm__ __volatile__ ("lock; addl $0,0(%%esp)" : : : "memory")
#define rmb() __asm__ __volatile__ ("lock; addl $0,0(%%esp)" : : : "memory")
#define wmb() __asm__ __volatile__ ("" : : : "memory")

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
#define membar_safe(type)				\
  do { __asm__ __volatile__("ba,pt    %%xcc,1f\n\t"	\
			    "membar   " type "\n"	\
			    "1:\n"			\
			    : : : "memory");		\
  } while (0)
#define mb()  membar("#LoadLoad | #LoadStore | #StoreStore | #StoreLoad")
#define rmb() membar("#LoadLoad")
#define wmb() membar("#StoreStore")

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
