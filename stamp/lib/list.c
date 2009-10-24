/* =============================================================================
 *
 * list.c
 * -- Sorted singly linked list
 * -- Options: -DLIST_NO_DUPLICATES (default: allow duplicates)
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


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "alx.h"
#include "list.h"
#include "types.h"
#include "tm.h"


/* =============================================================================
 * compareDataPtrAddresses
 * -- Default compare function
 * =============================================================================
 */
static long
compareDataPtrAddresses (al_t* lock, const void* a, const void* b)
{
    return ((long)a - (long)b);
}


/* =============================================================================
 * list_iter_reset
 * =============================================================================
 */
void
list_iter_reset (list_iter_t* itPtr, list_t* listPtr)
{
    *itPtr = &(listPtr->head);
}


/* =============================================================================
 * TMlist_iter_reset
 * =============================================================================
 */
__attribute__((atomic))
void
TMlist_iter_reset (al_t* lock, list_iter_t* itPtr, list_t* listPtr)
{
    LocalStore(itPtr, &(listPtr->head));
}


/* =============================================================================
 * list_iter_hasNext
 * =============================================================================
 */
bool_t
list_iter_hasNext (list_iter_t* itPtr, list_t* listPtr)
{
    return (((*itPtr)->nextPtr != NULL) ? TRUE : FALSE);
}


/* =============================================================================
 * TMlist_iter_hasNext
 * =============================================================================
 */
__attribute__((atomic))
bool_t
TMlist_iter_hasNext (al_t* lock, list_iter_t* itPtr, list_t* listPtr)
{
    list_node_t* nodePtr = (list_node_t*)LocalLoad(itPtr);
    list_iter_t next = (list_iter_t)TM_SHARED_READ_P(nodePtr->nextPtr);

    return ((next != NULL) ? TRUE : FALSE);
}


/* =============================================================================
 * list_iter_next
 * =============================================================================
 */
void*
list_iter_next (list_iter_t* itPtr, list_t* listPtr)
{
    *itPtr = (*itPtr)->nextPtr;

    return (*itPtr)->dataPtr;
}


/* =============================================================================
 * TMlist_iter_next
 * =============================================================================
 */
__attribute__((atomic))
void*
TMlist_iter_next (al_t* lock, list_iter_t* itPtr, list_t* listPtr)
{
    list_node_t* nodePtr = (list_node_t*)LocalLoad(itPtr);
    list_iter_t next = (list_iter_t)TM_SHARED_READ_P(nodePtr->nextPtr);
    LocalStore(itPtr,next);

    return LocalLoad(&next->dataPtr);
}


/* =============================================================================
 * allocNode
 * -- Returns NULL on failure
 * =============================================================================
 */
static list_node_t*
allocNode (void* dataPtr)
{
    list_node_t* nodePtr = (list_node_t*)P_MALLOC(sizeof(list_node_t));
    if (nodePtr == NULL) {
        return NULL;
    }

    nodePtr->dataPtr = dataPtr;
    nodePtr->nextPtr = NULL;

    return nodePtr;
}


/* =============================================================================
 * PallocNode
 * -- Returns NULL on failure
 * =============================================================================
 */
static list_node_t*
PallocNode (void* dataPtr)
{
    list_node_t* nodePtr = (list_node_t*)P_MALLOC(sizeof(list_node_t));
    if (nodePtr == NULL) {
        return NULL;
    }

    nodePtr->dataPtr = dataPtr;
    nodePtr->nextPtr = NULL;

    return nodePtr;
}


/* =============================================================================
 * TMallocNode
 * -- Returns NULL on failure
 * =============================================================================
 */
__attribute__((atomic))
static list_node_t*
TMallocNode (al_t* lock, void* dataPtr)
{
    list_node_t* nodePtr = (list_node_t*)TM_MALLOC(sizeof(list_node_t));
    if (nodePtr == NULL) {
        return NULL;
    }

    LocalStore(&nodePtr->dataPtr,dataPtr);
    LocalStore(&nodePtr->nextPtr,NULL);

    return nodePtr;
}


/* =============================================================================
 * list_alloc
 * -- If NULL passed for 'compare' function, will compare data pointer addresses
 * -- Returns NULL on failure
 * =============================================================================
 */
list_t*
list_alloc (long (*compare)(al_t*, const void*, const void*))
{
    list_t* listPtr = (list_t*)P_MALLOC(sizeof(list_t));
    if (listPtr == NULL) {
        return NULL;
    }

    listPtr->head.dataPtr = NULL;
    listPtr->head.nextPtr = NULL;
    listPtr->size = 0;

    if (compare == NULL) {
        listPtr->compare = &compareDataPtrAddresses; /* default */
    } else {
        listPtr->compare = compare;
    }

    return listPtr;
}


