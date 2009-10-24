/* =============================================================================
 *
 * reservation.c
 * -- Representation of car, flight, and hotel relations
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * For the license of bayes/sort.h and bayes/sort.c, please see the header
 * of the files.
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of kmeans, please see kmeans/LICENSE.kmeans
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/mt19937ar.c and lib/mt19937ar.h, please see the
 * header of the files.
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/rbtree.h and lib/rbtree.c, please see
 * lib/LEGALNOTICE.rbtree and lib/LICENSE.rbtree
 * 
 * ------------------------------------------------------------------------
 * 
 * Unless otherwise noted, the following license applies to STAMP files:
 * 
 * Copyright (c) 2007, Stanford University
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 * 
 *     * Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
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
 *
 * =============================================================================
 */


#include <assert.h>
#include <stdlib.h>
#include "alx.h"
#include "memory.h"
#include "reservation.h"
#include "tm.h"
#include "types.h"


/* =============================================================================
 * reservation_info_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
__attribute__((atomic))
reservation_info_t*
reservation_info_alloc (al_t* lock, reservation_type_t type, long id, long price)
{
    reservation_info_t* reservationInfoPtr;

    reservationInfoPtr = (reservation_info_t*)TM_MALLOC(sizeof(reservation_info_t));
    if (reservationInfoPtr != NULL) {
	LocalStore(&reservationInfoPtr->type, type);
        LocalStore(&reservationInfoPtr->id, id);
        LocalStore(&reservationInfoPtr->price, price);
    }

    return reservationInfoPtr;
}


/* =============================================================================
 * reservation_info_free
 * =============================================================================
 */
__attribute__((atomic))
void
reservation_info_free (al_t* lock, reservation_info_t* reservationInfoPtr)
{
    TM_FREE(reservationInfoPtr);
}


/* =============================================================================
 * reservation_info_compare
 * -- Returns -1 if A < B, 0 if A = B, 1 if A > B
 * =============================================================================
 */
long
reservation_info_compare (al_t* lock, reservation_info_t* aPtr, reservation_info_t* bPtr)
{
    long typeDiff;

    typeDiff = aPtr->type - bPtr->type;

    return ((typeDiff != 0) ? (typeDiff) : (aPtr->id - bPtr->id));
}


/* =============================================================================
 * checkReservation
 * -- Check if consistent
 * =============================================================================
 */
__attribute__((atomic))
static void
checkReservation (al_t* lock, reservation_t* reservationPtr)
{
    long numUsed = (long)TM_SHARED_READ(reservationPtr->numUsed);
    if (numUsed < 0) {
        TM_RESTART();
    }
    
    long numFree = (long)TM_SHARED_READ(reservationPtr->numFree);
    if (numFree < 0) {
        TM_RESTART();
    }

    long numTotal = (long)TM_SHARED_READ(reservationPtr->numTotal);
    if (numTotal < 0) {
        TM_RESTART();
    }

    if ((numUsed + numFree) != numTotal) {
        TM_RESTART();
    }

    long price = (long)TM_SHARED_READ(reservationPtr->price);
    if (price < 0) {
        TM_RESTART();
    }
}

#define CHECK_RESERVATION(lock, reservation)	\
    checkReservation(lock, reservation)


static void
checkReservation_seq (reservation_t* reservationPtr)
{
    assert(reservationPtr->numUsed >= 0);
    assert(reservationPtr->numFree >= 0);
    assert(reservationPtr->numTotal >= 0);
    assert((reservationPtr->numUsed + reservationPtr->numFree) ==
           (reservationPtr->numTotal));
    assert(reservationPtr->price >= 0);
}


