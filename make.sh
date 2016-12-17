#!/bin/bash 

#g++ -c -g configuration.cpp -o configuration.o
#echo "========================================"
#g++ -c -g ping_test.cpp -o ping_test.o
#echo "========================================"
##g++ -c -g worker.cpp -o ping_test.o

# online
g++ -g main.cpp logger.cpp master.cpp test.cpp tool.cpp ping_test.cpp udp_test.cpp worker.cpp  configuration.cpp  /usr/local/lib/libevent.a /usr/local/lib/libevent_core.a /usr/local/lib/libevent_core.a  /usr/local/lib/libevent_openssl.a /usr/local/lib/libevent_pthreads.a /usr/local/lib/libjsoncpp.a /usr/local/lib/libhiredis.a -lrt -I /usr/local/include/ -I /usr/local/include/hiredis -o newprober

# old online
#g++ -g main.cpp logger.cpp master.cpp test.cpp tool.cpp ping_test.cpp worker.cpp  configuration.cpp  /usr/local/lib/libevent.a /usr/local/lib/libevent_core.a /usr/local/lib/libevent_core.a  /usr/local/lib/libevent_openssl.a /usr/local/lib/libevent_pthreads.a /usr/local/lib/libjsoncpp.a /usr/local/lib/libhiredis.a -lrt -I /usr/local/include/ -I /usr/local/include/hiredis -o newprober

# for test
#g++ -g main.cpp logger.cpp master.cpp test.cpp tool.cpp ping_test.cpp udp_test.cpp worker.cpp  configuration.cpp  /home/guangtong/libevent/lib/libevent.a /home/guangtong/libevent/lib/libevent_core.a /home/guangtong/libevent/lib/libevent_core.a  /home/guangtong/libevent/lib/libevent_openssl.a /home/guangtong/libevent/lib/libevent_pthreads.a /usr/local/lib/libjsoncpp.a /usr/local/lib/libhiredis.a -lrt -I /home/guangtong/libevent/include -I /usr/local/include/ -I /usr/local/include/hiredis -o newprober
