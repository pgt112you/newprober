#include "worker.h"
#include "logger.h"
#include <netdb.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>

#include <iostream>

#define BUFSIZE 1500


static void get_readable_addr(struct sockaddr_in *p_addr, std::string &addr_str) {
    char addr[INET_ADDRSTRLEN]; 
    bzero(addr, INET_ADDRSTRLEN); 
    inet_ntop(AF_INET, &(p_addr->sin_addr), addr, INET_ADDRSTRLEN); 
    int port;  
    port = ntohs(p_addr->sin_port); 
    char key_c[23];   // address:port 16+1+5 180.149.136.95:12112 
    bzero(key_c, 23);  
    sprintf(key_c, "%s:%d", addr, port); 
    addr_str = key_c;
}


// 处理icmp报文使用，这里其实应该用bufferevent
static void worker_icmp_cb(evutil_socket_t fd, short event, void*arg) {
    printf(">>>>>>>>>>>>>>icmp fd is %d\n", fd);
    struct ip *ip, *hip;
    struct icmp *icmp;
    struct udphdr *udp;
    int hlen1, hlen2, icmplen, ret;
    Worker *p_w = (Worker *)arg;
    char recvbuf[BUFSIZE];
    bzero(recvbuf, BUFSIZE);
    struct sockaddr src_addr;
    socklen_t len;
    int num = recvfrom(fd, recvbuf, BUFSIZE, 0, &src_addr, &len);
    ip = (struct ip *) recvbuf; /* start of IP header */
    hlen1 = ip->ip_hl << 2;     /* length of IP header */
    icmp = (struct icmp *) (recvbuf + hlen1); /* start of ICMP header */
    if ((icmplen = num - hlen1) < 8) {
        printf("not enough to look at ICMP header %d\n", icmplen);
        printf("n is %d, hlen1 is %d, icmplen is %d\n", num, hlen1, icmplen);
        return;               /* not enough to look at ICMP header */
    }
    if (icmp->icmp_type == ICMP_UNREACH) {
        printf(">>>>>>>>>>>>>>2222icmp fd is %d\n", fd);
        if (icmplen < 8 + sizeof(struct ip)) {
            printf("not enough data to look at inner IP\n");
            return;           /* not enough data to look at inner IP */
        }

        hip = (struct ip *) (recvbuf + hlen1 + 8);    // hip是icmp错误报文里面的包含的出错的ip报文
        hlen2 = hip->ip_hl << 2;
        if (icmplen < 8 + hlen2 + 4) {
            printf("not enough data to look at UDP ports\n");
            return;           /* not enough data to look at UDP ports */
        }
        udp = (struct udphdr *) (recvbuf + hlen1 + 8 + hlen2);
        if (hip->ip_p == IPPROTO_UDP) {
            if (icmp->icmp_code == ICMP_UNREACH_PORT) {
                printf("source %d unreachable \n", ntohs(udp->source));
                //printf("dest %s %d unreachable \n", hip->ip_dst.s_addr, ntohs(udp->dest)); // 应该是卡在了hip->ip_dst上
                printf("dest %d unreachable \n", ntohs(udp->dest));  
                ret = -1;   /* have reached destination */

                // 找到这个icmp端口不可达对应的test_event
                char ip_buff[16];
                bzero(ip_buff, 16);
                inet_ntop(AF_INET, &(hip->ip_dst), ip_buff, INET_ADDRSTRLEN);
                char port_buff[6];
                bzero(port_buff, 6);
                sprintf(port_buff, "%d", ntohs(udp->dest));
                std::string addr_str = std::string(ip_buff) + std::string(":") + std::string(port_buff);
                std::cout << "addr_str is " << addr_str << std::endl;
                Test_event *p_tev = p_w->get_icmp_unreach_test_event(addr_str); 
                if (p_tev == NULL) {
                    std::cout << getpid() << " can not find test event for " << addr_str << std::endl;
                    return;
                }
                Test *p_test = p_tev->get_test();
                p_test->finish(addr_str);
                event_del(p_tev->get_event());
                delete p_tev;

            }
            else {
                printf("%d icmp code is %d\n", getpid(), ret);
                ret = icmp->icmp_code;  /* 0, 1, 2, ... */
            }
        }
    }
}



// 处理飞来的日志文件描述符专用
static void worker_channel_1_cb(evutil_socket_t fd, short event, void *arg) {
    Worker *p_w = (Worker *)arg;
    int openfd = recv_openfd(fd);
    p_w->split_log(openfd);
    return;
}

static void worker_channel_readcb(struct bufferevent *bev, void *ctx) {
    Worker *p_w = (Worker *)ctx;
    p_w->deal_channel_data(bev);
    //int inputfd = bufferevent_getfd(bev);
    //p_w->deal_channel_data_fd(inputfd);

}


