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

#define CAS(var,old,new)  cmpxchg(&(var),(old),(new))
#define INC(var)          fetch_and_add1(&(var))
#define DEC(var)          fetch_and_sub1(&(var))
#define inc(var)          (((volatile intptr_t)var)++)
#define dec(var)          (((volatile intptr_t)var)--)

#endif
