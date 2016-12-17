#include <map>
#include <vector>
#include <unistd.h>
#include <signal.h>
#include <event2/event.h> 
#include <event2/util.h>  
#include <event2/event_struct.h>
#include <event2/http.h>
#include <async.h>
#include <adapters/libevent.h>
#include <hiredis.h>
#include "tool.h"
#include "configuration.h"
#include "logger.h"


#ifndef NEWPROBER_MASTER_H
#define NEWPROBER_MASTER_H


#ifndef NEWPROBER_WORKER_H
#include "worker.h"
#endif

class Worker;

#define MASTER_MSG_TYPE_ADDCONF 0
#define MASTER_MSG_TYPE_DELCONF 1
#define MASTER_MSG_TYPE_EDITCONF 2
#define MASTER_MSG_TYPE_STARTTEST 3
#define MASTER_MSG_TYPE_STOPTEST 4
#define MASTER_MSG_TYPE_FIFOPATH 5
#define MASTER_MSG_TYPE_REDISPORT 6

#define MAX_CONFFILE_LEN 128

class Master {
  public:
    Master();
    ~Master();
    int init();
    int get_server_conf(const char *filepath);
    int get_test_conf(const char *filepath);
    void set_logger(Logger *l);
    Logger *get_logger();
    int generate_worker();
    int will_distribute_conf(int delay);
    int distribute_conf();
    void flush_test_conf();    // 将配置冲回到配置文件中
    int run();
    struct event_base *get_base();
    void deal_channel_data(struct bufferevent *bev);  
    void will_split_log();    // 关闭目前的日志，生成一个新的日志的文件描述符，将这个描述符传递给子进程
    void split_log();    // 关闭目前的日志，生成一个新的日志的文件描述符，将这个描述符传递给子进程
    void deal_metric_request(struct evhttp_request *req);
    void delete_metric_request(struct evhttp_request *req);

  private:
    int workernum;  // worker的数目
    int port;   // 监听的端口号
    Logger *logger;
    struct event_base *base; 
    struct evhttp *http;
    struct evhttp_bound_socket *handle;
    struct event *distribute_conf_ev;
    struct event *split_log_ev;
    struct event *flush_conf_ev;
    
    int splitlog_interval;
    int flushconf_interval;
    std::string fifopath;
    int redisport;
    char test_conf_file[MAX_CONFFILE_LEN];
    char server_conf_file[MAX_CONFFILE_LEN];

    std::vector<pid_t> workerpid_vec;   // 存放worker pid的vector

    std::map<int, std::string> configid_map;  // 存放检测配置的json字符串的map, int是配置的id
    std::map<int, std::vector<std::map<int, std::string>::iterator> > config_map;  // 值是对应的配置在configid_map里面的iterator, int是对应worker的channel的fd
    std::map<int, int> configid_channel_map;   // 存放配置的id和channel的fd的对应关系

    std::vector<int> channel_fd_vec;   // 存放和子进程通讯的socket fd的vector
    std::vector<int> channel_1_fd_vec;   // 存放和子进程通讯的用来传递文件描述符的socket fd的vector
    std::map<int, pid_t> channel_pid_map;   // 存放和子进程通讯的channel的fd和子进程的pid的对应关系
    std::vector<struct bufferevent *> channelevent_vec;

    int send_msg(int channel, int type, std::string &msg);   // 往channel为fd的子进程发送类型为type的信息msg
    void send_cmd(int channel, int sendfd);
    void recover_resource();

};


#endif
