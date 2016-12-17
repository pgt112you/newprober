#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <string>
#include <vector>
#include <sys/time.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include "configuration.h"
#include "test.h"



#ifndef NEWPROBER_PING_TEST_H
#define NEWPROBER_PING_TEST_H

#ifndef NEWPROBER_WORKER_H
#include "worker.h"
#endif


#define TEST_STATUS_OK 0  
// 初始化
#define TEST_STATUS_INIT 1  
// 发送探测
#define TEST_STATUS_PROBE 2
// 接收数据
#define TEST_STATUS_RECV 3
// 处理接收的数据
#define TEST_STATUS_PROCESS 4
// 还不是最后一次检测了
#define TEST_STATUS_NOLAST 5
// 已经是最后一次检测了
#define TEST_STATUS_LAST 6
// 所有检测完毕
#define TEST_STATUS_DONE 7
// 检测失败
#define TEST_STATUS_FAIL 8



struct Ping_data {
    unsigned int ttl;
    double rtt;
    int datalen;
    unsigned int icmpseq;
    char hostip[INET_ADDRSTRLEN];
};


class Worker;

class Ping_test : public Test {
  public:
    Ping_test(Worker *w);
    int config(Config *conf);
    int init();
    int run();
    int probe();
    int receive();
    int process();
    int finish();
    struct event *get_test_ev();
    //int get_frenc();
    int get_nowtimes();
    int get_interval();
    int get_times();
    ~Ping_test();
    int sockfd;   // 暂时移到public里面，测试用

  private:
    std::string host;
    int times;    // 一次探测中发送ping命令的次数
    int nowtimes;    // 在一次探测中目前已经发送了几次ping命令了
    int nowrecvtimes;    // 目前已经收到了多少次ping的探测结果了
    int ttl;
    //int frenc;    // 检测频率，在Test类里
    struct timeval sendtime;
    struct timeval recvtime;
    //Worker *worker;   // 在Test类里
    //int sockfd;   // 暂时移到public里面，测试用
    struct addrinfo *servaddr;
    int status;
    char sendbuf[1500];
    char recvbuf[1500];
    char controlbuf[1500];
    int nsent;
    time_t starttime;    // 本次探测开始的时间戳
    time_t donetime;    // 本次探测结束的时间戳
    std::vector<struct Ping_data> data_vec;
    pid_t pid;
    int recvlen;
    //struct event test_ev;
    int timeout;    // 每次ping的超时时间   单位是ms
    int interval;   // 每两次ping之间的时间间隔 单位是ms 如果这个值小于0，就是下一次的ping在这一次ping的数据回来之后，或者这一次的ping超时了，再发送
};

/* 
 * 存放具体test类型的指针和相应的event对象的类
*/
class Ping_Test_event : public Test_event {
  public:
    Ping_Test_event(Ping_test *test) : Test_event(test) {};
    bool get_mark();
    void set_mark(bool m);

  private:
    bool mark;   // 标识字段，比如表示是ping是超时了，还是到时间进行下一次检测了
};


#endif
