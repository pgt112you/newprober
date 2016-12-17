#include "configuration.h"


Config::Config() {
    host = "";
    times = -1;
    ttl = -1;
    frenc = -1;
    timeout = -1;
    interval = -1;
    id = -1;
}

Config::Config(std::string host, int times, int ttl, int frenc, int timeout, int interval) {
    this->host = host;
    this->times = times;
    this->ttl = ttl;
    this->frenc = frenc;
    this->timeout = timeout;
    this->interval = interval;
}

long Config::get_id() {
    return id;
}

void Config::set_id(int i) {
    id = i;
}

std::string Config::get_host() {
    return host;
}

int Config::get_port() {
    return port;
}

void Config::set_host(std::string h) {
    host = h;
}

void Config::set_port(int p) {
    port = p;
}

int Config::get_times() {
    return times;
}

void Config::set_times(int t) {
    times = t;
}

int Config::get_ttl() {
    return ttl;
}

void Config::set_ttl(int t) {
    ttl = t;
}

int Config::get_frenc() {
    return frenc;
}

void Config::set_frenc(int f) {
    frenc = f;
}

int Config::get_timeout() {
    return timeout;
}

std::string Config::get_type() {
    return type;
}

void Config::set_timeout(int t) {
    timeout = t;
}

int Config::get_interval() {
    return interval;
}

void Config::set_interval(int i) {
    interval = i;
}

void Config::set_type(std::string t) {
    type = t;
}


