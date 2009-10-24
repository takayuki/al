#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "alx.h"
#include "tree.h"

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

SPLAY_HEAD(TREE,tree_node) tab = SPLAY_INITIALIZER(tab);

struct tree_node {
  long key;
  long val;
  SPLAY_ENTRY(tree_node) links;
};

__attribute__((atomic ("l1")))
int
node_cmp(struct tree_node* x,struct tree_node* y)
{
  if (x == 0 || y == 0) abort();
  return x->key - y->key;
}

SPLAY_PROTOTYPE(TREE,tree_node,links,node_cmp,"l1");
SPLAY_GENERATE(TREE,tree_node,links,node_cmp,"l1");

__attribute__((atomic ("l1")))
void
add(long n)
{
  struct tree_node* r;
  
  if ((r = (struct tree_node*)malloc(sizeof(*r))) == 0)
    abort();
  r->key = n;
  r->val = n+1;
  TREE_SPLAY_INSERT(&tab,r);
  return;
}

__attribute__((atomic ("l1")))
struct tree_node*
del(long n)
{
  struct tree_node find;
  struct tree_node* r;

  find.key = n;
  if ((r = TREE_SPLAY_FIND(&tab, &find)))
    return TREE_SPLAY_REMOVE(&tab,r);
  else
    return 0;
}

__attribute__((atomic ("l1")))
struct tree_node*
lookup(long n)
{
  struct tree_node find;

  find.key = n;
  return TREE_SPLAY_FIND(&tab, &find);
}

static int thrd = 2;
static int iter = 100000;

void*
task(void* arg)
{
  unsigned short id = (unsigned short)arg;
  long n = iter;
  long t;
  unsigned short xseed[3] = {id,id,id};
  unsigned long rand;

  while (n--) {
    rand = nrand48(xseed);
    t = rand % 1000;
    switch(rand%4) {
    case 0: add(t); break;
    case 1: del(t); break;
    default: lookup(t); break;
    }
  }
  return 0;
}

void*
validate(void* arg)
{
  int flag = (int)arg;
  struct tree_node *p;
  long a = 0,b = 0,c = 0;

  setAdaptMode(-1);
  for (p = SPLAY_MIN(TREE,&tab); p != 0; p = SPLAY_NEXT(TREE,&tab,p)) {
    b = p->key;
    if (!(a <= b)) printf("*** Oops, %ld <= %ld\n",a,b);
    a = b; c++;
    if(flag) printf("%ld\n",a);
  }
  printf("number of elements=%ld\n",c);
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

  while ((ch = getopt(argc,argv,"p:n:atlx:")) != -1) {
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
