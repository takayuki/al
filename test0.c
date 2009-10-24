#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "alx.h"
#include "queue.h"

void
help(void)
{
  fprintf(stderr,
          "usage: test [-hnpx]\n"
          "  -p  number of threads (default: 2)\n"
          "  -n  number of repeats (default: 1000000)\n"
          "  -l  use lock only\n"
          "  -t  use transaction only\n"
          "  -x  transactional overhead (default: 25)\n"
          "  -h  show this\n");
  exit(0);
}

long cnt[3] = {0,0,0};

__attribute__((atomic ("l1")))
void
empty(void)
{
}

void*
task(void* arg)
{
  int n = (int)arg;

  while (n--) empty();
  return 0;
}

int
main(int argc,char* argv[])
{
  int p = 2,n = 1000000,ch,i;
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
  return 0;
}