/* =============================================================================
 * reservation_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
__attribute__((atomic))
reservation_t*
reservation_alloc (al_t* lock, long id, long numTotal, long price)
{
    reservation_t* reservationPtr;

    reservationPtr = (reservation_t*)TM_MALLOC(sizeof(reservation_t));
    if (reservationPtr != NULL) {
        LocalStore(&reservationPtr->id,id);
        LocalStore(&reservationPtr->numUsed, 0);
        LocalStore(&reservationPtr->numFree, numTotal);
        LocalStore(&reservationPtr->numTotal, numTotal);
        LocalStore(&reservationPtr->price, price);
        CHECK_RESERVATION(lock, reservationPtr);
    }

    return reservationPtr;
}


reservation_t*
reservation_alloc_seq (long id, long numTotal, long price)
{
    reservation_t* reservationPtr;

    reservationPtr = (reservation_t*)P_MALLOC(sizeof(reservation_t));
    if (reservationPtr != NULL) {
        reservationPtr->id = id;
        reservationPtr->numUsed = 0;
        reservationPtr->numFree = numTotal;
        reservationPtr->numTotal = numTotal;
        reservationPtr->price = price;
        checkReservation_seq(reservationPtr);
    }

    return reservationPtr;
}


/* =============================================================================
 * reservation_addToTotal
 * -- Adds if 'num' > 0, removes if 'num' < 0;
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((atomic))
bool_t
reservation_addToTotal (al_t* lock, reservation_t* reservationPtr, long num)
{
    long numFree = (long)TM_SHARED_READ(reservationPtr->numFree);

    if (numFree + num < 0) {
        return FALSE;
    }

    TM_SHARED_WRITE(reservationPtr->numFree, (numFree + num));
    TM_SHARED_WRITE(reservationPtr->numTotal,
                    ((long)TM_SHARED_READ(reservationPtr->numTotal) + num));

    CHECK_RESERVATION(lock, reservationPtr);

    return TRUE;
}


bool_t
reservation_addToTotal_seq (reservation_t* reservationPtr, long num)
{
    if (reservationPtr->numFree + num < 0) {
        return FALSE;
    }

    reservationPtr->numFree += num;
    reservationPtr->numTotal += num;

    checkReservation_seq(reservationPtr);

    return TRUE;
}


/* =============================================================================
 * reservation_make
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((atomic))
bool_t
reservation_make (al_t* lock, reservation_t* reservationPtr)
{
    long numFree = (long)TM_SHARED_READ(reservationPtr->numFree);

    if (numFree < 1) {
        return FALSE;
    }
    TM_SHARED_WRITE(reservationPtr->numUsed,
                    ((long)TM_SHARED_READ(reservationPtr->numUsed) + 1));
    TM_SHARED_WRITE(reservationPtr->numFree, (numFree - 1));

    CHECK_RESERVATION(lock, reservationPtr);

    return TRUE;
}


bool_t
reservation_make_seq (reservation_t* reservationPtr)
{
    if (reservationPtr->numFree < 1) {
        return FALSE;
    }

    reservationPtr->numUsed++;
    reservationPtr->numFree--;

    checkReservation_seq(reservationPtr);

    return TRUE;
}


/* =============================================================================
 * reservation_cancel
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((atomic))
bool_t
reservation_cancel (al_t* lock, reservation_t* reservationPtr)
{
    long numUsed = (long)TM_SHARED_READ(reservationPtr->numUsed);

    if (numUsed < 1) {
        return FALSE;
    }

    TM_SHARED_WRITE(reservationPtr->numUsed, (numUsed - 1));
    TM_SHARED_WRITE(reservationPtr->numFree,
                    ((long)TM_SHARED_READ(reservationPtr->numFree) + 1));

    CHECK_RESERVATION(lock, reservationPtr);

    return TRUE;
}


bool_t
reservation_cancel_seq (reservation_t* reservationPtr)
{
    if (reservationPtr->numUsed < 1) {
        return FALSE;
    }

    reservationPtr->numUsed--;
    reservationPtr->numFree++;

    checkReservation_seq(reservationPtr);

    return TRUE;
}


/* =============================================================================
 * reservation_updatePrice
 * -- Failure if 'price' < 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((atomic))
bool_t
reservation_updatePrice (al_t* lock, reservation_t* reservationPtr, long newPrice)
{
    if (newPrice < 0) {
        return FALSE;
    }

    TM_SHARED_WRITE(reservationPtr->price, newPrice);

    CHECK_RESERVATION(lock, reservationPtr);

    return TRUE;
}


bool_t
reservation_updatePrice_seq (reservation_t* reservationPtr, long newPrice)
{
    if (newPrice < 0) {
        return FALSE;
    }

    reservationPtr->price = newPrice;

    checkReservation_seq(reservationPtr);

    return TRUE;
}


/* =============================================================================
 * reservation_compare
 * -- Returns -1 if A < B, 0 if A = B, 1 if A > B
 * =============================================================================
 */
