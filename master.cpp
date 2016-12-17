#include "master.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <sys/types.h>  
#include <sys/socket.h>


static struct event_base *wbase;


static void send_document_cb(struct evhttp_request *req, void *arg) {
    evhttp_send_reply(req, 200, "OK", NULL);
}

static void metric_request_cb(struct evhttp_request *req, void *arg) {
    Master *p_m = (Master *)arg;
    char *fulluri = (char *)evhttp_request_get_uri(req);
    if (strcmp(fulluri, "/metric") == 0) {
        p_m->deal_metric_request(req);
    }
    else if (strcmp(fulluri, "/metric/delete") == 0) {
        p_m->delete_metric_request(req);
    }
    evhttp_send_reply(req, 200, "OK", NULL);
    free(fulluri);

}

static void master_channel_readcb(struct bufferevent *bev, void *ctx) { 
    Master *p_m = (Master *)ctx;   
    printf("in master channel readcb\n");
    p_m->deal_channel_data(bev); 
}

static void master_timeout_flushconf_cb(evutil_socket_t fd, short event, void *arg) { 
    printf("in master timeout flushconf cb\n");
    Master *p_m = (Master *)arg;   
    p_m->flush_test_conf(); 
}

static void master_timeout_distributeconf_cb(evutil_socket_t fd, short event, void *arg) { 
    printf("in master timeout distributeconf cb\n");
    Master *p_m = (Master *)arg;   
    p_m->distribute_conf(); 
}

static void master_timeout_splitlog_cb(evutil_socket_t fd, short event, void *arg) { 
    printf("in master timeout splitlog cb\n");
    Master *p_m = (Master *)arg;   
    p_m->split_log(); 
}

Master::Master() {
    port = 8000;
    workernum = 0;
    base = NULL;
    http = NULL;
    redisport = -1;
}


Master::~Master() {
    recover_resource();
}

void Master::recover_resource() {
    if (http != NULL) {
        evhttp_free(http);
        //event_del(http);
        //event_free(http);
        http = NULL;
    }
    if (distribute_conf_ev != NULL) {
        event_del(distribute_conf_ev);
        event_free(distribute_conf_ev);
        distribute_conf_ev = NULL;
    }
    if (split_log_ev != NULL) {
        event_del(split_log_ev);
        event_free(split_log_ev);
        split_log_ev = NULL;
    }
    if (base != NULL) {
        event_base_free(base);
        base = NULL;
    }
}

int Master::init() {
    base = event_base_new(); 
    http = evhttp_new(base);
    evhttp_set_cb(http, "/metric", metric_request_cb, (void *)this);
    evhttp_set_cb(http, "/metric/delete", metric_request_cb, (void *)this);
    evhttp_set_gencb(http, send_document_cb, NULL);
    handle = evhttp_bind_socket_with_handle(http, "0.0.0.0", port);
    {
        char uri_root[512];
        /* Extract and display the address we're listening on. */
        struct sockaddr_storage ss;
        evutil_socket_t fd;
        ev_socklen_t socklen = sizeof(ss);
        char addrbuf[128];
        void *inaddr;
        const char *addr;
        int got_port = -1;
        fd = evhttp_bound_socket_get_fd(handle);
        memset(&ss, 0, sizeof(ss));
        if (getsockname(fd, (struct sockaddr *)&ss, &socklen)) {
            perror("getsockname() failed");
            return 1;
        }
        if (ss.ss_family == AF_INET) {
            got_port = ntohs(((struct sockaddr_in*)&ss)->sin_port);
            inaddr = &((struct sockaddr_in*)&ss)->sin_addr;
        } else if (ss.ss_family == AF_INET6) {
            got_port = ntohs(((struct sockaddr_in6*)&ss)->sin6_port);
            inaddr = &((struct sockaddr_in6*)&ss)->sin6_addr;
        } else {
            fprintf(stderr, "Weird address family %d\n",
                ss.ss_family);
            return 1;
        }
        addr = evutil_inet_ntop(ss.ss_family, inaddr, addrbuf,
            sizeof(addrbuf));
        if (addr) {
            printf("Listening on %s:%d\n", addr, got_port);
            evutil_snprintf(uri_root, sizeof(uri_root),
                "http://%s:%d",addr,got_port);
        } else {
            fprintf(stderr, "evutil_inet_ntop failed\n");
            return 1;
        }
    }
}

