#! /bin/sh

invoke() {
  proc=$1; shift; p=$1; shift; n=$1; shift;
  time -p $proc -p$p -n$n $@ 2>&1| \
  awk '
  BEGIN {
    secs=0.0; start=0; abort=0; total='$p'*'$n';
  }
  /real (\.[0-9]|[0-9][0-9]*\.)/ {
    split($0,a," "); secs=a[2];
  }
  /Starts=[0-9][0-9]*/ {
    split($0,a," "); split(a[2],x,"="); split(a[3],y,"=");
    start=x[2]; abort=y[2];
  }
  END {
    trans=start-abort; lock=total-trans;
    printf("%.3f,%d,%d\n",secs,trans,lock);
  }'
}


bench() {
  proc="./test1"; n=100000; r=3
  for m in "-t" "-l" "-a"; do
    for p in 1 2 4 8 16 32 64 128; do
      i=1;
      while expr $i \<= $r >/dev/null; do
        invoke $proc $p $n $m; i=`expr $i + 1`
      done | \
      sort -n | \
      while read l; do
        printf "%s," $l;
      done
      printf "\"%s\",%d,%d,\"%s\"\n" $proc $p $n $m
    done
  done
}

bench | tee -a bench.dat
