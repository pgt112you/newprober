#include <fcntl.h>
#include <stdio.h>
#include <string>
#include <cstring>


#ifndef NEWPROBER_LOGGER_H
#define NEWPROBER_LOGGER_H

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_NOTICE 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_CRITIAL 3
#define LOG_LEVEL_ERROR 4

#define MAX_FILEPATH 256
#define TIME_LEN 20
#define LEVEL_LEN 10

class Logger {
  public:
    Logger();
    ~Logger();
    void set_logfile(const char *);
    int set_level(int level);    // 返回之前的level
    void debug(const char *);
    void notice(const char *);
    void info(const char *);
    void critial(const char *);
    void error(const char *);
    int split_log();    // 分割日志，返回新的日志文件的fd
    int close_fd();   // 关闭日志文件
    int set_fd(int newfd);    // 将日志的文件描述符设置成newfd
  private:
    int level;
    int logfilefd;
    char logfile[MAX_FILEPATH];
    char timebuf[TIME_LEN];
    char levelbuf[LEVEL_LEN];
    void output(int level, const char *content);
};


#endif