int Master::get_server_conf(const char *filepath) {
    bzero(server_conf_file, MAX_CONFFILE_LEN);
    memcpy(server_conf_file, filepath, strlen(filepath));
    std::fstream fs;
    fs.open(filepath, std::ios_base::in);
    char buf[256];
    char *pos;
    char *sep_pos;    // 分隔符的位置
    while (!fs.eof()) {
        bzero(buf, 256);
        std::string buf_str;
        std::getline(fs, buf_str);
        //fs >> buf;
        memcpy(buf, buf_str.c_str(), 256);
        pos = buf;
        while (*pos == ' ') pos++;
        if (pos == NULL) continue;  // 去掉空行
        if (*pos == '\0') continue;
        if (*pos == '\n') continue;  // 去掉空行
        if (*pos == '#') continue;   // 去掉注释行
        sep_pos = strchr(pos, ' ');
        if (sep_pos == NULL) continue;
        std::string name(pos, sep_pos-pos);
        //while (*pos != ' ') pos++; 
        //std::string value(pos);
        while (*sep_pos == ' ') sep_pos++;
        std::string value(sep_pos);
        if (!strcmp(name.c_str(), "port")) {
            port = (int)atoi(value.c_str());
        }
        else if (!strcmp(name.c_str(), "workernum")) {
            workernum = (int)atoi(value.c_str());
        }
        else if (!strcmp(name.c_str(), "splitlog")) {
            splitlog_interval = (int)atoi(value.c_str());
        }
        else if (!strcmp(name.c_str(), "flushconf")) {
            flushconf_interval = (int)atoi(value.c_str());
        }
        else if (!strcmp(name.c_str(), "fifopath")) {
            fifopath = value;
        }
        else if (!strcmp(name.c_str(), "redisport")) {
            redisport = (int)atoi(value.c_str());
        }
        else {
            ;
        }
    }
    fs.close();
    return 0;
}

int Master::get_test_conf(const char *filepath) {
    bzero(test_conf_file, MAX_CONFFILE_LEN);
    memcpy(test_conf_file, filepath, strlen(filepath));
    std::fstream fs;
    fs.open(filepath, std::ios_base::in);
    char buf[1024];
    char *pos;
    while (!fs.eof()) {
        bzero(buf, 1024);
        std::string buf_str;
        std::getline(fs, buf_str);
        memcpy(buf, buf_str.c_str(), 256);
        //fs >> buf;
        pos = buf;
        while (*pos == ' ') pos++;
        if (pos == NULL) continue;  // 去掉空行
        if (*pos == '\0') continue;
        if (*pos == '\n') continue;  // 去掉空行
        if (*pos == '#') continue;   // 去掉注释行
        Json::Reader reader;
        Json::Value root;
        bool ok;
        ok = reader.parse(buf_str, root);
        if (!ok) return -1;
        if (!(root.isMember("type") && root.isMember("id"))) {
            return -1;
        }
        configid_map[root["id"].asInt()] = buf_str;
    }
    fs.close();
    return 0;
}

