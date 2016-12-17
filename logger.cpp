#include "logger.h"


Logger::Logger() {
    level = LOG_LEVEL_INFO;
    logfilefd = -1;
    bzero(logfile, LOG_LEVEL_INFO);
    bzero(timebuf, TIME_LEN);
    bzero(levelbuf, LEVEL_LEN);
}

Logger::~Logger() {
    close_fd();
}

void Logger::set_logfile(const char *newlogfile) {
    bzero(logfile, MAX_FILEPATH);
    memcpy(logfile, newlogfile, MAX_FILEPATH);
    logfilefd = open(logfile, O_RDWR|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    int val = fcntl(logfilefd, F_GETFL, 0);
    val |= O_NONBLOCK;
    fcntl(logfilefd, F_SETFL, val);
    bzero(logfile, MAX_FILEPATH);
    memcpy(logfile, newlogfile, MAX_FILEPATH);
}

int Logger::set_level(int newlevel) {
    int oldlevel = level;
    level = newlevel;
    return oldlevel;
}

void Logger::debug(const char *content) {
    if (level < LOG_LEVEL_DEBUG) 
        return;
    output(LOG_LEVEL_DEBUG, content);
}

void Logger::notice(const char *content) {
    if (level < LOG_LEVEL_NOTICE) 
        return;
    output(LOG_LEVEL_NOTICE, content);
}

void Logger::info(const char *content) {
    if (level < LOG_LEVEL_INFO) 
        return;
    output(LOG_LEVEL_INFO, content);
}

void Logger::critial(const char *content) {
    if (level < LOG_LEVEL_CRITIAL) 
        return;
    output(LOG_LEVEL_CRITIAL, content);
}

void Logger::error(const char *content) {
    if (level < LOG_LEVEL_ERROR) 
        return;
    output(LOG_LEVEL_ERROR, content);
}

void Logger::output(int level, const char *content_cstr) {
    std::string content = content_cstr;
    time_t nowt = time(NULL);
    struct tm *p_t = localtime(&nowt);
    bzero(timebuf, TIME_LEN);
    strftime(timebuf, TIME_LEN, "%Y-%m-%d %H:%M:%S", p_t);
    bzero(levelbuf, LEVEL_LEN);
    switch (level) {
        case LOG_LEVEL_DEBUG:
            strcpy(levelbuf, "DEBUG");
            break;
        case LOG_LEVEL_NOTICE:
            strcpy(levelbuf, "NOTICE");
            break;
        case LOG_LEVEL_INFO:
            strcpy(levelbuf, "INFO");
            break;
        case LOG_LEVEL_CRITIAL:
            strcpy(levelbuf, "CRITIAL");
            break;
        case LOG_LEVEL_ERROR:
            strcpy(levelbuf, "ERROR");
            break;
    }
    std::string output_str = std::string(levelbuf) + "    " + std::string(timebuf) + "    " + content + "\n";    // level    time    content
    write(logfilefd, output_str.c_str(), output_str.size());
}

int Logger::split_log() {
    close_fd();

    time_t nowt = time(NULL);  
    struct tm *p_t = localtime(&nowt);
    bzero(timebuf, TIME_LEN);
    strftime(timebuf, TIME_LEN, "%Y-%m-%d_%H:%M:%S", p_t);
    std::string baklogfile = std::string(logfile) + std::string("-") + std::string(timebuf);
    rename(logfile, baklogfile.c_str());

    logfilefd = open(logfile, O_RDWR|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    int val = fcntl(logfilefd, F_GETFL, 0);
    val |= O_NONBLOCK;
    fcntl(logfilefd, F_SETFL, val);
    return logfilefd;
}

int Logger::close_fd() {
    close(logfilefd);
    return logfilefd;
}

int Logger::set_fd(int newfd) {
    int oldfd = logfilefd;
    logfilefd = newfd;
    return oldfd;
}
