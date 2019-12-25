#include <netinet/in.h>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <cstdio>
#include <cstdlib>
#include "utils.h"

static void handle_connect(int sockfd);

static void handle_events(int epollfd, struct epoll_event *events, int num, int sockfd, char *buf);

static void do_read(int epollfd, int fd, int sockfd, char *buf);

static void do_write(int epollfd, int fd, int sockfd, char *buf);

// 添加事件
static void add_event(int epollfd, int fd, int state);

// 修改事件
static void modify_event(int epollfd, int fd, int state);

// 删除事件
static void del_event(int epollfd, int fd, int state);

int main(int argc, char *argv[]) {
    int res, sockfd;
    char buf[100];
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket error\n");
        exit(1);
    }
    struct sockaddr_in sockaddr;
    bzero(&sockaddr, sizeof(sockaddr));
    sockaddr.sin_family = PF_INET;
    sockaddr.sin_port = htons(SERVER_PORT);
    inet_pton(PF_INET, SERVER_IP, &sockaddr.sin_addr);
    res = connect(sockfd, (struct sockaddr *) &sockaddr, sizeof(sockaddr));
    if (res == -1) {
        perror("error : cannot connect the server!\n");
        exit(1);
    }
    // socket方式
//    stpcpy(buf, "您好");
//    write(sockfd, buf, strlen(buf));
    handle_connect(sockfd);
    return 0;
}

static void handle_connect(int sockfd) {
    int epollfd, ret;
    char buf[MAXSIZE];

    struct epoll_event events[EPOLLEVENTS];
    // 创建描述符
    epollfd = epoll_create(EPOLL_SIZE);
    if (epollfd == -1) {
        perror("epollfd created error \n");
        exit(1);
    }
    printf("epollfd created succ, epollfd = %d\n", epollfd);
    // 添加监听描述符事件
    add_event(epollfd, STDIN_FILENO, EPOLLIN);
    while (1) {
        ret = epoll_wait(epollfd, events, EPOLLEVENTS, -1);
        handle_events(epollfd, events, ret, sockfd, buf);
    }
    close(epollfd);
}

static void handle_events(int epollfd, struct epoll_event *events, int num, int sockfd, char *buf) {
    int fd;
    for (int i = 0; i < num; ++i) {
        fd = events[i].data.fd;
        // 根据描述符的类型和事件类型进行处理
        if (events[i].events & EPOLLIN) {
            do_read(epollfd, fd, sockfd, buf);
        } else if (events[i].events & EPOLLOUT) {
            do_write(epollfd, fd, sockfd, buf);
        }
    }
}

static void do_read(int epollfd, int fd, int sockfd, char *buf) {
    int nread;
    nread = read(fd, buf, MAXSIZE);
    if (nread == -1) {
        perror("read error\n");
        close(fd);
    } else if (nread == 0) {
        fprintf(stderr, "server close.\n");
        close(fd);
    } else {
        if (fd == STDIN_FILENO) {  //输入
            add_event(epollfd, sockfd, EPOLLOUT);
        } else {
            del_event(epollfd, sockfd, EPOLLIN);
            add_event(epollfd, STDOUT_FILENO, EPOLLOUT);
        }
    }
}

static void do_write(int epollfd, int fd, int sockfd, char *buf) {
    int nwrite;
    nwrite = write(fd, buf, strlen(buf));
    if (nwrite == -1) {
        perror("write error");
        close(fd);
    } else {
        if (fd == STDOUT_FILENO) {
            del_event(epollfd, fd, EPOLLOUT);
        } else {
            modify_event(epollfd, fd, EPOLLIN);
        }
    }
    memset(buf, 0, MAXSIZE);
}

static void add_event(int epollfd, int fd, int state) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = state;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
    printf("fd added to epoll!\n");
}

static void modify_event(int epollfd, int fd, int state) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = state;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
    printf("fd mod to epoll!\n");
}

static void del_event(int epollfd, int fd, int state) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = state;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
    printf("fd del to epoll!\n");
}
