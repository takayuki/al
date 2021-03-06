/*
 * Al -- an implementation of the adaptive locks
 *
 * Copyright (C) 2008, University of Oregon
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 * 
 *   * Neither the name of University of Oregon nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF OREGON ``AS IS'' AND ANY
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
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "alx.h"
#include "fmt.h"
#include "random.h"
#include "queue.h"

void
help(void)
{
  fprintf(stderr,
          "usage: hashtable [-hnpx]\n"
          "  -p  number of threads (default: 2)\n"
          "  -n  number of repeats (default: 100)\n"
          "  -a  use adaptive lock (default)\n"
          "  -l  use lock only\n"
          "  -t  use transaction only\n"
          "  -x  transactional overhead (default: 5.0)\n"
          "  -z  number of instr. in transactional load (default: 50)\n"
          "  -f  power number of locks (default: 0(single lock))\n"
          "  -h  show this\n");
  exit(0);
}

#define NBUCKETS	1024
#define BUCKETMASK	(NBUCKETS-1)

typedef unsigned long hash_code;

typedef struct hash_node {
  LIST_ENTRY(hash_node) next;
  hash_code hash;
  char key[16];
  char value[16];
} hash_node;

LIST_HEAD(bucket_head,hash_node);

typedef struct {
  struct bucket_head table[NBUCKETS];
  al_t locks[NBUCKETS];
} hash_table;

static hash_table ht;
static unsigned long NLOCKS = 1;
static unsigned long LOCKMASK = 0;

static hash_code
hash(const char* s)
{
  hash_code h = 1;
  hash_code ch;
  while (ch = *(unsigned char *)s) {
    h = h * 1000003 + ch;
    s++;
  }
  return h;
}

__attribute__((atomic))
hash_node*
atomic_insert(al_t* lock,hash_node* newnode)
{
  struct bucket_head* head;
  hash_node* prev;
  hash_node* node;
  hash_code h,h2;
  hash_node* result = 0;

  newnode->hash = hash(newnode->key);
  h = newnode->hash;
  head = &ht.table[(h & BUCKETMASK)];
  prev = 0;
  LIST_FOREACH(node,head,next) {
    h2 = node->hash;
    if (h == h2 && strcmp(newnode->key,node->key) == 0) {
      LIST_REMOVE(node,next);
      if (prev) LIST_INSERT_AFTER(prev,newnode,next);
      else LIST_INSERT_HEAD(head,newnode,next);
      result = node;
      goto done;
    }
    prev = node;
  }
  if (prev) LIST_INSERT_AFTER(prev,newnode,next);
  else LIST_INSERT_HEAD(head,newnode,next);
 done:
  return result;
}

__attribute__((atomic))
hash_node*
atomic_delete(al_t* lock,char* key)
{
  struct bucket_head* head;
  hash_node* node;
  hash_code h,h2;
  hash_node* result = 0;

  h = hash(key);
  head = &ht.table[(h & BUCKETMASK)];
  LIST_FOREACH(node,head,next) {
    h2 = node->hash;
    if (h == h2 && strcmp(key,node->key) == 0) {
      LIST_REMOVE(node,next);
      result = node;
      goto done;
    }
  }
 done:
  return result;
}

__attribute__((atomic ("ro")))
hash_node*
atomic_find(al_t* lock,char* key)
{
  struct bucket_head* head;
  hash_node* node;
  hash_code h,h2;
  hash_node* result = 0;

  h = hash(key);
  head = &ht.table[(h & BUCKETMASK)];
  LIST_FOREACH(node,head,next) {
    h2 = node->hash;
    if (h == h2 && strcmp(key,node->key) == 0) {
      result = node;
      goto done;
    }
  }
 done:
  return result;
}

hash_node*
insert(hash_node* node)
{
  return atomic_insert(&ht.locks[(hash(node->key) & LOCKMASK)],node);
}

hash_node*
delete(char* key)
{
  return atomic_delete(&ht.locks[(hash(key) & LOCKMASK)],key);
}

hash_node*
find(char* key)
{
  return atomic_find(&ht.locks[(hash(key) & LOCKMASK)],key);
}

void*
validate(void* arg)
{
  int i;
  struct bucket_head* head;
  hash_node* node;
  unsigned long entries = 0;

  for (i = 0; i < NBUCKETS; i++) {
    head = &ht.table[i];
    LIST_FOREACH(node,head,next) {
      if (node->hash != hash(node->key))
	printf("*** Oops, %s(%lx/%lx)\n",
	       node->key,node->hash,hash(node->key));
      entries++;
    }
  }
  printf("number of entries=%ld\n",entries);
  return 0;
}

static int thrd = 2;
static int iter = 100000;

void*
task(void* arg)
{
  unsigned short id = (unsigned short)(unsigned long)arg;
  long n = iter;
  char key[16];
  hash_node* node;
  unsigned long seed = id;
  unsigned long rand;
  
  key[0] = 'k';
  while (n--) {
    rand = Random(&seed);
    fmt_ulong(&key[1],rand % 1000);
    rand = Random(&seed);
    rand >>= 6;
    switch(rand%4) {
    case 0:
      if ((node = malloc(sizeof(hash_node))) == 0)
	abort();
      strcpy(node->key,key);
      strcpy(node->value,key);
      node = insert(node);
      if (node)
	free(node);
      break;
    case 1:
      node = delete(key);
      if (node)
	free(node);
      break;
    default:
      find(key);
      break;
    }
  }
  return 0;
}

#ifdef HAVE_GETHRTIME
static hrtime_t start;
static hrtime_t totalElapse;
#else
static struct timeval start;
static struct timeval totalElapse;
#endif
static int totalThreads;
static pthread_mutex_t totalMutex = PTHREAD_MUTEX_INITIALIZER;  
#ifdef HAVE_PTHREAD_BARRIER
static pthread_barrier_t invokeBarrier;
#endif

void*
invoke(void* arg)
{
  void* ret;

#ifdef HAVE_PTHREAD_BARRIER
  pthread_barrier_wait(&invokeBarrier);
#endif
  pthread_mutex_lock(&totalMutex);
  if (totalThreads == 0) timer_start(&start);
  totalThreads++;
  pthread_mutex_unlock(&totalMutex);
  ret = task(arg);
  pthread_mutex_lock(&totalMutex);
  totalThreads--;
  if (totalThreads == 0) timer_stop(&start,&totalElapse);
  pthread_mutex_unlock(&totalMutex);
  return ret;
}

int
main(int argc,char* argv[])
{
  int ch,i;
  pthread_t t[256];
  void* r;
  double elapse;
  size_t size;

  while ((ch = getopt(argc,argv,"p:n:altx:z:f:h")) != -1) {
    switch (ch) {
    case 'p': thrd = atoi(optarg); break;
    case 'n': iter = atoi(optarg); break;
    case 'a': setAdaptMode(0); break;
    case 'l': setAdaptMode(-1); break;
    case 't': setAdaptMode(1); break;
    case 'x': setTranxOvhd(atof(optarg)); break;
    case 'z': setTranxInstr(atoi(optarg),atoi(optarg)*2); break;
    case 'f': NLOCKS = 1<<atoi(optarg); LOCKMASK = NLOCKS-1; break;
    case 'h':
    default: help();
    }
  }
  argc -= optind;
  argv += optind;

  if (NBUCKETS < NLOCKS) {
    NLOCKS = NBUCKETS; LOCKMASK = NLOCKS-1;
  }
  for (i = 0; i < NLOCKS; i++)
    al_init(&ht.locks[i],"ht.locks[]");
  if (256 <= thrd) thrd = 256;
#ifdef HAVE_PTHREAD_BARRIER
  pthread_barrier_init(&invokeBarrier,0,thrd);
#endif
  for (i = 0; i < thrd; i++)
    pthread_create(&t[i],0,invoke,i+1);
  for (i = 0; i < thrd; i++)
    pthread_join(t[i],&r);
  pthread_create(&t[0],0,validate,0);
  pthread_join(t[0],&r);
#ifdef HAVE_GETHRTIME
  elapse = ((double)totalElapse) / 1000000000.0;
#else
  elapse = (double)totalElapse.tv_sec;
  elapse += ((double)totalElapse.tv_usec) / 1000000.0;
#endif
  printf("elapse=%.3lf,exec_per_sec=%.3lf\n",
	 elapse,((double)(thrd*iter))/elapse);
  for (i = 0; i < NLOCKS; i++)
    al_dump(&ht.locks[i]);
  return 0;
}