/* =============================================================================
 * Plist_alloc
 * -- If NULL passed for 'compare' function, will compare data pointer addresses
 * -- Returns NULL on failure
 * =============================================================================
 */
list_t*
Plist_alloc (long (*compare)(al_t*, const void*, const void*))
{
    list_t* listPtr = (list_t*)P_MALLOC(sizeof(list_t));
    if (listPtr == NULL) {
        return NULL;
    }

    listPtr->head.dataPtr = NULL;
    listPtr->head.nextPtr = NULL;
    listPtr->size = 0;

    if (compare == NULL) {
        listPtr->compare = &compareDataPtrAddresses; /* default */
    } else {
        listPtr->compare = compare;
    }

    return listPtr;
}


/* =============================================================================
 * TMlist_alloc
 * -- If NULL passed for 'compare' function, will compare data pointer addresses
 * -- Returns NULL on failure
 * =============================================================================
 */
__attribute__((atomic))
list_t*
TMlist_alloc (al_t* lock, long (*compare)(al_t*, const void*, const void*))
{
    list_t* listPtr = (list_t*)TM_MALLOC(sizeof(list_t));
    if (listPtr == NULL) {
        return NULL;
    }

    LocalStore(&listPtr->head.dataPtr, NULL);
    LocalStore(&listPtr->head.nextPtr, NULL);
    LocalStore(&listPtr->size, 0);

    if (compare == NULL) {
	LocalStore(&listPtr->compare,&compareDataPtrAddresses); /* default */
    } else {
        LocalStore(&listPtr->compare,compare);
    }

    return listPtr;
}


/* =============================================================================
 * freeNode
 * =============================================================================
 */
static void
freeNode (list_node_t* nodePtr)
{
    P_FREE(nodePtr);
}


/* =============================================================================
 * PfreeNode
 * =============================================================================
 */
static void
PfreeNode (list_node_t* nodePtr)
{
    P_FREE(nodePtr);
}


/* =============================================================================
 * TMfreeNode
 * =============================================================================
 */
__attribute__((atomic))
static void
TMfreeNode (al_t* lock, list_node_t* nodePtr)
{
    TM_FREE(nodePtr);
}


/* =============================================================================
 * freeList
 * =============================================================================
 */
static void
freeList (list_node_t* nodePtr)
{
    if (nodePtr != NULL) {
        freeList(nodePtr->nextPtr);
        freeNode(nodePtr);
    }
}


/* =============================================================================
 * PfreeList
 * =============================================================================
 */
static void
PfreeList (list_node_t* nodePtr)
{
    if (nodePtr != NULL) {
        PfreeList(nodePtr->nextPtr);
        PfreeNode(nodePtr);
    }
}


/* =============================================================================
 * TMfreeList
 * =============================================================================
 */
static void TMfreeList (al_t* lock, list_node_t* nodePtr);
__attribute__((atomic))
static void
TMfreeList (al_t* lock, list_node_t* nodePtr)
{
    if (nodePtr != NULL) {
        list_node_t* nextPtr = (list_node_t*)TM_SHARED_READ_P(nodePtr->nextPtr);
        TMfreeList(lock, nextPtr);
        TMfreeNode(lock, nodePtr);
    }
}


/* =============================================================================
 * list_free
 * =============================================================================
 */
void
list_free (list_t* listPtr)
{
    freeList(listPtr->head.nextPtr);
    P_FREE(listPtr);
}


/* =============================================================================
 * Plist_free
 * =============================================================================
 */
void
Plist_free (list_t* listPtr)
{
    PfreeList(listPtr->head.nextPtr);
    P_FREE(listPtr);
}


/* =============================================================================
 * TMlist_free
 * =============================================================================
 */
__attribute__((atomic))
void
TMlist_free (al_t* lock, list_t* listPtr)
{
    list_node_t* nextPtr = (list_node_t*)TM_SHARED_READ_P(listPtr->head.nextPtr);
    TMfreeList(lock, nextPtr);
    TM_FREE(listPtr);
}


/* =============================================================================
 * list_isEmpty
 * -- Return TRUE if list is empty, else FALSE
 * =============================================================================
 */
bool_t
list_isEmpty (list_t* listPtr)
{
    return (listPtr->head.nextPtr == NULL);
}


/* =============================================================================
 * TMlist_isEmpty
 * -- Return TRUE if list is empty, else FALSE
 * =============================================================================
 */
