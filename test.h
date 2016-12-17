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
#include "logger.h"
#include "worker.h"



#ifndef NEWPROBER_TEST_H
#define NEWPROBER_TEST_H


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

class Worker;


class Test {
  public:
    virtual int config(Config *conf) {};
    virtual int init() {};
    virtual int run() {};
    virtual int probe() {};
    virtual int receive() {};
    virtual int process() {};
    virtual int finish() {};
    virtual int finish(std::string &) {};
    virtual int sendresult(std::string &content);
    virtual struct event *get_test_ev() {};
    virtual void set_id(int i);
    virtual int get_id();
    virtual void set_name(std::string n);
    virtual std::string get_name();
    virtual void set_frenc(int f); 
    virtual int get_frenc(); 
    virtual Logger *get_logger();
    virtual void set_logger(Logger *l);

  //private:
  protected:
    int id;    // 一个test的标示符
    int frenc;    // 检测频率
    std::string name;    // 测试的名称，即config的type
    Logger *logger;
    Worker *worker;
    struct event test_ev;

};

class Test_event {
  public:
    Test_event(Test *test);
    Test *get_test();
    struct event *get_event();
    struct timeval *get_timeout();

  private:
    Test *p_test;   // 这里以后改成基类Test的指针
    struct event ev;
    struct timeval timeout;
};


#endif
