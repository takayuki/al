/* =============================================================================
 *
 * rbtree.h
 * -- Red-black balanced binary search tree
 *
 * =============================================================================
 *
 * Copyright (C) Sun Microsystems Inc., 2006.  All Rights Reserved.
 * Authors: Dave Dice, Nir Shavit, Ori Shalev.
 *
 * STM: Transactional Locking for Disjoint Access Parallelism
 *
 * Transactional Locking II,
 * Dave Dice, Ori Shalev, Nir Shavit
 * DISC 2006, Sept 2006, Stockholm, Sweden.
 *
 * =============================================================================
 *
 * Modified by Chi Cao Minh
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


#ifndef RBTREE_H
#define RBTREE_H 1


#include <inttypes.h>
#include "al.h"
#include "tm.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct node {
    long k;
    long v;
    struct node* p;
    struct node* l;
    struct node* r;
    long c;
} node_t;


typedef struct rbtree {
    node_t* root;
} rbtree_t;


/* =============================================================================
 * rbtree_verify
 * =============================================================================
 */
long
rbtree_verify (rbtree_t* s, long verbose);


/* =============================================================================
 * rbtree_alloc
 * =============================================================================
 */
rbtree_t*
rbtree_alloc ();


/* =============================================================================
 * TMrbtree_alloc
 * =============================================================================
 */
rbtree_t*
TMrbtree_alloc (al_t* l);


/* =============================================================================
 * rbtree_free
 * =============================================================================
 */
void
rbtree_free (rbtree_t* r);


/* =============================================================================
 * TMrbtree_free
 * =============================================================================
 */
void
TMrbtree_free (al_t* l, rbtree_t* r);


/* =============================================================================
 * rbtree_insert
 * =============================================================================
 */
long
rbtree_insert (rbtree_t* r, long key, long val);


/* =============================================================================
 * TMrbtree_insert
 * =============================================================================
 */
long
TMrbtree_insert (al_t* l, rbtree_t* r, long key, long val);


/* =============================================================================
 * rbtree_delete
 * =============================================================================
 */
long
rbtree_delete (rbtree_t* r, long key);


/* =============================================================================
 * TMrbtree_delete
 * =============================================================================
 */
long
TMrbtree_delete (al_t* l, rbtree_t* r, long key);


/* =============================================================================
 * rbtree_update
 * =============================================================================
 */
long
rbtree_update (rbtree_t* r, long key, long val);


/* =============================================================================
 * TMrbtree_update
 * =============================================================================
 */
long
TMrbtree_update (al_t* l, rbtree_t* r, long key, long val);


/* =============================================================================
 * rbtree_get
 * =============================================================================
 */
long
rbtree_get (rbtree_t* r, long key);


/* =============================================================================
 * TMrbtree_get
 * =============================================================================
 */
long
TMrbtree_get (al_t* l, rbtree_t* r, long key);


/* =============================================================================
 * rbtree_contains
 * =============================================================================
 */
long
rbtree_contains (rbtree_t* r, long key);


/* =============================================================================
 * TMrbtree_contains
 * =============================================================================
 */
long
TMrbtree_contains (al_t* l, rbtree_t* r, long key);


#define TMRBTREE_ALLOC(l)           TMrbtree_alloc(l)
#define TMRBTREE_FREE(l,r)          TMrbtree_free(l, r)
#define TMRBTREE_INSERT(l, r, k, v) TMrbtree_insert(l,  r, (long)(k), (long)(v))
#define TMRBTREE_DELETE(l, r, k)    TMrbtree_delete(l, r, (long)(k))
#define TMRBTREE_UPDATE(l, r, k, v) TMrbtree_update(l, r, (long)(k), (long)(v))
#define TMRBTREE_GET(l, r, k)       TMrbtree_get(l, r, (long)(k))
#define TMRBTREE_CONTAINS(l, r, k)  TMrbtree_contains(l, r, (long)(k))


#ifdef __cplusplus
}
#endif



#endif /* RBTREE_H */



/* =============================================================================
 *
 * End of rbtree.h
 *
 * =============================================================================
 */
