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
#include <thread>
#include <QMainWindow>

#include "statuses.hpp"
#include "player.hpp"
#include "constants.h"

class MainWindow;

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
    std::string login;
    std::string password;
    bool connected = true;
    bool gettingData = false;
    std::thread* dataGetterThread;
    MainWindow *GUI;
    GettingDataType gettingDataType;
    bool isHost = false;
    std::map<int, std::pair<std::string, std::string>> *availableSessions;


    Client();
    void init();
    void startClient(void);
    void closeClient(void);
    void startConnection(void);
    ssize_t readData(int fd, char * buffer, ssize_t buffsize);
    void writeData(int fd, char * buffer, ssize_t count);
    void sigHandler(int signal);
    int authorize(char *log, char *pass, int authKind);
    void activateConnectionProcess(const char *proccesName);
    void runDataGetter();
    void joinSession(int id);
    int createSession();
private:
    void dataGetter();
    int goToSession(int id);
};

#endif // CLIENT_H
