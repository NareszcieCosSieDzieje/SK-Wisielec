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
#include "gettingdatathread.h"

class MainWindow;

class GettingDataThread;

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
    std::string login;
    std::string password;
    std::string inSessionID;
    bool connected = true;
    MainWindow *GUI;
    std::map<int, std::pair<std::string, std::string>> *availableSessions;
    bool isHost = false;
    std::string currentSessionName;
    char *srvAddr;


    Client(char *addr);
    GettingDataThread *gettingDataThread;
    void init();
    void startClient(void);
    void closeClient(void);
    void startConnection();
    ssize_t readData(int fd, char * buffer, ssize_t buffsize);
    void writeData(int fd, char * buffer, ssize_t count);
    int authorize(char *log, char *pass, int authKind);
    void activateConnectionProcess(const char *proccesName);
    void joinSession(int id);
    int createSession();
    int goToSession(int id);
    int startSession();
    void startGame();
    void onRoundFinish(bool winner);
    void startRound();
private:
    void dataGetter();
    void waitForPlayers();
    bool validateIpAddress(const std::string &ipAddress);
};

#endif // CLIENT_H