__attribute__((atomic))
bool_t
TMlist_isEmpty (al_t* lock, list_t* listPtr)
{
    return (((void*)TM_SHARED_READ_P(listPtr->head.nextPtr) == NULL) ?
            TRUE : FALSE);
}


/* =============================================================================
 * list_getSize
 * -- Returns the size of the list
 * =============================================================================
 */
long
list_getSize (list_t* listPtr)
{
    return listPtr->size;
}


/* =============================================================================
 * TMlist_getSize
 * -- Returns the size of the list
 * =============================================================================
 */
__attribute__((atomic))
long
TMlist_getSize (al_t* lock, list_t* listPtr)
{
    return (long)TM_SHARED_READ(listPtr->size);
}


/* =============================================================================
 * findPrevious
 * =============================================================================
 */
static list_node_t*
findPrevious (list_t* listPtr, void* dataPtr)
{
    al_t dummy = AL_INITIALIZER(__FILE__ ":" __func__);
    list_node_t* prevPtr = &(listPtr->head);
    list_node_t* nodePtr = prevPtr->nextPtr;

    for (; nodePtr != NULL; nodePtr = nodePtr->nextPtr) {
        if (listPtr->compare(&dummy, nodePtr->dataPtr, dataPtr) >= 0) {
            return prevPtr;
        }
        prevPtr = nodePtr;
    }

    return prevPtr;
}


/* =============================================================================
 * TMfindPrevious
 * =============================================================================
 */
__attribute__((atomic))
static list_node_t*
TMfindPrevious (al_t* lock, list_t* listPtr, void* dataPtr)
{
    list_node_t* prevPtr = &(listPtr->head);
    list_node_t* nodePtr;
    long (*compare)(al_t*, const void*, const void*) = (long (*)(al_t*, const void*, const void*))LocalLoad(&listPtr->compare);

    for (nodePtr = (list_node_t*)TM_SHARED_READ_P(prevPtr->nextPtr);
         nodePtr != NULL;
         nodePtr = (list_node_t*)TM_SHARED_READ_P(nodePtr->nextPtr))
    {
        if (compare(lock, LocalLoad(&nodePtr->dataPtr), dataPtr) >= 0) {
            return prevPtr;
        }
        prevPtr = nodePtr;
    }

    return prevPtr;
}


/* =============================================================================
 * list_find
 * -- Returns NULL if not found, else returns pointer to data
 * =============================================================================
 */
void*
list_find (list_t* listPtr, void* dataPtr)
{
    al_t dummy = AL_INITIALIZER(__FILE__ ":" __func__);
    list_node_t* nodePtr;
    list_node_t* prevPtr = findPrevious(listPtr, dataPtr);

    nodePtr = prevPtr->nextPtr;

    if ((nodePtr == NULL) ||
        (listPtr->compare(&dummy, nodePtr->dataPtr, dataPtr) != 0)) {
        return NULL;
    }

    return (nodePtr->dataPtr);
}


/* =============================================================================
 * TMlist_find
 * -- Returns NULL if not found, else returns pointer to data
 * =============================================================================
 */
__attribute__((atomic))
void*
TMlist_find (al_t* lock, list_t* listPtr, void* dataPtr)
{
    list_node_t* nodePtr;
    list_node_t* prevPtr = TMfindPrevious(lock, listPtr, dataPtr);
    long (*compare)(al_t*, const void*, const void*) = (long (*)(al_t*, const void*, const void*))LocalLoad(&listPtr->compare);

    nodePtr = (list_node_t*)TM_SHARED_READ_P(prevPtr->nextPtr);

    if ((nodePtr == NULL) ||
        compare(lock, LocalLoad(&nodePtr->dataPtr), dataPtr) != 0) {
        return NULL;
    }

    return LocalLoad(&nodePtr->dataPtr);
}


/* =============================================================================
 * list_insert
 * -- Return TRUE on success, else FALSE
 * =============================================================================
 */
bool_t
list_insert (list_t* listPtr, void* dataPtr)
{
    al_t dummy = AL_INITIALIZER(__FILE__ ":" __func__);
    list_node_t* prevPtr;
    list_node_t* nodePtr;
    list_node_t* currPtr;

    prevPtr = findPrevious(listPtr, dataPtr);
    currPtr = prevPtr->nextPtr;

#ifdef LIST_NO_DUPLICATES
    if ((currPtr != NULL) &&
        listPtr->compare(&dummy, currPtr->dataPtr, dataPtr) == 0) {
        return FALSE;
    }
#endif

    nodePtr = allocNode(dataPtr);
    if (nodePtr == NULL) {
        return FALSE;
    }

    nodePtr->nextPtr = currPtr;
    prevPtr->nextPtr = nodePtr;
    listPtr->size++;

    return TRUE;
}


