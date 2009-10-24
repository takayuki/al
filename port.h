#ifndef _PORT_H
#define _PORT_H
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <inttypes.h>

intptr_t cmpxchg(volatile intptr_t*,intptr_t,intptr_t);
intptr_t fetch_and_add1(volatile intptr_t*);
intptr_t fetch_and_sub1(volatile intptr_t*);

#define CAS(var,old,new)	cmpxchg(&(var),(old),(new))
#define INC(var)		fetch_and_add1(&(var))
#define DEC(var)		fetch_and_sub1(&(var))
#define LOAD(var)		(var)
#define STORE(var,val)		((var)=(val))

#endif
