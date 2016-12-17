#include <async.h>
//#include <adapters/libevent.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <map>
#include <string>
#include <event2/event.h>
#include <event2/util.h>
#include <event2/event_struct.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <json/json.h>
#include <hiredis.h>
#include "configuration.h"
#include "logger.h"
#include "tool.h"



#ifndef NEWPROBER_WORKER_H
#define NEWPROBER_WORKER_H

#define FILE_MODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

#ifndef NEWPROBER_MASTER_H
#include "master.h"
#endif

#ifndef NEWPROBER_PING_TEST_H
#include "ping_test.h"
#endif

#ifndef NEWPROBER_UDP_TEST_H
#include "udp_test.h"
#endif

class Ping_test;
class UDP_test;



class Worker {
  public:
    Worker();
    void run();
    void add_test(Test *p_test);
    struct event_base *get_base();
    redisAsyncContext *get_redis();
    void set_channel(int channel);
    void set_channel_1(int channel);
    void listen_icmp();
    void set_logger(Logger *l);
    Logger *get_logger();
    void split_log(int newfd);
    void deal_channel_data(struct bufferevent *bev);
    int getfifo();
    Test_event *get_icmp_unreach_test_event(struct sockaddr_in *);
    Test_event *get_icmp_unreach_test_event(std::string &);
    void set_icmp_unreach_test_event(struct sockaddr_in*, Test_event *);
    void del_icmp_unreach_test_event(struct sockaddr_in*);
  private:
    std::map<int, Test *> test_map;   // test id和test的指针的对应关系
    //std::map<int, Test *> run_test_map;   // test id和test的指针的对应关系，这里是加到了base里的test
    pid_t pid;
    int pipefd;
    int testnum;
    struct event_base *base;
    struct bufferevent *channelevent;
    struct event *channel_1_event;
    struct event *icmp_event;
    char *buf;
    int channel;    // 和master进程通讯的channel对应的fd
    int channel_1;    // 用来传递日志文件描述符的channel, 是1,2,3的1，不是小写的L
    int icmpsock;    // icmp监听的socket fd
    std::map<std::string, Test_event *> icmp_unreach_map;    // 用来存放icmp不可达的时候，对应的不可达地址和相应的test event的对应关系，key是对应的不可达的地址+端口号，如10.88.15.46:9002
    Logger *logger;
    std::string fifopath;
    int fifofd;
    int redisport;
    redisAsyncContext *redisc;
    void openfifo();
    void openredis();
    Test *generate_test(Json::Value &root);  // 根据json配置里面的内容，生成相应的test，并且运行，如果失败返回-1，成功返回配置的id
    int delete_test(Json::Value &root);  // 删除root里面id对应的配置的test
};


#endif
