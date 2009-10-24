/* =============================================================================
 *
 * client.c
 *
 * =============================================================================
 *
 * Copyright (C) 2008, University of Oregon.  All Rights Reserved.
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
#include "alx.h"
#include "action.h"
#include "client.h"
#include "manager.h"
#include "reservation.h"
#include "thread.h"
#include "types.h"


/* =============================================================================
 * client_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
client_t*
client_alloc (long id,
              manager_t* managerPtr,
              long numOperation,
              long numQueryPerTransaction,
              long queryRange,
              long percentUser)
{
    client_t* clientPtr;

    clientPtr = (client_t*)malloc(sizeof(client_t));
    if (clientPtr == NULL) {
        return NULL;
    }

    clientPtr->randomPtr = random_alloc();
    if (clientPtr->randomPtr == NULL) {
        return NULL;
    }

    clientPtr->id = id;
    clientPtr->managerPtr = managerPtr;
    random_seed(clientPtr->randomPtr, id);
    clientPtr->numOperation = numOperation;
    clientPtr->numQueryPerTransaction = numQueryPerTransaction;
    clientPtr->queryRange = queryRange;
    clientPtr->percentUser = percentUser;

    return clientPtr;
}


/* =============================================================================
 * client_free
 * =============================================================================
 */
void
client_free (client_t* clientPtr)
{
    free(clientPtr);
}


/* =============================================================================
 * selectAction
 * =============================================================================
 */
static action_t
selectAction (long r, long percentUser)
{
    action_t action;

    if (r < percentUser) {
        action = ACTION_MAKE_RESERVATION;
    } else if (r & 1) {
        action = ACTION_DELETE_CUSTOMER;
    } else {
        action = ACTION_UPDATE_TABLES;
    }

    return action;
}


/* =============================================================================
 * client_run
 * -- Execute list operations on the database
 * =============================================================================
 */
#define MAXQUERYPERTRANSACTION 8

al_t clientLock = AL_INITIALIZER("clientLock");

__attribute__((atomic))
static void
atomic1(al_t* lock,
	manager_t* managerPtr, random_t* randomPtr,
	long queryRange, long numQuery, long customerId)
{
    long types[MAXQUERYPERTRANSACTION];
    long ids[MAXQUERYPERTRANSACTION];
    long maxPrices[NUM_RESERVATION_TYPE] = { -1, -1, -1 };
    long maxIds[NUM_RESERVATION_TYPE] = { -1, -1, -1 };
    long n;
    bool_t isFound = FALSE;

    for (n = 0; n < numQuery; n++) {
	types[n] = random_generate(randomPtr) % NUM_RESERVATION_TYPE;
	ids[n] = (random_generate(randomPtr) % queryRange) + 1;
    }
    TM_BEGIN();
    for (n = 0; n < numQuery; n++) {
	long t = types[n];
	long id = ids[n];
	long price = -1;
	switch (t) {
	case RESERVATION_CAR:
	    if (MANAGER_QUERY_CAR(lock, managerPtr, id) >= 0) {
		price = MANAGER_QUERY_CAR_PRICE(lock, managerPtr, id);
	    }
	    break;
	case RESERVATION_FLIGHT:
	    if (MANAGER_QUERY_FLIGHT(lock, managerPtr, id) >= 0) {
		price = MANAGER_QUERY_FLIGHT_PRICE(lock, managerPtr, id);
	    }
	    break;
	case RESERVATION_ROOM:
	    if (MANAGER_QUERY_ROOM(lock, managerPtr, id) >= 0) {
		price = MANAGER_QUERY_ROOM_PRICE(lock, managerPtr, id);
	    }
	    break;
	default:
	    assert(0);
	}
	if (price > maxPrices[t]) {
	    maxPrices[t] = price;
	    maxIds[t] = id;
	    isFound = TRUE;
	}
    } /* for n */
    if (isFound) {
	MANAGER_ADD_CUSTOMER(lock, managerPtr, customerId);
    }
    if (maxIds[RESERVATION_CAR] > 0) {
	MANAGER_RESERVE_CAR(lock, managerPtr,
			    customerId, maxIds[RESERVATION_CAR]);
    }
    if (maxIds[RESERVATION_FLIGHT] > 0) {
	MANAGER_RESERVE_FLIGHT(lock, managerPtr,
			       customerId, maxIds[RESERVATION_FLIGHT]);
    }
    if (maxIds[RESERVATION_ROOM] > 0) {
	MANAGER_RESERVE_ROOM(lock, managerPtr,
			     customerId, maxIds[RESERVATION_ROOM]);
    }
    TM_END();
}