/* =============================================================================
 * Plist_insert
 * -- Return TRUE on success, else FALSE
 * =============================================================================
 */
bool_t
Plist_insert (list_t* listPtr, void* dataPtr)
{
    al_t dummy = AL_INITIALIZER(__FILE__ ":" __func__);
    list_node_t* prevPtr;
    list_node_t* nodePtr;
    list_node_t* currPtr;

    prevPtr = findPrevious(listPtr, dataPtr);
    currPtr = prevPtr->nextPtr;

#ifdef LIST_NO_DUPLICATES
    if ((currPtr != NULL) &&
        listPtr->compare(&dummy, currPtr->dataPtr, dataPtr) == 0) {
        return FALSE;
    }
#endif

    nodePtr = PallocNode(dataPtr);
    if (nodePtr == NULL) {
        return FALSE;
    }

    nodePtr->nextPtr = currPtr;
    prevPtr->nextPtr = nodePtr;
    listPtr->size++;

    return TRUE;
}


/* =============================================================================
 * TMlist_insert
 * -- Return TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((atomic))
bool_t
TMlist_insert (al_t* lock, list_t* listPtr, void* dataPtr)
{
    list_node_t* prevPtr;
    list_node_t* nodePtr;
    list_node_t* currPtr;

    prevPtr = TMfindPrevious(lock, listPtr, dataPtr);
    currPtr = (list_node_t*)TM_SHARED_READ_P(prevPtr->nextPtr);

#ifdef LIST_NO_DUPLICATES
    long (*compare)(al_t*, const void*, const void*) = (long (*)(al_t*, const void*, const void*))LocalLoad(&listPtr->compare);
    if ((currPtr != NULL) &&
        compare(lock, LocalLoad(&currPtr->dataPtr), dataPtr) == 0) {
        return FALSE;
    }
#endif

    nodePtr = TMallocNode(lock, dataPtr);
    if (nodePtr == NULL) {
        return FALSE;
    }

    LocalStore(&nodePtr->nextPtr, currPtr);
    TM_SHARED_WRITE_P(prevPtr->nextPtr, nodePtr);
    TM_SHARED_WRITE(listPtr->size, (TM_SHARED_READ(listPtr->size) + 1));

    return TRUE;
}


/* =============================================================================
 * list_remove
 * -- Returns TRUE if successful, else FALSE
 * =============================================================================
 */
bool_t
list_remove (list_t* listPtr, void* dataPtr)
{
    al_t dummy = AL_INITIALIZER(__FILE__ ":" __func__);
    list_node_t* prevPtr;
    list_node_t* nodePtr;

    prevPtr = findPrevious(listPtr, dataPtr);

    nodePtr = prevPtr->nextPtr;
    if ((nodePtr != NULL) &&
        (listPtr->compare(&dummy, nodePtr->dataPtr, dataPtr) == 0))
    {
        prevPtr->nextPtr = nodePtr->nextPtr;
        nodePtr->nextPtr = NULL;
        freeNode(nodePtr);
        listPtr->size--;
        assert(listPtr->size >= 0);
        return TRUE;
    }

    return FALSE;
}


/* =============================================================================
 * Plist_remove
 * -- Returns TRUE if successful, else FALSE
 * =============================================================================
 */
bool_t
Plist_remove (list_t* listPtr, void* dataPtr)
{
    al_t dummy = AL_INITIALIZER(__FILE__ ":" __func__);
    list_node_t* prevPtr;
    list_node_t* nodePtr;

    prevPtr = findPrevious(listPtr, dataPtr);

    nodePtr = prevPtr->nextPtr;
    if ((nodePtr != NULL) &&
        (listPtr->compare(&dummy, nodePtr->dataPtr, dataPtr) == 0))
    {
        prevPtr->nextPtr = nodePtr->nextPtr;
        nodePtr->nextPtr = NULL;
        PfreeNode(nodePtr);
        listPtr->size--;
        assert(listPtr->size >= 0);
        return TRUE;
    }

    return FALSE;
}


/* =============================================================================
 * TMlist_remove
 * -- Returns TRUE if successful, else FALSE
 * =============================================================================
 */
