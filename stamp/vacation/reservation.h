/* =============================================================================
 *
 * reservation.h
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


#ifndef RESERVATION_H
#define RESERVATION_H 1


#include "tm.h"
#include "types.h"

typedef enum reservation_type {
    RESERVATION_CAR,
    RESERVATION_FLIGHT,
    RESERVATION_ROOM,
    NUM_RESERVATION_TYPE
} reservation_type_t;

typedef struct reservation_info {
    reservation_type_t type;
    long id;
    long price; /* holds price at time reservation was made */
} reservation_info_t;

typedef struct reservation {
    long id;
    long numUsed;
    long numFree;
    long numTotal;
    long price;
} reservation_t;


/* =============================================================================
 * reservation_info_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
reservation_info_t*
reservation_info_alloc (al_t* lock, reservation_type_t type, long id, long price);


/* =============================================================================
 * reservation_info_free
 * =============================================================================
 */
void
reservation_info_free (al_t* lock, reservation_info_t* reservationInfoPtr);


/* =============================================================================
 * reservation_info_compare
 * -- Returns -1 if A < B, 0 if A = B, 1 if A > B
 * =============================================================================
 */
long
reservation_info_compare (al_t* lock, reservation_info_t* aPtr, reservation_info_t* bPtr);


/* =============================================================================
 * reservation_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
reservation_t*
reservation_alloc (al_t* lock, long id, long price, long numTotal);

reservation_t*
reservation_alloc_seq (long id, long price, long numTotal);


/* =============================================================================
 * reservation_addToTotal
 * -- Adds if 'num' > 0, removes if 'num' < 0;
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool_t
reservation_addToTotal (al_t* lock, reservation_t* reservationPtr, long num);

bool_t
reservation_addToTotal_seq (reservation_t* reservationPtr, long num);


/* =============================================================================
 * reservation_make
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool_t
reservation_make (al_t* lock, reservation_t* reservationPtr);

bool_t
reservation_make_seq (reservation_t* reservationPtr);


/* =============================================================================
 * reservation_cancel
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool_t
reservation_cancel (al_t* lock, reservation_t* reservationPtr);

bool_t
reservation_cancel_seq (reservation_t* reservationPtr);


/* =============================================================================
 * reservation_updatePrice
 * -- Failure if 'price' < 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool_t
reservation_updatePrice (al_t* lock, reservation_t* reservationPtr, long newPrice);

bool_t
reservation_updatePrice_seq (reservation_t* reservationPtr, long newPrice);


/* =============================================================================
 * reservation_compare
 * -- Returns -1 if A < B, 0 if A = B, 1 if A > B
 * =============================================================================
 */
long
reservation_compare (al_t* lock, reservation_t* aPtr, reservation_t* bPtr);


/* =============================================================================
 * reservation_hash
 * =============================================================================
 */
ulong_t
reservation_hash (reservation_t* reservationPtr);


/* =============================================================================
 * reservation_free
 * =============================================================================
 */
void
reservation_free (al_t* lock, reservation_t* reservationPtr);


#define RESERVATION_INFO_ALLOC(lock, type, id, price)	\
    reservation_info_alloc(lock, type, id, price)
#define RESERVATION_INFO_FREE(lock, r)		\
    reservation_info_free(lock, r)

#define RESERVATION_ALLOC(lock, id, price, tot)	\
    reservation_alloc(lock, id, price, tot)
#define RESERVATION_ADD_TO_TOTAL(lock, r, num)	\
    reservation_addToTotal(lock, r, num)
#define RESERVATION_MAKE(lock, r)		\
    reservation_make(lock, r)
#define RESERVATION_CANCEL(lock, r)		\
    reservation_cancel(lock, r)
#define RESERVATION_UPDATE_PRICE(lock, r, price) \
    reservation_updatePrice(lock, r, price)
#define RESERVATION_FREE(lock, r)		\
    reservation_free(lock, r)


#endif /* RESERVATION_H */


/* =============================================================================
 *
 * End of reservation.h
 *
 * =============================================================================
 */
