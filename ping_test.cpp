#include <errno.h>
#include <math.h>
#include "ping_test.h"
#include "tool.h"



/* 
 * 处理ping的定时任务
*/
void ping_timeout_callback_fn(evutil_socket_t fd, short event, void *arg) {
    Ping_Test_event *p_tev = (Ping_Test_event *)arg;
    Ping_test *p_test = (Ping_test *)p_tev->get_test();
    int result;
    if ((p_test->get_nowtimes()) >=  p_test->get_times()) {   // 检测完成
        p_test->finish();
        event_del(p_tev->get_event());
        delete p_tev;
        return;
    }
    p_test->probe();   // 进行下一次的ping
    return;
}

/* 
 * 如果ping在最后一个ping的时候，超时可能出问题，导致资源未回收. 
 * 在接收到数据，并且ping还没超时的时候，要把超时的那个timeout的event给删了
 * 在超时了，还没收到数据的时候，要把收数据的那个event给删了
*/

void ping_socket_callback_fn(evutil_socket_t fd, short event, void *arg) {
    Ping_Test_event *p_tev = (Ping_Test_event *)arg;
    Ping_test *p_test = (Ping_test *)p_tev->get_test();
    int result;
    if (event & EV_TIMEOUT) {
        if ((p_test->get_nowtimes()) >=  p_test->get_times()) {   // 检测完成
            event_del(p_tev->get_event());
            p_test->finish();
            delete p_tev;
            return;
        }
        else {
            delete p_tev;
            // 如果此test没设置interval，就手动发起下一次的探测
            if (p_test->get_interval() <= 0) {
                p_test->probe();
            }
            return;
        }
    }
    if (event & EV_READ) { 
        // 某个test发出的请求有响应了                          
        delete p_tev;
        result = p_test->receive();  // 接收数据               
        if (result == TEST_STATUS_FAIL) {
            p_test->finish();
            return;
        }   
        result = p_test->process();  // 处理数据
        if (result == TEST_STATUS_NOLAST) {
            // 这一轮的test还没执行完，比如ping要求ping4次，这是第2次的响应，当没有设置interval的时候会走到这里                                                        
            p_test->probe();
        }
        else if (result == TEST_STATUS_PROCESS) {
            ;// 这一轮的test还没执行完，比如ping要求ping4次，这是第2次的响应                                                        
        }
        else if (result == TEST_STATUS_LAST) {                 
            // 这一轮的test已经执行完了                        
            p_test->finish();  // 结束处理                     
        }   
        return;
    }       

}


Ping_test::Ping_test(Worker *w) {
    worker = w;
    servaddr = NULL;
    nowtimes = 0;
    pid = getpid();
}

int Ping_test::config(Config *conf) {
    times = conf->get_times();
    if (times <= 0)
        times = 4;
    ttl = conf->get_ttl();
    if (ttl <= 0)
        ttl = 64;
    host = conf->get_host();
    frenc = conf->get_frenc();
    timeout = conf->get_timeout();
    if (timeout <= 0) {
        timeout = -1;
    }
    interval = conf->get_interval();
    if (interval <= 0) {
        interval = -1;
    }
    return 0;
}

int Ping_test::run() {
    int result;
    starttime = time(NULL);
    result = init();
    if (result == TEST_STATUS_FAIL) {
        finish();
    }
    if (interval > 0) {
        // 根据每次ping的间隔，添加超时事件
        Ping_Test_event *p_tev = new Ping_Test_event(this);
        p_tev->set_mark(false);    // mark 为false代表着到了时间，应该发起probe了
        event_assign(p_tev->get_event(), worker->get_base(), -1, EV_PERSIST, ping_timeout_callback_fn, (void *)p_tev);
        struct timeval *p_temp_tv;
        p_temp_tv = p_tev->get_timeout();
        p_temp_tv->tv_sec = interval / 1000;
        p_temp_tv->tv_usec = (interval % 1000) * 1000;
        event_add(p_tev->get_event(), p_temp_tv);
    }
    else {
        // 没有设置每次ping的间隔，在每次收到ping的返回数据的时候，发送下一次的ping，设置默认超时时间为1秒
        result = probe();
    }

    return TEST_STATUS_OK;
}