int Master::generate_worker() {
    if (workernum <= 0) {
        return -1;
    }
    int parent_pid = getpid();
    Worker *pw = NULL;
    //std::vector<int> worker_closefd_vec;  // 存放在子进程里要关闭的chanel的无用的fd
    for (int i = 0; i < workernum; i++) {
        // 可以考虑在这里先生成一个worker对象，然后在worker_run里面调用worker
        int channel[2];
        int channel_1[2];   // 用来传递日志的文件描述符的channel
        socketpair(AF_UNIX, SOCK_STREAM, 0, channel);
        socketpair(AF_UNIX, SOCK_STREAM, 0, channel_1);
        pid_t pid = fork();
        if (pid == 0) {   // child process
            close(channel[0]);
            close(channel_1[0]);
            evutil_make_socket_nonblocking(channel[1]);
            evutil_make_socket_nonblocking(channel_1[1]);
            Worker worker;
            worker.set_channel(channel[1]);
            worker.set_channel_1(channel_1[1]);
            worker.set_logger(get_logger());
            worker.listen_icmp();
            pw = &worker;

            // 回收主进程遗留的资源
            std::vector<int>::iterator it;
            for (it = channel_fd_vec.begin(); it != channel_fd_vec.end(); ++it) {
                close(*it);
            }
            channel_fd_vec.clear();
            for (it = channel_1_fd_vec.begin(); it != channel_1_fd_vec.end(); ++it) {
                close(*it);
            }
            channel_1_fd_vec.clear();

            //std::vector<struct bufferevent *>::iterator itt;
            //for (itt = channelevent_vec.begin(); itt != channelevent_vec.end(); ++itt) {
            //    bufferevent_free(*itt);
            //}
            //channelevent_vec.clear();

            channel_pid_map.clear();
            config_map.clear();
            workerpid_vec.clear();
            //if (http != NULL) {
            //    evhttp_free(http);
            //    http = NULL;
            //}
            //if (base != NULL) {
            //    event_base_free(base);
            //    base = NULL;
            //}

            break;
        }
        else {
            //if (base == NULL) init();

            // 监听channel
            close(channel[1]);    // master 关闭channel 1，worker关闭channel 0
            close(channel_1[1]);    // master 关闭channel 1，worker关闭channel 0
            evutil_make_socket_nonblocking(channel[0]);
            channel_fd_vec.push_back(channel[0]);
            evutil_make_socket_nonblocking(channel_1[0]);
            channel_1_fd_vec.push_back(channel_1[0]);

            //struct bufferevent *channelevent = bufferevent_socket_new(base, channel[0], BEV_OPT_CLOSE_ON_FREE);
            //bufferevent_setwatermark(channelevent, EV_READ, sizeof(channel_data), 0);
            //bufferevent_setcb(channelevent, master_channel_readcb, NULL, NULL, (void *)this); 
            //bufferevent_enable(channelevent, EV_READ);
            //channelevent_vec.push_back(channelevent);

            // 初始化master一些相关的变量
            workerpid_vec.push_back(pid);
            channel_pid_map[channel[0]] = pid;
            config_map[channel[0]] = std::vector<std::map<int, std::string>::iterator>(); 

        }
    }
    if (parent_pid != getpid()) {
        logger->info("worker run");
        pw->run();
    }
    else {    // parent
        //;
        if (base == NULL)
            init();
        std::vector<int>::iterator it;
        for (it = channel_fd_vec.begin(); it != channel_fd_vec.end(); it++) {
            struct bufferevent *channelevent = bufferevent_socket_new(base, *it, BEV_OPT_CLOSE_ON_FREE);
            bufferevent_setwatermark(channelevent, EV_READ, sizeof(channel_data), 0);
            bufferevent_setcb(channelevent, master_channel_readcb, NULL, NULL, (void *)this); 
            bufferevent_enable(channelevent, EV_READ);
            channelevent_vec.push_back(channelevent);
        }
    }
    return 0;
}


int Master::distribute_conf() {
    std::map<int, std::string>::iterator it;
    int i = 0;
    int res;
    for (it = configid_map.begin(); it != configid_map.end(); it++) {
        int channelfd = channel_fd_vec[i++ % workernum];
        configid_channel_map[it->first] = channelfd;
        config_map[channelfd].push_back(it);
        // 分发test.conf相关配置
        res = send_msg(channelfd, MASTER_MSG_TYPE_ADDCONF, it->second);
        // 分发server.conf相关配置
        //res = send_msg(channelfd, MASTER_MSG_TYPE_FIFOPATH, fifopath);
        char port_c[6];
        bzero(port_c, 6);
        sprintf(port_c, "%d", redisport);
        std::string portstr(port_c);
        res = send_msg(channelfd, MASTER_MSG_TYPE_REDISPORT, portstr);
    }
}

