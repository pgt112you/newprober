#include <sys/time.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <stdarg.h>
#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>


#ifndef NEWPROBER_TOOL_H
#define NEWPROBER_TOOL_H

void tv_sub(struct timeval *out, struct timeval *in);
uint16_t in_cksum(uint16_t *addr, int len);

#define MAX_MSG_LEN 1000


typedef struct {
    int type;
    char msg[MAX_MSG_LEN];   // 消息的大小不能大于MAX_MSG_LEN个字节
} channel_data;


#define MAXLINE 2048
void setproctitle_init(int argc, char **argv, char **envp);
void setproctitle(const char *fmt, ...);

/* 用fd来接收一个打开的fd，并且将这个fd返回 */
int recv_openfd(int fd);
/* 用fd来发送一个打开的fd（openfd） */
void send_openfd(int fd, int openfd);



#endif