void worker_timeout_callback_fn(evutil_socket_t fd, short event, void *arg) {
    Test *p_test = (Test *)arg;
    int result;
    if (event & EV_TIMEOUT) {
        // 到时间了，运行新的test
        printf("run test %s\n", p_test->get_name().c_str());
        result = p_test->run();
    }
}

void redisconnectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        printf("11 Error: %s\n", c->errstr);
        return;
    }
    printf("Connected...\n");
}

void redisdisconnectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        printf("11 Error: %s\n", c->errstr);
        return;
    }
    printf("Disconnected...\n");
}



Worker::Worker() {
    pid = getpid();
    testnum = 0;
    base = event_base_new();
    // 生成icmp监听的事件
    
}

void Worker::add_test(Test *p_test) {
    //event_assign(&(p_test->get_test_ev()), base, fd, EV_READ|EV_PERSIST, callback_fn, (void*)&t);
    event_assign(p_test->get_test_ev(), base, -1, EV_PERSIST, worker_timeout_callback_fn, (void*)p_test);
    struct timeval frenc_tv;
    frenc_tv.tv_sec = p_test->get_frenc();
    frenc_tv.tv_usec = 0;
    event_add(p_test->get_test_ev(), &frenc_tv);
    int id = p_test->get_id();
    test_map[id] = p_test;
    printf("worker add test %s frenc %d\n", p_test->get_name().c_str(), p_test->get_frenc());
}

void Worker::run() {
    event_base_dispatch(base);
}

struct event_base *Worker::get_base() {
    return base;
}

struct redisAsyncContext *Worker::get_redis() {
    return redisc;
}