int Master::send_msg(int channelfd, int type, std::string &msg) {
    channel_data data;
    bzero(&data, sizeof(channel_data));
    data.type = type;
    size_t data_size = MAX_MSG_LEN;
    if (data_size > msg.size())
        data_size = msg.size();
    memcpy((void *)&data.msg, msg.c_str(), data_size);
    return write(channelfd, &data, sizeof(channel_data));
}

int Master::run() {
    /* 设置一些定时配置 */
    //分割日志
    split_log_ev = event_new(base, -1, EV_PERSIST, master_timeout_splitlog_cb, (void*)this);
    struct timeval frenc_tv;
    frenc_tv.tv_sec = splitlog_interval;   // 这里应该是有几个worker进程停几秒
    frenc_tv.tv_usec = 0;
    event_add(split_log_ev, &frenc_tv);

    // 分发配置
    distribute_conf_ev = event_new(base, -1, 0, master_timeout_distributeconf_cb, (void*)this);
    frenc_tv.tv_sec = 1;   // 这里应该是有几个worker进程停几秒
    frenc_tv.tv_usec = 0;
    event_add(distribute_conf_ev, &frenc_tv);

    // 冲配置文件
    flush_conf_ev = event_new(base, -1, EV_PERSIST, master_timeout_flushconf_cb, (void*)this);
    frenc_tv.tv_sec = flushconf_interval; 
    frenc_tv.tv_usec = 0;
    event_add(flush_conf_ev, &frenc_tv);

    event_base_dispatch(base); 
}

Logger *Master::get_logger() {
    return logger;
}

void Master::set_logger(Logger *l) {
    logger = l;
}

void Master::deal_channel_data(struct bufferevent *bev) {
    struct evbuffer *input = bufferevent_get_input(bev);  
    while (evbuffer_get_length(input) >= sizeof(channel_data)) {
        channel_data data;  
        //evbuffer_copyout(input, &data, sizeof(channel_data)); 
        evbuffer_remove(input, &data, sizeof(channel_data)); 
        //bufferevent_read(bev, &data, sizeof(channel_data)); 
        Json::Reader reader;
        Json::Value root;
        bool ok; 
        switch (data.type) {
            case MASTER_MSG_TYPE_ADDCONF: 
                int id;
                memcpy(&id, data.msg, sizeof(int));
                configid_channel_map[id] = bufferevent_getfd(bev);
                break;
            case MASTER_MSG_TYPE_DELCONF:
                break;
        }
    }
}

struct event_base *Master::get_base() {
    return base;
}

void Master::split_log() {
    int newfd = logger->split_log();
    std::vector<int>::iterator it;
    for (it = channel_1_fd_vec.begin(); it != channel_1_fd_vec.end(); it++) {
        send_cmd(*it, newfd);
    }
}

void Master::send_cmd(int fd, int cmdfd) {
    send_openfd(fd, cmdfd);
}