__attribute__((atomic))
static void
atomic2(al_t* lock, manager_t* managerPtr, long customerId)
{
    long bill = MANAGER_QUERY_CUSTOMER_BILL(lock, managerPtr, customerId);
    if (bill >= 0) {
	MANAGER_DELETE_CUSTOMER(lock, managerPtr, customerId);
    }
}

__attribute__((atomic))
static void
atomic3(al_t* lock,
	manager_t* managerPtr, random_t* randomPtr,
	long queryRange, long numUpdate)
{
    long types[MAXQUERYPERTRANSACTION];
    long ids[MAXQUERYPERTRANSACTION];
    long ops[MAXQUERYPERTRANSACTION];
    long prices[MAXQUERYPERTRANSACTION];
    long n;

    for (n = 0; n < numUpdate; n++) {
	types[n] = random_generate(randomPtr) % NUM_RESERVATION_TYPE;
	ids[n] = (random_generate(randomPtr) % queryRange) + 1;
	ops[n] = random_generate(randomPtr) % 2;
	if (ops[n]) {
	    prices[n] = ((random_generate(randomPtr) % 5) * 10) + 50;
	}
    }
    TM_BEGIN();
    for (n = 0; n < numUpdate; n++) {
	long t = types[n];
	long id = ids[n];
	long doAdd = ops[n];
	if (doAdd) {
	    long newPrice = prices[n];
	    switch (t) {
	    case RESERVATION_CAR:
		MANAGER_ADD_CAR(lock, managerPtr, id, 100, newPrice);
		break;
	    case RESERVATION_FLIGHT:
		MANAGER_ADD_FLIGHT(lock, managerPtr, id, 100, newPrice);
		break;
	    case RESERVATION_ROOM:
		MANAGER_ADD_ROOM(lock, managerPtr, id, 100, newPrice);
		break;
	    default:
		assert(0);
	    }
	} else { /* do delete */
	    switch (t) {
	    case RESERVATION_CAR:
		MANAGER_DELETE_CAR(lock, managerPtr, id, 100);
		break;
	    case RESERVATION_FLIGHT:
		MANAGER_DELETE_FLIGHT(lock, managerPtr, id);
		break;
	    case RESERVATION_ROOM:
		MANAGER_DELETE_ROOM(lock, managerPtr, id, 100);
		break;
	    default:
		assert(0);
	    }
	}
    }
    TM_END();
}

void
client_run (void* argPtr)
{
    TM_THREAD_ENTER();

    long myId = thread_getId();
    client_t* clientPtr = ((client_t**)argPtr)[myId];
    manager_t* managerPtr = clientPtr->managerPtr;
    random_t*  randomPtr  = clientPtr->randomPtr;

    long numOperation           = clientPtr->numOperation;
    long numQueryPerTransaction = clientPtr->numQueryPerTransaction;
    long queryRange             = clientPtr->queryRange;
    long percentUser            = clientPtr->percentUser;
    long i;



    if (MAXQUERYPERTRANSACTION <= numQueryPerTransaction)
	numQueryPerTransaction = MAXQUERYPERTRANSACTION;
    for (i = 0; i < numOperation; i++) {
        long r = random_generate(randomPtr) % 100;
        action_t action = selectAction(r, percentUser);

        switch (action) {

	case ACTION_MAKE_RESERVATION: {
	    long numQuery = random_generate(randomPtr) % numQueryPerTransaction + 1;
	    long customerId = random_generate(randomPtr) % queryRange + 1;
	    atomic1(&clientLock, managerPtr, randomPtr,
		    queryRange, numQuery, customerId);
	    break;
	}

	case ACTION_DELETE_CUSTOMER: {
	    long customerId = random_generate(randomPtr) % queryRange + 1;
	    atomic2(&clientLock, managerPtr, customerId);
	    break;
	}

	case ACTION_UPDATE_TABLES: {
	    long numUpdate = random_generate(randomPtr) % numQueryPerTransaction + 1;
	    atomic3(&clientLock,
		    managerPtr, randomPtr,
		    queryRange, numUpdate);
	    break;
	}

	default:
	    assert(0);
        } /* switch (action) */
	
    } /* for i */

    TM_THREAD_EXIT();
}


/* =============================================================================
 *
 * End of client.c
 *
 * =============================================================================
 */




