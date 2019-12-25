#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cerrno>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "utils.h"

// 创建套接字并进行绑定
static int socket_bind(const char *ip, int port);

// IO多路复用epoll
static void do_epoll(int listenfd);

// 添加事件
static void add_event(int epollfd, int fd, int state);

// 修改事件
static void modify_event(int epollfd, int fd, int state);

// 删除事件
static void del_event(int epollfd, int fd, int state);

// 事件处理函数
static void handle_events(int epollfd, struct epoll_event *events, int num, int listenfd, char *buf);

// 处理接受到的连接
static void handle_accept(int epollfd, int listenfd);

// 读处理
static void do_read(int epollfd, int fd, char *buf);

// 写处理
static void do_write(int epollfd, int fd, char *buf);

int main(int argc, char *argv[]) {
    int listenfd;
    listenfd = socket_bind(SERVER_IP, SERVER_PORT);
    listen(listenfd, SOMAXCONN);
    do_epoll(listenfd);
    return 0;
}

static int socket_bind(const char *ip, int port) {
    int listenfd;
    // 创建监听
    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("socket error:");
        exit(1);
    }
    printf("listenfd socket created \n");
    struct sockaddr_in sockaddr;
    bzero(&sockaddr, sizeof(sockaddr));  // memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = PF_INET;
    inet_pton(PF_INET, ip, &sockaddr.sin_addr);
    sockaddr.sin_port = htons(port);
    // 绑定地址
    if (bind(listenfd, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) == -1) {
        perror("bind error:");
        exit(1);
    }
    return listenfd;
}

static void do_epoll(int listenfd) {
    int epollfd;
    struct epoll_event events[EPOLLEVENTS];
    int ret;
    char buf[MAXSIZE];
    bzero(&buf, sizeof(buf));
    // 创建描述符
    epollfd = epoll_create(EPOLL_SIZE);
    if (epollfd == -1) {
        printf("epollfd created error:%s \n", strerror(errno));
        exit(1);
    }
    printf("epollfd created succ, epollfd = %d\n", epollfd);
    // 添加监听描述符事件
    add_event(epollfd, listenfd, EPOLLIN);
    while(1) {
        // 获取已经准备好的描述符事件
        ret = epoll_wait(epollfd, events, EPOLLEVENTS, -1);
        handle_events(epollfd, events, ret, listenfd, buf);
    }
    close(epollfd);
}

static void handle_events(int epollfd, struct epoll_event *events, int num, int listenfd, char *buf) {
    int fd;
    for (int i = 0; i < num; ++i) {
        fd = events[i].data.fd;
        // 根据描述符的类型和事件类型进行处理
        if ((fd == listenfd) && (events[i].events & EPOLLIN)) {
            handle_accept(epollfd, listenfd);
        } else if (events[i].events & EPOLLIN) {
            do_read(epollfd, fd, buf);
        } else if (events[i].events & EPOLLOUT) {
            do_write(epollfd, fd, buf);
        }
    }
}

static void handle_accept(int epollfd, int listenfd) {
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    client_fd = accept(listenfd, (struct sockaddr *) &client_addr, &client_len);
    if (client_fd == -1) {
        perror("accept new client error \n");
        exit(1);
    } else {
        printf("accept new client succ:%s:%d\n", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
        add_event(epollfd, client_fd, EPOLLIN);
    }
}

static void do_read(int epollfd, int fd, char *buf) {
    int nread = read(fd, buf, MAXSIZE);
    if (nread == -1) {
        perror("read error\n");
        close(fd);
        del_event(epollfd, fd, EPOLLIN);
    } else if (nread == 0) {
        perror("client close\n");
        close(fd);
        del_event(epollfd, fd, EPOLLIN);
    } else {
        printf("read msg is:%s", buf);
        modify_event(epollfd, fd, EPOLLOUT);
    }
}

static void do_write(int epollfd, int fd, char *buf) {
    int nwrite = write(fd, buf, strlen(buf));
    if (nwrite == -1) {
        perror("write error\n");
        close(fd);
        del_event(epollfd, fd, EPOLLOUT);
    } else {
        modify_event(epollfd, fd, EPOLLIN);
        memset(buf, 0, MAXSIZE);
    }
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

