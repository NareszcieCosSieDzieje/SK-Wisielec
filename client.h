#ifndef CLIENT_H
#define CLIENT_H
#include <netinet/in.h>
#include <netdb.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/epoll.h>
#include <cstring>
#include <csignal>
#include <string>
#include <map>
#include <vector>
#include <errno.h>
#include <iostream>

#include "statuses.hpp"
#include "player.hpp"

class Client
{
public:
    int clientFd{};
    const unsigned int localPort{59997};
    const unsigned int remotePort{55555};
    sockaddr_in bindAddr {
            .sin_family = AF_INET,
            .sin_port = htons(localPort)
    };
    addrinfo hints {
            .ai_flags= 0,
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = IPPROTO_TCP};
    addrinfo *resolved;
    int joinedSessionID{};
    char *login;
    char *password;
    bool connected = true;


    Client();
    void init();
    void startClient(void);
    void closeClient(void);
    void startConnection(void);
    ssize_t readData(int fd, char * buffer, ssize_t buffsize);
    void writeData(int fd, char * buffer, ssize_t count);
    void sigHandler(int signal);
    int tryToLogin(char *log, char *pass);
};

#endif // CLIENT_H
