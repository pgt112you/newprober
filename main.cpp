#include "tool.h"
#include "master.h"
#include "logger.h"


using std::string;

static Master master;

static void termhandler(int signo) {
    printf("in termhandler\n");
    event_base_loopbreak(master.get_base());
    exit(0);
}



int main(int argc, char *argv[]) {
    printf("begin in main\n");
    if(signal(SIGINT, termhandler) == SIG_ERR) {
        printf("can't set sigint sigterm\n");
    }
    if(signal(SIGTERM, termhandler) == SIG_ERR) {
        printf("can't set sigterm sigterm\n");
    }
    pid_t parent_pid = getpid();
    Logger logger;
    logger.set_level(LOG_LEVEL_INFO);
    logger.set_logfile("/root/newprober/access.log");
    event_enable_debug_mode();
    Master master;
    master.get_server_conf("/root/newprober/server.conf");    // 得到服务自身运行的一些配置，比如端口号什么的
    master.get_test_conf("/root/newprober/test.conf");    // 得到需要检测的项目的配置
    master.set_logger(&logger);
    printf("begin generate worker\n");
    master.generate_worker();    // 生成子进程                 
    if (parent_pid != getpid()) {  // 子进程                   
        // 这里面是不可能走到的
        char argv_buf[MAXLINE] = {0};                          
        setproctitle_init(argc, argv, environ);                
        setproctitle("%s %s %s", "newprober", "worker", argv_buf);
    }
    else {    // 主进程                                        
        //master.init();    // 初始化master
        char argv_buf[MAXLINE] = {0};                          
        for(int i = 1; i < argc; i++) {
            strcat(argv_buf, argv[i]);                         
            strcat(argv_buf, " ");
        }
        setproctitle_init(argc, argv, environ);
        setproctitle("%s %s %s", "newprober", "master", argv_buf);
        sleep(1);    // 等待子进程都启动起来了
        //master.will_distribute_conf(1);    // 向子进程分发检测配置   
        //master.will_split_log();
        logger.info("master run");
        master.run();    // 启动监听的端口，等待见新的任务   
    }
    return 0;
}
