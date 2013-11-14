#!/bin/sh
# script for running q2.

run_query() {
  sudo /etc/init.d/mysql stop
  sudo sync
  sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
  echo "Dropped cache"
  sleep 3
  sudo /etc/init.d/mysql start
  sleep 3
  ./scidb_rdb q2 800 20
}

for rep in 1 2 3
do
  run_query
done
