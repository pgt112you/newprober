#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
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



#ifndef NEWPROBER_UDP_TEST_H
#define NEWPROBER_UDP_TEST_H

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



struct UDP_data {
    unsigned int ttl;
    double rtt;
    int datalen;
    unsigned int icmpseq;
    char hostip[INET_ADDRSTRLEN];
};


class Worker;

class UDP_test : public Test {
  public:
    UDP_test(Worker *w);
    int config(Config *conf);
    int init();
    int run();
    int probe();
    int receive();
    int process();
    int finish();
    int finish(std::string &);
    struct event *get_test_ev();
    //int get_frenc();
    int get_nowtimes();
    int get_interval();
    int get_times();
    struct sockaddr_in *get_servaddr();
    void register_event(Test_event *p_tev);   // 向worker的icmp_unreach_map里面注册检测地址对应的test_event
    void unregister_event();
    ~UDP_test();
    int sockfd;   // 暂时移到public里面，测试用

  private:
    bool success;    // udp端口是否存活的标识
    std::string host;
    int port;
    int ttl;
    //int frenc;    // 检测频率，在Test类里
    struct timeval sendtime;
    struct timeval recvtime;
    //Worker *worker;   // 在Test类里
    //int sockfd;   // 暂时移到public里面，测试用
    struct sockaddr_in servaddr;
    int status;
    char sendbuf[1500];
    char recvbuf[1500];
    char controlbuf[1500];
    int nsent;
    time_t starttime;    // 本次探测开始的时间戳
    time_t donetime;    // 本次探测结束的时间戳
    std::vector<struct UDP_data> data_vec;
    pid_t pid;
    int recvlen;
    //struct event test_ev;
    int timeout;    // 每次udp的超时时间   单位是ms
    int interval;   // 每两次udp之间的时间间隔 单位是ms 如果这个值小于0，就是下一次的udp在这一次udp的数据回来之后，或者这一次的udp超时了，再发送
};

/* 
 * 存放具体test类型的指针和相应的event对象的类
*/
class UDP_Test_event : public Test_event {
  public:
    UDP_Test_event(UDP_test *test) : Test_event(test) {};
    bool get_mark();
    void set_mark(bool m);

  private:
    bool mark;   // 标识字段，比如表示是udp是超时了，还是到时间进行下一次检测了
};


#endif
