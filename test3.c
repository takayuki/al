#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "alx.h"
#include "tree.h"

void
help(void)
{
  fprintf(stderr,
          "usage: test3 [-hnpx]\n"
          "  -p  number of threads (default: 2)\n"
          "  -n  number of repeats (default: 100)\n"
          "  -a  use adaptive lock (default)\n"
          "  -l  use lock only\n"
          "  -t  use transaction only\n"
          "  -s  lock scheme (default: 1)\n"
          "  -x  transactional overhead x10 (default: 50)\n"
          "  -h  show this\n");
  exit(0);
}

RB_HEAD(TREE,tree_node) tab = RB_INITIALIZER(tab);

typedef struct tree_node {
  long key;
  long value;
  RB_ENTRY(tree_node) links;
} tree_node;

__attribute__((atomic ("l1")))
int
node_cmp(tree_node* x,tree_node* y)
{
  if (x == 0 || y == 0)
    abort();
  return x->key - y->key;
}

RB_GENERATE(TREE,tree_node,links,node_cmp,"l1");

__attribute__((atomic ("l1")))
tree_node*
insert(tree_node* node)
{
  return TREE_RB_INSERT(&tab,node);
}

__attribute__((atomic ("l1")))
tree_node*
delete(long n)
{
  tree_node find;
  tree_node* r;

  find.key = n;
  if ((r = TREE_RB_FIND(&tab,&find)))
    return TREE_RB_REMOVE(&tab,r);
  else
    return 0;
}

__attribute__((atomic ("l1","ro")))
tree_node*
find(long n)
{
  tree_node find;
  find.key = n;
  return TREE_RB_FIND(&tab, &find);
}

static int thrd = 2;
static int iter = 100000;

void*
task(void* arg)
{
  unsigned short id = (unsigned short)(unsigned int)arg;
  long n = iter;
  long key;
  tree_node* node;
  unsigned long seed = id;
  unsigned long rand;

  while (n--) {
    rand = Random(&seed);
    key = rand % 1000;
    rand = Random(&seed);
    rand >>= 6;
    switch(rand%4) {
    case 0:
      if ((node = malloc(sizeof(tree_node))) == 0)
	abort();
      node->key = key;
      node->value = key;
      if (insert(node))
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

void*
validate(void* arg)
{
  int flag = (int)arg;
  struct tree_node *p;
  long a = 0,b = 0,c = 0;

  setAdaptMode(-1);
  RB_FOREACH(p,TREE,&tab) {
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
  if (totalThreads == 0) timer_stop(&start,&totalElapse,0,0);
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

  while ((ch = getopt(argc,argv,"p:n:atls:x:h")) != -1) {
    switch (ch) {
    case 'p': thrd = atoi(optarg); break;
    case 'n': iter = atoi(optarg); break;
    case 'a': setAdaptMode(0); break;
    case 'l': setAdaptMode(-1); break;
    case 't': setAdaptMode(1); break;
    case 's': setLockScheme(atoi(optarg)); break;
    case 'x': setTranxOvhd(atoi(optarg)); break;
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
