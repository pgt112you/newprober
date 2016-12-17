#include <string>


#ifndef CONFIGURATION_H
#define CONFIGURATION_H


class Config {
  public:
    //Config(std::string host, int times, int ttl):host(host), times(times), ttl(ttl) {};
    Config(std::string host, int times, int ttl, int frenc, int timeout, int interval=-1);    // ping的配置接口
    Config();
    long get_id();
    std::string get_host();
    int get_port();
    int get_times();
    int get_ttl();
    int get_frenc();
    int get_timeout();
    int get_interval();
    std::string get_type();
    void set_id(int i);
    void set_host(std::string h);
    void set_port(int p);
    void set_times(int t);
    void set_ttl(int t);
    void set_frenc(int f);
    void set_timeout(int t);
    void set_interval(int i);
    void set_type(std::string t);
  private:
    long id;
    std::string host;
    int port;
    int times;
    int ttl;
    int frenc;      // 检测频率，即发次两次检测的时间间隔。比如一个ping检测。每次检测发送4个ping，每个ping间隔2秒。这样的发送4个ping的这样的检测每60秒进行一次。这里frenc就是60，interval就是2
    int timeout;    // 一次检测中每次ping的超时时间 单位是ms
    int interval;   // 一次检测中发送两次ping之间的时间间隔 单位是ms
    std::string type;
};

#endif