/*
 * 建立好socket，生成好相应的二进制的地址
*/
int Ping_test::init() {
    // 生成socket fd
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        printf("init ping socket fd error: %s\n", strerror(errno));
        return TEST_STATUS_FAIL;
    }
    evutil_make_socket_nonblocking(sockfd);
    // 得到目的地址对应的二进制地址
    // 这里以后可能要通过dns相关的函数自己获取一下
    struct addrinfo hints;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_flags = AI_CANONNAME;
    int res;
    res = getaddrinfo(host.c_str(), NULL, &hints, &servaddr);
    if (res != 0) {
        printf("init ping socket address error: %s\n", strerror(res));
        return TEST_STATUS_FAIL;  
    }
    status = TEST_STATUS_INIT;
    // 设置icmp的序列号
    nsent = 0;
    // 情况接受缓存和发送缓存
    bzero(sendbuf, 1500);
    bzero(recvbuf, 1500);
    bzero(controlbuf, 1500);
    starttime = time(NULL);
    nowrecvtimes = 0;
    nowtimes = 0;
    return status;
    
}

/*
 * 发送ping命令
*/
int Ping_test::probe() {
    int len;
    struct icmp *icmp;
    icmp = (struct icmp *)sendbuf;
    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_id = getpid();
    icmp->icmp_seq = nsent++;
    int datalen = 56;
    memset(icmp->icmp_data, 0xa5, datalen);
    gettimeofday((struct timeval *)icmp->icmp_data, NULL);
    len = 8 + datalen;
    icmp->icmp_cksum = 0;
    icmp->icmp_cksum = in_cksum((unsigned short *)icmp, len);
    if (sendto(sockfd, sendbuf, len, 0, servaddr->ai_addr, servaddr->ai_addrlen) < 0) {
        printf("probe ping error: %s\n", strerror(errno));
        return TEST_STATUS_FAIL;
    }
    nowtimes++;
    status = TEST_STATUS_PROBE;
    // 添加对这次probe的读取的响应
    Ping_Test_event *p_tev = new Ping_Test_event(this);
    event_assign(p_tev->get_event(), worker->get_base(), sockfd, EV_READ, ping_socket_callback_fn, (void *)p_tev);
    p_tev->set_mark(true);  // mark 为true代表的是probe检测超时
    struct timeval *p_temp_tv = NULL;
    if (timeout > 0) {
        p_temp_tv = p_tev->get_timeout();
        p_temp_tv->tv_sec = timeout / 1000;
        p_temp_tv->tv_usec = (timeout % 1000) * 1000;
    }
    event_add(p_tev->get_event(), p_temp_tv);
    return status;
}

/*
 * 接收回来的数据
*/
int Ping_test::receive() {
    struct msghdr msg;
    struct iovec iov;
    iov.iov_base = recvbuf;
    iov.iov_len = sizeof(recvbuf);
    char remoteaddr[servaddr->ai_addrlen];   // 这里要处理一下servaddr为NULL的情况
    msg.msg_name = remoteaddr;
    //msg.msg_name = calloc(1, servaddr->ai_addrlen);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = controlbuf;
    msg.msg_namelen = servaddr->ai_addrlen;
    msg.msg_controllen = sizeof(controlbuf);
    recvlen = recvmsg(sockfd, &msg, 0);
    gettimeofday(&recvtime, NULL);
    status = TEST_STATUS_RECV;
    nowrecvtimes++;
    return status;
}

