#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "alx.h"

void
help(void)
{
  fprintf(stderr,
          "usage: null [-hnpx]\n"
          "  -p  number of threads (default: 2)\n"
          "  -n  number of repeats (default: 1000000)\n"
          "  -a  use adaptive lock (default)\n"
          "  -l  use lock only\n"
          "  -t  use transaction only\n"
          "  -x  transactional overhead (default: 5.0)\n"
          "  -f  power number of locks (default: 0(single lock))\n"
          "  -h  show this\n");
  exit(0);
}

#define MAXLOCKS	1024

static al_t locks[MAXLOCKS];
static unsigned long NLOCKS = 1;
static unsigned long LOCKMASK = 0;

__attribute__((atomic))
void
atomic_empty(al_t* lock)
{
}

void
empty(unsigned long key)
{
  atomic_empty(&locks[(key & LOCKMASK)]);
}

static int thrd = 2;
static int iter = 100000;

void*
task(void* arg)
{
  unsigned short id = (unsigned short)(unsigned long)arg;
  long n = iter;
  unsigned long seed = id;

  while (n--) {
    empty(Random(&seed));
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
static pthread_barrier_t invokeBarrier;

void*
invoke(void* arg)
{
  void* ret;

  pthread_barrier_wait(&invokeBarrier);
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

  while ((ch = getopt(argc,argv,"p:n:altx:f:h")) != -1) {
    switch (ch) {
    case 'p': thrd = atoi(optarg); break;
    case 'n': iter = atoi(optarg); break;
    case 'a': setAdaptMode(0); break;
    case 'l': setAdaptMode(-1); break;
    case 't': setAdaptMode(1); break;
    case 'x': setTranxOvhd(atof(optarg)); break;
    case 'f': NLOCKS = 1<<atoi(optarg); LOCKMASK = NLOCKS-1; break;
    case 'h':
    default: help();
    }
  }
  argc -= optind;
  argv += optind;

  if (MAXLOCKS < NLOCKS) { NLOCKS = MAXLOCKS; LOCKMASK = NLOCKS-1; }
  if (256 <= thrd) thrd = 256;
  pthread_barrier_init(&invokeBarrier,0,thrd);
  for (i = 0; i < thrd; i++) pthread_create(&t[i],0,invoke,i+1);
  for (i = 0; i < thrd; i++) pthread_join(t[i],&r);
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
