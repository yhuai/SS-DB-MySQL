#!/bin/sh
# script for running all queries on one node.
# assumes 100GB (small) dataset

run_query() {
  sudo /etc/init.d/mysql stop
  sudo sync
  sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
  echo "Dropped cache"
  sleep 3
  sudo /etc/init.d/mysql start
  sleep 3
  ./scidb_rdb "$1" "$2" "$3" "$4" "$5" "$6" "$7"
}

run_query_rep() {
  for rep in 1 2 3
  do
    run_query "$1" "$2" "$3" "$4" "$5" "$6" "$7"
  done
}
run_query_rep "q1all" 2
run_query_rep "q3all" 2
run_query_rep "q4" 5 503000 5000 503000 5000 
run_query_rep "q5" 5 503000 5000 503000 5000
run_query_rep "q6" 5 100 20
run_query_rep "q7" 503000 5000 503000 5000
run_query_rep "q8" 503000 1000 503000 1000 50
run_query_rep "q9" 100 503000 1000 503000 1000 50
