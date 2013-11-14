#!/bin/sh

run_query() {
        sudo /etc/init.d/mysql stop
        sudo sync
        sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
        echo "Dropped cache"
        sleep 3
        sudo /etc/init.d/mysql start
        sleep 3
        ./scidb_rdb "$1" "$2"
}

run_query_rep() {
  run_query "$1" "$2"
  run_query "$1" "$2"
  run_query "$1" "$2"
}
run_query_rep "q1all" ""
run_query_rep "load" "5600"
run_query_rep "load" "5800"
run_query_rep "load" "6000"
run_query_rep "q3all" ""