void Master::flush_test_conf() {
    //std::fstream fs;
    //fs.open(test_conf_file, std::ios_base::out);
    //std::map<int, std::string>::iterator it;
    //for (it = configid_map.begin(); it != configid_map.end(); it++) {
    //    fs << it->second << std::endl << std::endl;
    //}
    //fs.close();
    int fd = open(test_conf_file, O_RDWR|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    std::map<int, std::string>::iterator it;
    for (it = configid_map.begin(); it != configid_map.end(); it++) {
        write(fd, it->second.c_str(), it->second.size());
        write(fd, "\n\n", 2);
    }
    close(fd);

}


void Master::deal_metric_request(struct evhttp_request *req) {
    const char *cmdtype;
    struct evkeyvalq *headers;
    struct evkeyval *header;
    struct evbuffer *buf;
    switch (evhttp_request_get_command(req)) {
    case EVHTTP_REQ_GET: 
        cmdtype = "GET"; 
        break;
    case EVHTTP_REQ_POST: 
        cmdtype = "POST"; 
        break;
    case EVHTTP_REQ_DELETE: 
        cmdtype = "DELETE"; 
        break;
    case EVHTTP_REQ_PUT: 
        cmdtype = "PUT"; 
        break;
    default: 
        cmdtype = "unknown"; 
        return;
    }    
    buf = evhttp_request_get_input_buffer(req);
    int buflen = evbuffer_get_length(buf);
    char *p = (char *)malloc(buflen+1);
    bzero(p, buflen+1);
    evbuffer_copyout(buf, p, buflen);
    std::string data_value_str(p);
    bzero(p, buflen+1);
    free(p);
    std::cout << data_value_str << " " << data_value_str.size() << std::endl;
    // 添加或者修改
    if (strcmp(cmdtype, "POST") == 0) {
        // 验证数据的正确性
        Json::Reader reader;
        Json::Value root;
        bool ok = reader.parse(data_value_str, root);
        if (!ok) {
            return;
        }
        if (!root.isMember("id") || !root.isMember("type")) {
            return;
        }
        int id = root["id"].asInt();
        std::map<int, std::string>::iterator temp_it = configid_map.find(id);
        if (temp_it == configid_map.end()) {    // 新增配置
            // 找到目前拥有配置最少的那个worker的channel
            std::map<int, std::vector<std::map<int, std::string>::iterator> >::iterator it; 
            std::map<int, std::vector<std::map<int, std::string>::iterator> >::iterator itt;
            int min = 1000000;
            for (it = config_map.begin(); it != config_map.end(); ++it) {
                if (it == config_map.begin()) {
                    itt = it;
                }
                if (min > it->second.size()) {
                    min = it->second.size();
                    itt = it;
                }
            }
            // 新加channel
            int channelfd = itt->first;
            configid_map[id] = data_value_str;
            config_map[channelfd].push_back(configid_map.find(id));
            configid_channel_map[id] = channelfd;
            int res = send_msg(channelfd, MASTER_MSG_TYPE_ADDCONF, data_value_str);
        }
        else {    // 修改配置
            configid_map[id] = data_value_str;
            int res = send_msg(configid_channel_map[id], MASTER_MSG_TYPE_EDITCONF, data_value_str);
        }
    }
}


void Master::delete_metric_request(struct evhttp_request *req) {
    const char *cmdtype;
    struct evkeyvalq *headers;
    struct evkeyval *header;
    struct evbuffer *buf;
    switch (evhttp_request_get_command(req)) {
    case EVHTTP_REQ_GET: 
        cmdtype = "GET"; 
        break;
    case EVHTTP_REQ_POST: 
        cmdtype = "POST"; 
        break;
    case EVHTTP_REQ_DELETE: 
        cmdtype = "DELETE"; 
        break;
    case EVHTTP_REQ_PUT: 
        cmdtype = "PUT"; 
        break;
    default: 
        cmdtype = "unknown"; 
        return;
    }    
    buf = evhttp_request_get_input_buffer(req);
    int buflen = evbuffer_get_length(buf);
    char *p = (char *)malloc(buflen+1);
    bzero(p, buflen+1);
    evbuffer_copyout(buf, p, buflen);
    std::string data_value_str(p);
    bzero(p, buflen+1);
    free(p);
    std::cout << data_value_str << " " << data_value_str.size() << std::endl;
    if (strcmp(cmdtype, "POST") == 0) {
        // 验证数据的正确性
        Json::Reader reader;
        Json::Value root;
        bool ok = reader.parse(data_value_str, root);
        if (!ok) {
            return;
        }
        if (!root.isMember("id") || !root.isMember("type")) {
            return;
        }
        int id = root["id"].asInt();
        std::map<int, std::string>::iterator temp_it = configid_map.find(id);
        if (temp_it == configid_map.end()) {
            return;
        }
        configid_map.erase(id);
        int res = send_msg(configid_channel_map[id], MASTER_MSG_TYPE_DELCONF, data_value_str);
    }
}

