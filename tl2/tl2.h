/* =============================================================================
 *
 * tl2.h
 *
 * Transactional Locking 2 software transactional memory
 *
 * =============================================================================
 *
 * Copyright (C) Sun Microsystems Inc., 2006.  All Rights Reserved.
 * Authors: Dave Dice, Nir Shavit, Ori Shalev.
 *
 * TL2: Transactional Locking for Disjoint Access Parallelism
 *
 * Transactional Locking II,
 * Dave Dice, Ori Shalev, Nir Shavit
 * DISC 2006, Sept 2006, Stockholm, Sweden.
 *
 * =============================================================================
 *
 * Modified by Chi Cao Minh (caominh@stanford.edu)
 *
 * See VERSIONS for revision history
 *
 * =============================================================================
 */

#ifndef TL2_H
#define TL2_H 1
#include <inttypes.h>
#include "tmalloc.h"


#  include <setjmp.h>







typedef int BitMap;
typedef uintptr_t vwLock; /* (Version,LOCKBIT) */
typedef unsigned char byte;

/* Read set and write-set log entry */
typedef struct _AVPair {
    struct _AVPair* Next;
    struct _AVPair* Prev;
    volatile intptr_t* Addr;
    intptr_t Valu;
    volatile vwLock* LockFor; /* points to the vwLock covering Addr */
    vwLock rdv; /* read-version @ time of 1st read - observed */
    byte Held;
    struct _Thread* Owner;
    long Ordinal;
} AVPair;

typedef struct _Log {
    AVPair* List;
    AVPair* put; /* Insert position - cursor */
    AVPair* tail; /* CCM: Pointer to last entry */
    long ovf; /* Overflow - request to grow */
#ifndef TL2_OPTIM_HASHLOG
    BitMap BloomFilter; /* Address exclusion fast-path test */
#endif
} Log;

#ifdef TL2_OPTIM_HASHLOG
typedef struct _HashLog {
    Log* logs;
    long numLog;
    long numEntry;
    BitMap BloomFilter; /* Address exclusion fast-path test */
} HashLog;
#endif

typedef struct _Thread {
    long UniqID;
    volatile long Mode;
    volatile long HoldsLocks; /* passed start of update */
    volatile long Retries;



    volatile vwLock rv;
    vwLock wv;
    vwLock abv;
    int* ROFlag;
    int IsRO;
    long Starts;
    long Aborts; /* Tally of # of aborts */
    unsigned long long rng;
    unsigned long long xorrng [1];
    void* memCache;

    tmalloc_t* allocPtr; /* CCM: speculatively allocated */
    tmalloc_t* freePtr; /* CCM: speculatively free'd */


    Log rdSet;

#ifdef TL2_OPTIM_HASHLOG
    HashLog wrSet;
#else
    Log wrSet;
#endif
    Log LocalUndo;

    sigjmp_buf* envPtr;




#ifdef TL2_STATS
    long stats[12];
    long TxST;
    long TxLD;
#endif /* TL2_STATS */
} Thread;


#ifdef __cplusplus
extern "C" {
#endif






#  include <setjmp.h>
#  define SIGSETJMP(env, savesigs)      sigsetjmp(env, savesigs)
#  define SIGLONGJMP(env, val)          siglongjmp(env, val); assert(0)



/*
 * Prototypes
 */




void TxStart (Thread*, sigjmp_buf*, int*);

Thread* TxNewThread ();





void TxFreeThread (Thread*);
void TxInitThread (Thread*, long id);
int TxCommit (Thread*);
void TxAbort (Thread*);
intptr_t TxLoad (Thread*, volatile intptr_t*);
void TxStore (Thread*, volatile intptr_t*, intptr_t);
void TxStoreLocal (Thread*, volatile intptr_t*, intptr_t);
void TxOnce ();
void TxShutdown ();

void* TxAlloc (Thread*, size_t);
void TxFree (Thread*, void*);







#ifdef __cplusplus
}
#endif


#endif /* TL2_H */


/* =============================================================================
 *
 * End of tl2.h
 *
 * =============================================================================
 */
