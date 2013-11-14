#!/bin/bash
# script for running all queries on one node.
# assumes 100GB (small) dataset
# 5 different parameters * 3 executions each = 15 runs per query

run_query() {
  sudo /etc/init.d/mysql stop
  sudo sync
  sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
  echo "Dropped cache"
  sleep 3
  sudo /etc/init.d/mysql start
  sleep 3
  ./scidb_rdb "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
}

run_query_rep() {
  for rep in 1 2 3
  do
    run_query "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
  done
}

XSTARTS=(503000 504000 500000 504000 504000)
YSTARTS=(503000 491000 504000 501000 493000)
for ind in 0 1 2 3 4
do
  run_query_rep "q1all" 2
  run_query_rep "q2" 900 20
  run_query_rep "q3all" 2
  run_query_rep "q4" 5 $XSTARTS[$ind] 10000 $YSTARTS[$ind] 10000 
  run_query_rep "q5" 5 $XSTARTS[$ind] 10000 $YSTARTS[$ind] 10000
  run_query_rep "q6new" 5 100 20 $XSTARTS[$ind] 10000 $YSTARTS[$ind] 10000
  run_query_rep "q7" $XSTARTS[$ind] 10000 $YSTARTS[$ind] 10000
  run_query_rep "q8" $XSTARTS[$ind] 1000 $YSTARTS[$ind] 1000 50
  run_query_rep "q9" 100 $XSTARTS[$ind] 1000 $YSTARTS[$ind] 1000 50
done