void Worker::set_channel(int fd) {
    channel = fd;
    evutil_make_socket_nonblocking(channel);
    channelevent = bufferevent_socket_new(base, channel, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setwatermark(channelevent, EV_READ, sizeof(channel_data), 0);
    bufferevent_setcb(channelevent, worker_channel_readcb, NULL, NULL, (void *)this);
    bufferevent_enable(channelevent, EV_READ);
}

void Worker::set_channel_1(int fd) {
    channel_1 = fd;
    evutil_make_socket_nonblocking(channel_1);
    channel_1_event = event_new(base, channel_1, EV_READ|EV_PERSIST, worker_channel_1_cb, (void *)this);
    event_add(channel_1_event, NULL);
}

void Worker::set_logger(Logger *l) {
    logger = l;
}

Logger *Worker::get_logger() {
    return logger;
}

/*
 * 监听icmp报文
 * 主要用来接收udp检测等
 * 得到的icmp差错报文
*/
void Worker::listen_icmp() {
    icmpsock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    evutil_make_socket_nonblocking(icmpsock);
    icmp_event = event_new(base, icmpsock, EV_READ|EV_PERSIST, worker_icmp_cb, (void *)this);
    event_add(icmp_event, NULL);
    return;
}

/*
 * 处理从channel接收过来的数据，包括判断数据的类型，比如是增加配置，删除配置，这种的，解析出相应的数据，然后再调用相应的处理函数
*/
void Worker::deal_channel_data(struct bufferevent *bev) {
    struct evbuffer *input = bufferevent_get_input(bev);
    while (evbuffer_get_length(input) >= sizeof(channel_data)) {
        channel_data data;
        //int num = evbuffer_copyout(input, &data, sizeof(channel_data));
        int num = evbuffer_remove(input, &data, sizeof(channel_data));
        if (num < 0)
            return;
        Json::Reader reader;
        Json::Value root;
        bool ok;
        switch (data.type) {
            case MASTER_MSG_TYPE_ADDCONF:
                logger->info("add new conf");
                ok = reader.parse(data.msg, root);
                if (!ok) {  // 解析json格式错误
                    return;
                }
                else {
                    Test *pt = generate_test(root);
                    // 向master返回新加的任务id
                    if (pt != NULL) {
                        //bufferevent_write(channelevent, &data, sizeof(channel_data));
                        int id = root["id"].asInt();
                        bzero(data.msg, MAX_MSG_LEN);
                        memcpy(data.msg, &id, sizeof(int));
                        bufferevent_write(channelevent, &data, num);
                        //int fd = bufferevent_getfd(bev);
                        //write(fd, &data, num);
                    }
                    add_test(pt);
                }
                break;
            case MASTER_MSG_TYPE_DELCONF:
                ok = reader.parse(data.msg, root);
                if (!ok) {  // 解析json格式错误
                    return;
                }
                logger->info("delete conf"); 
                delete_test(root);
                break;
            case MASTER_MSG_TYPE_REDISPORT:
                logger->info("FIFO path"); 
                //fifopath = data.msg;
                //memcpy(&redisport, data.msg, sizeof(int));
                redisport = (int)atoi(data.msg);
                //openfifo();
                openredis();
                break;
        }
    }
}


void Worker::openfifo() {
    if ((mkfifo(fifopath.c_str(), FILE_MODE) < 0) && (errno != EEXIST)) {
        logger->error("mkfifo failed");
    }
    fifofd = open(fifopath.c_str(), O_WRONLY|O_NONBLOCK, 0);
    if (fifofd == -1) {
        printf("open fifo failed %s\n", strerror(errno));
    }

}

void Worker::openredis() {
    redisc = redisAsyncConnect("127.0.0.1", redisport);
    if (redisc->err) {
         printf("connect redis Error: %s\n", redisc->errstr);
         return;
    }
    redisLibeventAttach(redisc, base);
    redisAsyncSetConnectCallback(redisc, redisconnectCallback);
    redisAsyncSetDisconnectCallback(redisc, redisdisconnectCallback);

}


int Worker::getfifo() {
    return fifofd;
}


Test *Worker::generate_test(Json::Value &root) {
    if (!(root.isMember("type") && root.isMember("id"))) {
        return NULL;    // 数据格式不正确
    }
    std::string logstr = "genreate type " + root["type"].asString() + " test";
    logger->info(logstr.c_str());
    if (0 == root["type"].asString().compare("PING")) {
        Config config;
        config.set_id(root["id"].asInt());
        if (root.isMember("host"))
            config.set_host(root["host"].asString());
        if (root.isMember("times"))
            config.set_times(root["times"].asInt());
        if (root.isMember("ttl"))
            config.set_ttl(root["ttl"].asInt());
        if (root.isMember("frenc"))
            config.set_frenc(root["frenc"].asInt());
        if (root.isMember("interval"))
            config.set_interval(root["interval"].asInt());
        Test *pt = new Ping_test(this);
        pt->config(&config);    // 设置配置
        pt->set_logger(logger);
        return pt;
    }
    if (0 == root["type"].asString().compare("UDP")) {
        Config config;
        config.set_id(root["id"].asInt());
        config.set_type(root["type"].asString());
        if (root.isMember("host"))
            config.set_host(root["host"].asString());
        if (root.isMember("port"))
            config.set_port(root["port"].asInt());
        if (root.isMember("frenc"))
            config.set_frenc(root["frenc"].asInt());
        if (root.isMember("timeout"))
            config.set_timeout(root["timeout"].asInt());
        Test *pt = new UDP_test(this);
        pt->config(&config);    // 设置配置
        pt->set_logger(logger);
        return pt;
    }
    return NULL;
}

int Worker::delete_test(Json::Value &root) {
    if (!root.isMember("id"))
        return -1;
    std::map<int, Test *>::iterator it = test_map.find(root["id"].asInt());
    if (it == test_map.end())
        return -1;
    Test *p_t = it->second;
    test_map.erase(root["id"].asInt());    
    event_del(p_t->get_test_ev());
    delete p_t;
    return 0;
}

void Worker::split_log(int newfd) {
    logger->close_fd();
    logger->set_fd(newfd);
}

Test_event * Worker::get_icmp_unreach_test_event(struct sockaddr_in *p_addr) {
    std::string key_str;
    get_readable_addr(p_addr, key_str);
    std::map<std::string, Test_event *>::iterator it;
    it = icmp_unreach_map.find(key_str); 
    if (it != icmp_unreach_map.end()) {
        return it->second;
    }
    else {
        return NULL;
    }
}

Test_event * Worker::get_icmp_unreach_test_event(std::string &addr_str) {   // addr_str like 10.88.15.46:9112
    std::map<std::string, Test_event *>::iterator it;
    it = icmp_unreach_map.find(addr_str); 
    if (it != icmp_unreach_map.end()) {
        return it->second;
    }
    else {
        return NULL;
    }
}

void Worker::set_icmp_unreach_test_event(struct sockaddr_in *p_addr, Test_event* p_tev) {
    std::string key_str;
    get_readable_addr(p_addr, key_str);
    std::cout << "register key_str is " << key_str << std::endl;
    if (p_tev) {
        std::string str((char *)p_addr, sizeof(struct sockaddr_in));
        icmp_unreach_map[key_str] = p_tev;
    }
}

void Worker::del_icmp_unreach_test_event(struct sockaddr_in* p_addr) {
    std::string key_str;
    get_readable_addr(p_addr, key_str);
    int res = icmp_unreach_map.erase(key_str);
    printf("erase result is %d\n", res);
}