__attribute__((atomic))
bool_t
TMlist_remove (al_t* lock, list_t* listPtr, void* dataPtr)
{
    list_node_t* prevPtr;
    list_node_t* nodePtr;
    long (*compare)(al_t*, const void*, const void*) = (long (*)(al_t*, const void*, const void*))LocalLoad(&listPtr->compare);

    prevPtr = TMfindPrevious(lock, listPtr, dataPtr);

    nodePtr = (list_node_t*)TM_SHARED_READ_P(prevPtr->nextPtr);
    if ((nodePtr != NULL) &&
	compare(lock, LocalLoad(&nodePtr->dataPtr), dataPtr) == 0)
    {
        TM_SHARED_WRITE_P(prevPtr->nextPtr, TM_SHARED_READ_P(nodePtr->nextPtr));
        TM_SHARED_WRITE_P(nodePtr->nextPtr, (struct list_node*)NULL);
        TMfreeNode(lock, nodePtr);
        TM_SHARED_WRITE(listPtr->size, (TM_SHARED_READ(listPtr->size) - 1));
        assert(TM_SHARED_READ(listPtr->size) >= 0);
        return TRUE;
    }

    return FALSE;
}


/* =============================================================================
 * list_clear
 * -- Removes all elements
 * =============================================================================
 */
void
list_clear (list_t* listPtr)
{
    freeList(listPtr->head.nextPtr);
    listPtr->head.nextPtr = NULL;
    listPtr->size = 0;
}


/* =============================================================================
 * Plist_clear
 * -- Removes all elements
 * =============================================================================
 */
void
Plist_clear (list_t* listPtr)
{
    PfreeList(listPtr->head.nextPtr);
    listPtr->head.nextPtr = NULL;
    listPtr->size = 0;
}


/* =============================================================================
 * TEST_LIST
 * =============================================================================
 */
#ifdef TEST_LIST


#include <assert.h>
#include <stdio.h>


static long
compare (al_t* lock, const void* a, const void* b)
{
    return (*((const long*)a) - *((const long*)b));
}

__attribute__((atomic))
static void
printList (al_t* lock, list_t* listPtr)
{
    list_iter_t it;
    printf("[");
    TMlist_iter_reset(lock, &it, listPtr);
    while (TMlist_iter_hasNext(lock, &it, listPtr)) {
        printf("%li ", *((long*)(TMlist_iter_next(lock, &it, listPtr))));
    }
    puts("]");
}

__attribute__((atomic))
static void
insertInt (al_t* lock, list_t* listPtr, long* data)
{
    printf("Inserting: %li\n", *data);
    TMlist_insert(lock, listPtr, (void*)data);
    printList(lock, listPtr);
}


__attribute__((atomic))
static void
removeInt (al_t* lock, list_t* listPtr, long* data)
{
    printf("Removing: %li\n", *data);
    TMlist_remove(lock, listPtr, (void*)data);
    printList(lock, listPtr);
}

void*
test (void* arg)
{
    al_t* lock = (al_t*)arg;
    list_t* listPtr;
#ifdef LIST_NO_DUPLICATES
    long data1[] = {3, 1, 4, 1, 5, -1};
#else
    long data1[] = {3, 1, 4, 5, -1};
#endif
    long data2[] = {3, 1, 4, 1, 5, -1};
    long i;

    puts("Starting...");

    puts("List sorted by values:");

    listPtr = TMlist_alloc(lock, &compare);

    for (i = 0; data1[i] >= 0; i++) {
        insertInt(lock, listPtr, &data1[i]);
        assert(*((long*)TMlist_find(lock, listPtr, &data1[i])) == data1[i]);
    }

    for (i = 0; data1[i] >= 0; i++) {
        removeInt(lock, listPtr, &data1[i]);
        assert(TMlist_find(lock, listPtr, &data1[i]) == NULL);
    }

    TMlist_free(lock, listPtr);

    puts("List sorted by addresses:");

    listPtr = TMlist_alloc(lock, NULL);

    for (i = 0; data2[i] >= 0; i++) {
        insertInt(lock, listPtr, &data2[i]);
        assert(*((long*)TMlist_find(lock, listPtr, &data2[i])) == data2[i]);
    }

    for (i = 0; data2[i] >= 0; i++) {
        removeInt(lock, listPtr, &data2[i]);
        assert(TMlist_find(lock, listPtr, &data2[i]) == NULL);
    }

    TMlist_free(lock, listPtr);

    puts("Done.");

    return 0;
}

int
main ()
{
    pthread_t t;
    al_t dummy = AL_INITIALIZER(__FILE__ ":" __func__);

    setAdaptMode(1);
    pthread_create(&t,0,test,&lock);
    pthread_join(t,0);
    return 0;
}

#endif /* TEST_LIST */


/* =============================================================================
 *
 * End of list.c
 *
 * =============================================================================
 */

