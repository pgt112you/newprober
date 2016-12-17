#include <errno.h>
#include <math.h>
#include "test.h"
#include "tool.h"


int Test::get_id() {
    return id;
}

void Test::set_id(int i = -1) {
    if (i < 0) 
        i = rand() + 1000000;   // 随机生成的id要求大于1 million
    id = i;
}

int Test::get_frenc() {
    return frenc;
}

void Test::set_frenc(int f=-1) {
    frenc = f;
}

std::string Test::get_name() {
    return name;
}

void Test::set_name(std::string n) {
    name = n;
}

Logger *Test::get_logger() {
    return logger;
}

void Test::set_logger(Logger *l) {
    logger = l;
}

int Test::sendresult(std::string &content) {
    //int fifofd = worker->getfifo();
    //printf("%d in test fifofd is %d\n", getpid(), fifofd);
    //int len = content.size();
    //char sendbuf[len+sizeof(int)];
    //bzero(sendbuf, len+sizeof(int));
    //memcpy(sendbuf, &len, sizeof(int));
    //memcpy(sendbuf+sizeof(int), content.c_str(), len);
    //int res = write(fifofd, sendbuf, len+sizeof(int));
    //printf("write to fifo result is %d\n", res);
    //if (res < 0) {
    //    printf("write fifo failed %s\n", strerror(errno));
    //}
    //printf("LPUSH proberlist %s\n", content.c_str());
    redisAsyncCommand(worker->get_redis(), NULL, NULL, "LPUSH proberlist %s", content.c_str(), content.size());
    return 0;
}

///////////////////////////////////////////////

Test_event::Test_event(Test *test) {
    p_test = test;
}

Test *Test_event::get_test() {
    return p_test;
}

struct event *Test_event::get_event() {
    return &ev;
}

struct timeval *Test_event::get_timeout() {
    return &timeout;
}

