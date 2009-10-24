#! /bin/sh

LD_PRELOAD="./hoard/src/libhoard.so:/usr/lib/libCrun.so.1"
export LD_PRELOAD

invoke() {
  proc=$1; shift; p=$1; shift; n=$1; shift;
  $proc -p$p -n$n $@ 2>&1| \
  awk '
  BEGIN {
    exec='$p'*'$n'; exec_per_sec=0.0; start=0; abort=0;
  }
  /elapse=(\.[0-9]|[0-9][0-9]*\.)/ {
    split($0,a,","); split(a[2],x,"=");
    exec_per_sec=x[2];
  }
  /Starts=[0-9][0-9]*/ {
    split($0,a," "); split(a[2],x,"="); split(a[3],y,"=");
    start=x[2]; abort=y[2];
  }
  END {
    trans=start-abort; lock=exec-trans;
    printf("%.3f,%d,%d\n",exec_per_sec,trans,lock);
  }'
}

bench() {
  proc="./test2"; n=100000; r=3
  #proc="./test3"; n=100000; r=3
  #proc="./test4"; n=10000; r=3
  for m in "-t" "-l" "-a"; do
    for p in 1 2 4 8 16 32 64; do
      i=1;
      while expr $i \<= $r >/dev/null; do
        invoke $proc $p $n $m; i=`expr $i + 1`
      done | \
      sort -n | head -2 | tail -1 | \
      while read l; do
        printf "%s," $l;
      done
      printf "\"%s\",%d,%d,\"%s\"\n" $proc $p $n $m
    done
  done
}

bench | tee -a bench.dat
