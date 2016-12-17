#include <errno.h>
#include "udp_test.h"



/* 
 * 处理udp的定时任务
*/
void udp_timeout_callback_fn(evutil_socket_t fd, short event, void *arg) {
    UDP_Test_event *p_tev = (UDP_Test_event *)arg;
    UDP_test *p_test = (UDP_test *)p_tev->get_test();
    if (event & EV_TIMEOUT) {
        printf("upd test %d timeout\n", p_test->get_id());
    }
    else {
        printf("udp test %d receive data\n", p_test->get_id());
    }

    // 删除worker的icmp_unreach_map中对应的检测
    p_test->unregister_event();

    p_test->finish();
    event_del(p_tev->get_event());
    delete p_tev;
    return;
}


UDP_test::UDP_test(Worker *w) {
    worker = w;
    pid = getpid();
}

int UDP_test::config(Config *conf) {
    id = conf->get_id();
    host = conf->get_host();
    port = conf->get_port();
    timeout = conf->get_timeout();
    if (timeout <= 0) {
        timeout = -1;
    }
    frenc = conf->get_frenc();
    if (frenc <= 0) {
        frenc = -1;
    }
    name = conf->get_type();
    return 0;
}

int UDP_test::run() {
    int result;
    starttime = time(NULL);
    result = init();
    if (result == TEST_STATUS_FAIL) {
        finish();
    }
    printf("timeout is %d\n", timeout);
    if (timeout > 0) {
        // 设置超时，即检测timeout之后，是否收到了icmp的端口不可达报文
        UDP_Test_event *p_tev = new UDP_Test_event(this);
        p_tev->set_mark(false);    // mark 为false代表着到了时间，应该发起probe了
        // 设置超时和对sockfd的读响应。sockfd的读响应是因为有可能要和服务端返回的内容比对
        printf("event_assign sockfd is %d\n", sockfd);
        event_assign(p_tev->get_event(), worker->get_base(), sockfd, EV_READ, udp_timeout_callback_fn, (void *)p_tev);
        struct timeval *p_temp_tv;
        p_temp_tv = p_tev->get_timeout();
        p_temp_tv->tv_sec = timeout / 1000;
        p_temp_tv->tv_usec = (timeout % 1000) * 1000;
        event_add(p_tev->get_event(), p_temp_tv);
        printf("%d -------------add event %d %d\n", getpid(), p_temp_tv->tv_sec, p_temp_tv->tv_usec);

        // 将这个test event和目的地址对应起来
        register_event(p_tev);
        printf("register event\n");
    }
    result = probe();

    return TEST_STATUS_OK;
}

/*
 * 建立好socket，生成好相应的二进制的地址
*/
int UDP_test::init() {
    // 生成socket fd
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    printf("in init sockfd is %d\n", sockfd);
    if (sockfd < 0) {
        printf("init udp socket fd error: %s\n", strerror(errno));
        return TEST_STATUS_FAIL;
    }
    evutil_make_socket_nonblocking(sockfd);
    // 得到目的地址对应的二进制地址
    //printf(">>>>>>addr is %s, port is %d\n", host.c_str(), port);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, host.c_str(), &servaddr.sin_addr);
    servaddr.sin_port = htons(port);
    //int res = connect(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    status = TEST_STATUS_INIT; 
    success = false;
    return status;
}

/*
 * 发送udp命令
*/
int UDP_test::probe() {
    const char *buf = "";   // 这里可以改成从配置中读取要发送的内容，而不是一个空字符串
    //printf("=====%d send data to server\t", getpid());
    //printf("addr is %s, port is %d\n", host.c_str(), port);
    int res = sendto(sockfd, buf, strlen(buf)+1, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (res < 0) {
        printf("send udp data error: %s\n", strerror(errno));
    }
    starttime = time(NULL);
    status = TEST_STATUS_PROBE;
    printf("%d end probe fd is %d\n", getpid(), sockfd);
    return status;
}

/*
 * 接收回来的数据
*/
int UDP_test::receive() {
    char buf[1024];
    bzero(buf, 1024);
    struct sockaddr src_addr;
    socklen_t src_addrlen;
    recvfrom(sockfd, buf, 1024, 0, &src_addr, &src_addrlen);
    status = TEST_STATUS_RECV;
    return status;
}

/*
 * 处理接收回来的数据
*/
int UDP_test::process() {

}

/*
 * 回收探测资源，做收尾处理工作
 * 计算探测结果
 * 将探测的结果发送到相应的地方
*/
int UDP_test::finish() {
    donetime = time(NULL);
    // 回收资源
    close(sockfd);
    status = TEST_STATUS_DONE;
    // 记录日志
    char logbuf[256];
    bzero(logbuf, 256);
    sprintf(logbuf, "udp service %s:%d is ok", host.c_str(), port);
    logger->info(logbuf);
    // 将数据写入到redis中
    Json::Value metric;
    metric["host"] = host;
    metric["port"] = port;
    metric["status"] = 1;
    Json::FastWriter fast_writer;
    std::string metric_str = fast_writer.write(metric);
    sendresult(metric_str);

    return status;
}

int UDP_test::finish(std::string &addr_str) {
    donetime = time(NULL);
    // 回收资源
    close(sockfd);
    status = TEST_STATUS_DONE;
    // 记录日志
    char logbuf[256];
    bzero(logbuf, 256);
    sprintf(logbuf, "udp service %s is unreachable", addr_str.c_str());
    logger->info(logbuf);
    // 将数据写入到redis中
    Json::Value metric;
    metric["host"] = host;
    metric["port"] = port;
    metric["status"] = 0;
    Json::FastWriter fast_writer;
    std::string metric_str = fast_writer.write(metric);
    sendresult(metric_str);

    return status;
}

struct event *UDP_test::get_test_ev() {
    return &test_ev;
}

struct sockaddr_in *UDP_test::get_servaddr() {
    return &servaddr;
}

void UDP_test::register_event(Test_event *p_tev) {
    worker->set_icmp_unreach_test_event(&servaddr, p_tev);    
}

void UDP_test::unregister_event() {
    worker->del_icmp_unreach_test_event(&servaddr); 
}

UDP_test::~UDP_test() {
    struct event_base *base = worker->get_base();
    event_del(&test_ev);

}

///////////////////////////////////////////////////

bool UDP_Test_event::get_mark() {
    return mark;
}

void UDP_Test_event::set_mark(bool m) {
    mark = m;
    return;
}

