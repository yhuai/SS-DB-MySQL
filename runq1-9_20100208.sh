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
# run_query_rep "q1all" ""
# run_query_rep "q3all" ""
 run_query_rep "q4"
 run_query_rep "q5"
# run_query_rep "q6"
# run_query_rep "q7"
# run_query_rep "q8"
# run_query_rep "q9"