long
reservation_compare (al_t* lock, reservation_t* aPtr, reservation_t* bPtr)
{
    return (aPtr->id - bPtr->id);
}


/* =============================================================================
 * reservation_hash
 * =============================================================================
 */
ulong_t
reservation_hash (reservation_t* reservationPtr)
{
    /* Separate tables for cars, flights, etc, so no need to use 'type' */
    return (ulong_t)reservationPtr->id;
}


/* =============================================================================
 * reservation_free
 * =============================================================================
 */
__attribute__((atomic))
void
reservation_free (al_t* lock, reservation_t* reservationPtr)
{
    TM_FREE(reservationPtr);
}


/* =============================================================================
 * TEST_RESERVATION
 * =============================================================================
 */
#ifdef TEST_RESERVATION


#include <assert.h>
#include <stdio.h>

__attribute__((atomic))
void*
test (void* arg)
{
    al_t* lock = (al_t*)arg;
    reservation_info_t* reservationInfo1Ptr;
    reservation_info_t* reservationInfo2Ptr;
    reservation_info_t* reservationInfo3Ptr;

    reservation_t* reservation1Ptr;
    reservation_t* reservation2Ptr;
    reservation_t* reservation3Ptr;

    assert(memory_init(1, 4, 2));

    puts("Starting...");

    reservationInfo1Ptr = reservation_info_alloc(lock, 0, 0, 0);
    reservationInfo2Ptr = reservation_info_alloc(lock, 0, 0, 1);
    reservationInfo3Ptr = reservation_info_alloc(lock, 2, 0, 1);

    /* Test compare */
    assert(reservation_info_compare(lock, reservationInfo1Ptr, reservationInfo2Ptr) == 0);
    assert(reservation_info_compare(lock, reservationInfo1Ptr, reservationInfo3Ptr) < 0);
    assert(reservation_info_compare(lock, reservationInfo2Ptr, reservationInfo3Ptr) < 0);

    reservation1Ptr = reservation_alloc(lock, 0, 0, 0);
    reservation2Ptr = reservation_alloc(lock, 0, 0, 1);
    reservation3Ptr = reservation_alloc(lock, 2, 0, 1);

    /* Test compare */
    assert(reservation_compare(lock, reservation1Ptr, reservation2Ptr) == 0);
    assert(reservation_compare(lock, reservation1Ptr, reservation3Ptr) != 0);
    assert(reservation_compare(lock, reservation2Ptr, reservation3Ptr) != 0);

    /* Cannot reserve if total is 0 */
    assert(!reservation_make(lock, reservation1Ptr));

    /* Cannot cancel if used is 0 */
    assert(!reservation_cancel(lock, reservation1Ptr));

    /* Cannot update with negative price */
    assert(!reservation_updatePrice(lock, reservation1Ptr, -1));

    /* Cannot make negative total */
    assert(!reservation_addToTotal(lock, reservation1Ptr, -1));

    /* Update total and price */
    assert(reservation_addToTotal(lock, reservation1Ptr, 1));
    assert(reservation_updatePrice(lock, reservation1Ptr, 1));
    assert(reservation1Ptr->numUsed == 0);
    assert(reservation1Ptr->numFree == 1);
    assert(reservation1Ptr->numTotal == 1);
    assert(reservation1Ptr->price == 1);
    checkReservation(lock, reservation1Ptr);

    /* Make and cancel reservation */
    assert(reservation_make(lock, reservation1Ptr));
    assert(reservation_cancel(lock, reservation1Ptr));
    assert(!reservation_cancel(lock, reservation1Ptr));

    reservation_info_free(lock, reservationInfo1Ptr);
    reservation_info_free(lock, reservationInfo2Ptr);
    reservation_info_free(lock, reservationInfo3Ptr);

    reservation_free(lock, reservation1Ptr);
    reservation_free(lock, reservation2Ptr);
    reservation_free(lock, reservation3Ptr);

    puts("All tests passed.");

    return 0;
}

int
main ()
{
    pthread_t t;
    al_t lock = AL_INITIALIZER;

    setAdaptMode(1);
    pthread_create(&t,0,test,&lock);
    pthread_join(t,0);
    return 0;
}

#endif /* TEST_RESERVATION */


/* =============================================================================
 *
 * End of reservation.c
 *
 * =============================================================================
 */
