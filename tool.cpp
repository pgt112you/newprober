#include "tool.h"



void tv_sub(struct timeval *out, struct timeval *in) {
    if ((out->tv_usec -= in->tv_usec) < 0) {
        --out->tv_sec;
        out->tv_usec += 1000000;
    }
    out->tv_sec -= in->tv_sec;
}



uint16_t in_cksum(uint16_t *addr, int len) {
    int nleft = len;
    uint32_t sum = 0;
    uint16_t *w = addr;
    uint16_t answer = 0;

    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char*)w;
        sum += answer;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return (answer);
}



extern char **environ;
static char **g_main_Argv = NULL; /* pointer to argument vector */
static char *g_main_LastArgv = NULL; /* end of argv */

void setproctitle_init(int argc, char **argv, char **envp) {
    int i;

    for (i = 0; envp[i] != NULL; i++) // calc envp num
        continue;
    environ = (char **) malloc(sizeof (char *) * (i + 1)); // malloc envp pointer
    for (i = 0; envp[i] != NULL; i++)
    {
        //environ[i] = (char *)malloc(sizeof(char) * strlen(envp[i]));
        char *temp = (char *)malloc(sizeof(char) * strlen(envp[i])+1);
        bzero(temp, sizeof(char)*strlen(envp[i])+1);
        environ[i] = temp;
        strcpy((char *)environ[i], (char *)envp[i]);
    }
    environ[i] = NULL;

    g_main_Argv = argv;
    if (i > 0)
      g_main_LastArgv = envp[i - 1] + strlen(envp[i - 1]);
    else
      g_main_LastArgv = argv[argc - 1] + strlen(argv[argc - 1]);
}

void setproctitle(const char *fmt, ...) {
    char *p;
    int i;
    char buf[MAXLINE];

    extern char **g_main_Argv;
    extern char *g_main_LastArgv;
    va_list ap;
    p = buf;

    va_start(ap, fmt);
    vsprintf(p, fmt, ap);
    va_end(ap);

    i = strlen(buf);

    if (i > g_main_LastArgv - g_main_Argv[0] - 2)
    {
        i = g_main_LastArgv - g_main_Argv[0] - 2;
        buf[i] = '\0';
    }
    (void) strcpy(g_main_Argv[0], buf);

    p = &g_main_Argv[0][i];
    while (p < g_main_LastArgv)
        *p++ = '\0';
    g_main_Argv[1] = NULL;
    prctl(PR_SET_NAME, buf);
}

int recv_openfd(int fd) {
    struct msghdr msg;
    struct iovec iov[1];
    size_t n;
    char ptr;
    int recvfd;
    union {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;
    struct cmsghdr *cmptr;
    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    iov[0].iov_base = &ptr;
    iov[0].iov_len = 1;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    if ((n = recvmsg(fd, &msg, 0)) < 0) {
        printf("recvmsg error\n");
        return -1;
    }
    if ((n = recvmsg(fd, &msg, 0)) == 0) {
        //printf("fd close by peer\n");
        return -1;
    }
    if (((cmptr = CMSG_FIRSTHDR(&msg)) != NULL) && (cmptr->cmsg_len = CMSG_LEN(sizeof(int)))) {
        if (cmptr->cmsg_level != SOL_SOCKET) {
            printf("cmsg level error\n");
            return -1;
        }
        if (cmptr->cmsg_type != SCM_RIGHTS) {
            printf("cmsg type error\n");
            return -1;
        }
        recvfd = *((int *)CMSG_DATA(cmptr));
    }
    else {
        recvfd = -1;
    }
    return recvfd;
}

void send_openfd(int fd, int openfd) {
    struct msghdr msg;
    struct iovec iov[1];
    union {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;
    struct cmsghdr *cmptr;

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);
    cmptr = CMSG_FIRSTHDR(&msg);
    cmptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;
    *((int *)CMSG_DATA(cmptr)) = openfd;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    iov[0].iov_base = (void *)"";
    iov[0].iov_len = 1;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    sendmsg(fd, &msg, 0);
    return;
}
