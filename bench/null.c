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
#include <unistd.h>
#include "alx.h"
#include "random.h"

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
          "  -z  number of instr. in transactional load (default: 50)\n"
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

  if (MAXLOCKS < NLOCKS) { NLOCKS = MAXLOCKS; LOCKMASK = NLOCKS-1; }
  if (256 <= thrd) thrd = 256;
#ifdef HAVE_PTHREAD_BARRIER
  pthread_barrier_init(&invokeBarrier,0,thrd);
#endif
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
