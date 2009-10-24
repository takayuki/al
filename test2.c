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
          "  -l  use lock only\n"
          "  -t  use transaction only\n"
          "  -x  transactional overhead (default: 25)\n"
          "  -h  show this\n");
  exit(0);
}

LIST_HEAD(head,list_entry) tab = LIST_HEAD_INITIALIZER(tab);

struct list_entry {
  long key;
  long val;
  LIST_ENTRY(list_entry) links;
};

__attribute__((atomic ("l1")))
void
add(long n)
{
  struct list_entry *p,*q,*r;
  
  assert((r = (struct list_entry*)malloc(sizeof(*r))));
  r->key = n;
  r->val = n+1;
  q = 0;
  LIST_FOREACH(p,&tab,links) {
    if (n == p->key) {
      free(r);
      break;
    }
    if (n < p->key) {
      LIST_INSERT_BEFORE(p,r,links);
      break;
    }
    q = p;
  }
  if (p == 0) {
    if (q == 0)
      LIST_INSERT_HEAD(&tab,r,links);
    else
      LIST_INSERT_AFTER(q,r,links);
  }
  return;
}

__attribute__((atomic ("l1")))
int
del(long n)
{
  struct list_entry *p;
  
  LIST_FOREACH(p,&tab,links) {
    if (n == p->key) {
      LIST_REMOVE(p,links);
      free(p);
      return 1;
    }
  }
  return 0;
}

__attribute__((atomic ("l1","ro")))
int
lookup(long n,struct list_entry* ret)
{
  struct list_entry *p;
  
  LIST_FOREACH(p,&tab,links) {
    if (n == p->key) {
      memcpy(ret,p,sizeof(*p));
      return 1;
    }
  }
  return 0;
}

void*
task(void* arg)
{
  long n = (int)arg;
  long t;
  struct list_entry ret;

  while (n--) {
    t = random() % 1000;
    switch(n%4) {
    case 0: add(t); break;
    case 1: del(t); break;
    default: lookup(t,&ret); break;
    }
  }
  return 0;
}

int
main(int argc,char* argv[])
{
  int p = 2,n = 100,ch,i;
  pthread_t t[256];
  void* r;

  while ((ch = getopt(argc,argv,"p:n:ltx:")) != -1) {
    switch (ch) {
    case 'n': n = atoi(optarg); break;
    case 'p': p = atoi(optarg); break;
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
  {
    struct list_entry *p;
    long a = 0,b = 0,c = 0;

    LIST_FOREACH(p,&tab,links) {
      b = p->key;
      assert(a <= b);
      a = b; c++;
      printf("%ld\n",a);
    }
    printf("number of elements=%ld\n",c);
  }
  return 0;
}
