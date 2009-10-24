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

RB_HEAD(TREE,tree_node) tab = RB_INITIALIZER(tab);

struct tree_node {
  long key;
  long val;
  RB_ENTRY(tree_node) links;
};

__attribute__((atomic ("l1")))
int
node_cmp(struct tree_node* x,struct tree_node* y)
{
  assert(x != 0);
  assert(y != 0);
  return x->key - y->key;
}

RB_GENERATE(TREE,tree_node,links,node_cmp,"l1");

__attribute__((atomic ("l1")))
void
add(long n)
{
  struct tree_node* r;
  
  assert((r = (struct tree_node*)malloc(sizeof(*r))));
  r->key = n;
  r->val = n+1;
  TREE_RB_INSERT(&tab,r);
  return;
}

__attribute__((atomic ("l1")))
struct tree_node*
del(long n)
{
  struct tree_node find;
  struct tree_node* r;

  find.key = n;
  if ((r = TREE_RB_FIND(&tab, &find)))
    return TREE_RB_REMOVE(&tab,r);
  else
    return 0;
}

__attribute__((atomic ("l1","ro")))
struct tree_node*
lookup(long n)
{
  struct tree_node find;

  find.key = n;
  return TREE_RB_FIND(&tab, &find);
}

void*
task(void* arg)
{
  long n = (int)arg;
  long t;

  while (n--) {
    t = random() % 1000;
    switch(n%4) {
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
  RB_FOREACH(p,TREE,&tab) {
    b = p->key;
    if (!(a <= b)) printf("*** Oops, %ld <= %ld\n",a,b);
    a = b; c++;
    if(flag) printf("%ld\n",a);
  }
  printf("number of elements=%ld\n",c);
  return 0;
}

int
main(int argc,char* argv[])
{
  int p = 2,n = 100,ch,i;
  pthread_t t[256];
  void* r;

  while ((ch = getopt(argc,argv,"p:n:atlx:")) != -1) {
    switch (ch) {
    case 'n': n = atoi(optarg); break;
    case 'p': p = atoi(optarg); break;
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

  if (256 <= p) p = 256;
  for (i = 0; i < p; i++) pthread_create(&t[i],0,task,(void*)n);
  for (i = 0; i < p; i++) pthread_join(t[i],&r);
  pthread_create(&t[0],0,validate,0);
  pthread_join(t[0],&r);
  return 0;
}
