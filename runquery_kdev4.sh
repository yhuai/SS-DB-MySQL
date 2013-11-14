#!/bin/sh

run_query() {
        sudo /etc/init.d/mysql stop
        sudo sync
        sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
        echo "Dropped cache"
        sleep 3
        sudo /etc/init.d/mysql start
        sleep 3
        ./build/scidb_rdb "$1"
}

run_query_rep() {
  run_query "$1"
  run_query "$1"
  run_query "$1"
}
run_query_rep "q1all"
run_query_rep "q1"
run_query_rep "q1-100"
# run_query_rep "q3"
run_query_rep "q3nfs"
