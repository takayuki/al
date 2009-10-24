#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "alx.h"
#include "queue.h"

void
help(void)
{
  fprintf(stderr,
          "usage: test [-hnpx]\n"
          "  -p  number of threads (default: 2)\n"
          "  -n  number of repeats (default: 100)\n"
          "  -a  use adaptive lock (default)\n"
          "  -l  use lock only\n"
          "  -t  use transaction only\n"
          "  -x  transactional overhead (default: 25)\n"
          "  -h  show this\n");
  exit(0);
}

#define NBUCKETS	1024
#define BUCKETMASK	(NBUCKETS-1)

typedef unsigned long hash_code;

typedef struct hash_node {
  LIST_ENTRY(hash_node) next;
  hash_code hash;
  char* key;
  char* value;
} hash_node;

LIST_HEAD(bucket_head,hash_node);

typedef struct {
  struct bucket_head table[NBUCKETS];
  unsigned long entries;
} hash_table;

static hash_table t;

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

__attribute__((atomic ("l1")))
hash_node*
insert(hash_node* newnode)
{
  struct bucket_head* head;
  hash_node* prev;
  hash_node* node;
  hash_code h,h2;
  hash_node* result = 0;

  newnode->hash = hash(newnode->key);
  h = newnode->hash;
  head = &t.table[(h & BUCKETMASK)];
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
  t.entries++;
 done:
  return result;
}

__attribute__((atomic ("l1")))
hash_node*
delete(char* key)
{
  struct bucket_head* head;
  hash_node* node;
  hash_code h,h2;
  hash_node* result = 0;

  h = hash(key);
  head = &t.table[(h & BUCKETMASK)];
  LIST_FOREACH(node,head,next) {
    h2 = node->hash;
    if (h == h2 && strcmp(key,node->key) == 0) {
      LIST_REMOVE(node,next);
      t.entries--;
      result = node;
      goto done;
    }
  }
 done:
  return result;
}

__attribute__((atomic ("l1","ro")))
hash_node*
find(char* key)
{
  struct bucket_head* head;
  hash_node* node;
  hash_code h,h2;
  hash_node* result = 0;

  h = hash(key);
  head = &t.table[(h & BUCKETMASK)];
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

void*
validate(void* arg)
{
  int i;
  struct bucket_head* head;
  hash_node* node;
  unsigned long entries = 0;

  for (i = 0; i < NBUCKETS; i++) {
    head = &t.table[i];
    LIST_FOREACH(node,head,next) {
      if (node->hash != hash(node->key))
	printf("*** Oops, %s(%lx/%lx)\n",
	       node->key,node->hash,hash(node->key));
      entries++;
    }
  }
  printf("number of entries=%ld/%ld\n",entries,t.entries);
  return 0;
}

static int thrd = 2;
static int iter = 100000;

void*
task(void* arg)
{
  unsigned short id = (unsigned short)arg;
  long n = iter;
  char key[20];
  hash_node* node;
  unsigned short xseed[3] = {id,id,id};
  unsigned long rand;

  while (n--) {
    rand = nrand48(xseed);
    sprintf(key,"k%ld",(rand % 1000));
    switch(rand%4) {
    case 0:
      if ((node = malloc(sizeof(hash_node))) == 0) abort();
      if ((node->key = strdup(key)) == 0) abort();
      if ((node->value = strdup(node->key)) == 0) abort();
      node = insert(node);
      if (node) { free(node->key); free(node->value); free(node); }
      break;
    case 1:
      node = delete(key);
      if (node) { free(node->key); free(node->value); free(node); }
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

void*
invoke(void* arg)
{
  void* ret;

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

  while ((ch = getopt(argc,argv,"p:n:altx:")) != -1) {
    switch (ch) {
    case 'n': iter = atoi(optarg); break;
    case 'p': thrd = atoi(optarg); break;
    case 'a': setAdaptMode(0); break;
    case 'l': setAdaptMode(-1); break;
    case 't': setAdaptMode(1); break;
    case 'x': setTransactOvhd(atof(optarg)); break;
    case 'h':
    default: help();
    }
  }
  argc -= optind;
  argv += optind;

  if (256 <= thrd) thrd = 256;
  for (i = 0; i < thrd; i++) pthread_create(&t[i],0,invoke,i+1);
  for (i = 0; i < thrd; i++) pthread_join(t[i],&r);
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
  return 0;
}
