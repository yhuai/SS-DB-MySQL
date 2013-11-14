#!/bin/bash
# script for running all queries on 10 nodes.
# assumes 1TB (normal) dataset
# run this on farm10. also, this assumes no-pass ssh and NOPASSWD sudoer
# (ssh-key should be generated and put in right place in ahead)

# for distributed test
run_query_rep_dist() {
  for rep in 1 2 3
  do
    echo "clearing caches in each node..."
    for nodenum in 5 6 7 8 9 10 11 12 13 14
    do
      node="farm$nodenum"
      echo "clearing cache on $node"
      ssh $node "sudo /etc/init.d/mysql stop;sudo sync;sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches';sleep 1;sudo /etc/init.d/mysql start"
    done
    echo "cleared caches."
    ./scidb_rdb "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
  done
}

# for master-node-only test
run_query_rep() {
  for rep in 1 2 3
  do
    sudo /etc/init.d/mysql stop
    sudo sync
    sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
    echo "Dropped cache"
    sleep 3
    sudo /etc/init.d/mysql start
    sleep 3
    ./scidb_rdb "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
  done
}

# q3 is simply exec per node (not a 'real' implementation, duh)
run_query_rep_q3() {
  for rep in 1 2 3
  do
    echo "running q3..."
    for nodenum in 5 6 7 8 9 10 11 12 13 14
    do
      node="farm$nodenum"
      let nodeid=$nodenum%10
      echo "running q3 on $node (nodeid=$nodeid)..."
      ssh $node "sudo /etc/init.d/mysql stop;sudo sync;sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches';sleep 1;sudo /etc/init.d/mysql start;sleep 2;cd scidb_rdb;./scidb_rdb q3allparallel 2 10 $nodeid" &
    done
    echo "launched q3 on each node. waiting for their ends..."
    sleep 1000
    echo "okay, they SHOULD finish. check the log later"
  done
}

XSTARTS=(503000 504000 500000 504000 504000)
YSTARTS=(503000 491000 504000 501000 493000)
for ind in 0 1 2 3 4
do
#  run_query_rep_dist "q1allparallel" 2
  run_query_rep "q2" 900 20
#  run_query_rep_q3
  run_query_rep "q4" 5 $XSTARTS[$ind] 10000 $YSTARTS[$ind] 10000 
  run_query_rep "q5" 5 $XSTARTS[$ind] 10000 $YSTARTS[$ind] 10000
  run_query_rep "q6new" 5 100 20 $XSTARTS[$ind] 10000 $YSTARTS[$ind] 10000
  run_query_rep "q7" $XSTARTS[$ind] 10000 $YSTARTS[$ind] 10000
#  run_query_rep_dist "q8parallel" $XSTARTS[$ind] 1000 $YSTARTS[$ind] 1000 50
#  run_query_rep_dist "q9parallel" 100 $XSTARTS[$ind] 1000 $YSTARTS[$ind] 1000 50
done