/*
 * 处理接收回来的数据
*/
int Ping_test::process() {
    unsigned int hlen1, icmplen;
    double rtt;
    struct ip *ip;
    struct icmp *icmp;
    struct timeval *tvsend;
    status = TEST_STATUS_PROCESS;

    ip = (struct ip *)recvbuf;
    hlen1 = ip->ip_hl << 2;
    if (ip->ip_p != IPPROTO_ICMP)
        return TEST_STATUS_FAIL;

    icmp = (struct icmp *)(recvbuf + hlen1);
    if ((icmplen = recvlen - hlen1) < 8) {
        return TEST_STATUS_FAIL;
    }
    if (icmp->icmp_type == ICMP_ECHOREPLY) {
        if (icmp->icmp_id != pid) {
            return TEST_STATUS_FAIL;
        }
        if (icmplen < 16) {
            return TEST_STATUS_FAIL;
        }

        tvsend = (struct timeval *)icmp->icmp_data;
        tv_sub(&recvtime, tvsend);
        rtt = recvtime.tv_sec * 1000.0 + recvtime.tv_usec / 1000.0;
        struct Ping_data probedata;
        struct sockaddr_in *sin = (struct sockaddr_in *)servaddr->ai_addr;
        inet_ntop(AF_INET, &sin->sin_addr, probedata.hostip, servaddr->ai_addrlen);
        probedata.ttl = ip->ip_ttl;
        probedata.rtt = rtt;
        probedata.icmpseq = icmp->icmp_seq;
        probedata.datalen = icmplen;
        data_vec.push_back(probedata);

        if (nowtimes >= times) {
            return TEST_STATUS_LAST;
        }
        else {
            if (interval <= 0) {
                return TEST_STATUS_NOLAST;
            }
            else {
                return TEST_STATUS_PROCESS;
            }
        }

    }
}

/*
 * 回收探测资源，做收尾处理工作
 * 计算探测结果
 * 将探测的结果发送到相应的地方
*/
int Ping_test::finish() {
    donetime = time(NULL);
    // 回收资源
    if (servaddr != NULL) {
        freeaddrinfo(servaddr);
        servaddr = NULL;
    }
    close(sockfd);

    // 计算结果
    double rtt_min, rtt_avg, rtt_max, rtt_mdev, rtt_sum;
    std::vector<struct Ping_data>::iterator it;
    for (it = data_vec.begin(); it != data_vec.end(); ++it) {
        if (it == data_vec.begin()) {
            rtt_min = rtt_max = rtt_sum = it->rtt;
        }
        else {
            if (it->rtt < rtt_min)
                rtt_min = it->rtt;
            if (it->rtt > rtt_max)
                rtt_max = it->rtt;
            rtt_sum += it->rtt;
        }
    }
    rtt_avg = rtt_sum / data_vec.size();
    double rtt_accum;
    for (it = data_vec.begin(); it != data_vec.end(); ++it) {
        rtt_accum += (it->rtt - rtt_avg) * (it->rtt - rtt_avg);
    }
    rtt_mdev = sqrt(rtt_accum / data_vec.size());
    donetime = time(NULL);
    // 发送探测结果
    Json::Value metric;
    metric["host"] = host;
    metric["transmitted"] = nowtimes;
    metric["received"] = nowrecvtimes;
    metric["loss"] = (int)(100 - nowrecvtimes*100.0/nowtimes);
    metric["time"] = (int)(donetime - starttime)*1000;
    metric["min"] = (int)rtt_min*100;
    metric["avg"] = (int)rtt_avg*100;
    metric["max"] = (int)rtt_max*100;
    metric["mdev"] =(int)rtt_mdev*100;
    Json::FastWriter fast_writer;
    std::string metric_str = fast_writer.write(metric);
    sendresult(metric_str);
    
    char logbuf[256];
    bzero(logbuf, 256);
    sprintf(logbuf, "%s %d packets transmitted, %d received, %.2f%% packet loss, time %d ms", host.c_str(), nowtimes, nowrecvtimes, 100 - nowrecvtimes*100.0/nowtimes, (donetime - starttime)*1000);
    logger->info(logbuf);
    bzero(logbuf, 256);
    sprintf(logbuf, "%s rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms", host.c_str(), rtt_min, rtt_avg, rtt_max, rtt_mdev, (donetime-starttime)*1000);
    logger->info(logbuf);
    status = TEST_STATUS_DONE;
    data_vec.clear();
    return status;
}

struct event *Ping_test::get_test_ev() {
    return &test_ev;
}

int Ping_test::get_times() {
    return times;
}

int Ping_test::get_nowtimes() {
    return nowtimes;
}

int Ping_test::get_interval() {
    return interval;
}

Ping_test::~Ping_test() {
    struct event_base *base = worker->get_base();
    event_del(&test_ev);

}

///////////////////////////////////////////////////

bool Ping_Test_event::get_mark() {
    return mark;
}

void Ping_Test_event::set_mark(bool m) {
    mark = m;
    return;
}

