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
#include <QMetaObject>
#include <qobjectdefs.h>
#include <string.h>

#include "statuses.hpp"
#include "player.hpp"
#include "client.h"
#include "constants.h"
#include "mainwindow.h"

class MainWindow;

Client::Client()
{

}

void Client::init(){
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    startClient();
    startConnection();
    while (connected) {
        sleep(0.5);
    }
    std::cout << "Out of main loop" << std::endl;
    closeClient();
    return;
}

void Client::activateConnectionProcess(const char* proccesName) {
    char msg[512];
    strcpy(msg, proccesName);
    writeData(clientFd, msg, sizeof(msg));
}

void Client::startClient(void){
    if((clientFd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
        perror("Failed to create socket.\n");
        exit(SOCKET_CREATE);
    }
}

void Client::closeClient(void){
    shutdown(clientFd, SHUT_RDWR);
    close(clientFd);
}

void Client::startConnection(void){
    int err = getaddrinfo("127.0.0.1", "55555", &hints, &resolved);
    if (err || !resolved){
        perror("Resolving address failed!\n");
        exit(GETADDRINFO_ERROR);
    }
    if ( connect(clientFd, resolved->ai_addr, resolved->ai_addrlen) < 0){
        perror("Failed to connect.\n");
        exit(SOCKET_CONNECT);
    }
    freeaddrinfo(resolved);
    activateConnectionProcess(ConnectionProcesses::VALIDATION);
}

ssize_t Client::readData(int fd, char * buffer, ssize_t buffsize){
    auto ret = read(fd, buffer, buffsize);
    std::cout << "Read " << ret << " bytes from server" << std::endl;
    if(ret==-1) {
        // printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
        perror("Read failed\n");
    }
    return ret;
}

void Client::writeData(int fd, char * buffer, ssize_t count){
    auto ret = write(fd, buffer, count);
    std::cout << "Send " << ret << " bytes to server" << std::endl;
    if(ret==-1) perror("Sending failed\n");
    if(ret!=count) perror("Send less than requested to server\n");
}

int Client::authorize(char *log, char *pass, int authKind) {
    char msg1[10];
    snprintf(msg1, sizeof(msg1), "%d", authKind);
    writeData(clientFd, msg1, sizeof(msg1));
    char msg2[100];
    strcpy(msg2, log);
    strcat(msg2, "-");
    strcat(msg2, pass);
    writeData(clientFd, msg2, sizeof(msg2));
    memset(msg2, 0, sizeof(msg2));
    readData(clientFd, msg2, sizeof(msg2));
    if (strncmp(msg2, "AUTH-FAIL", 9) == 0) {
        return AuthorizationStatus::FAILED;
    } else if (strncmp(msg2, "AUTH-OK", 7) == 0) {
        login = log;
        password = pass;
        return AuthorizationStatus::SUCCESSFUL;
    } else {
        return AuthorizationStatus::ERROR;
    }
}

void Client::dataGetter() {
    while (gettingData) {
        if (gettingDataType == GettingDataType::Sessions) {
            activateConnectionProcess(ConnectionProcesses::SESSION_DATA);
            char msg[2048];
            readData(clientFd, msg, sizeof(msg));
            std::map<int, std::pair<string, string>> sessions;
            if(msg[0] == '\0'){
                std::cout << "no sessions" << std::endl;
            } else {
                char *s;
                s = strtok(msg,":");
                long int numSessions = strtol(s, nullptr, 10);
                for( int i =0; i < numSessions; i++ ) {
                    int sessionID = strtol(strtok(msg, "-"), nullptr, 10);
                    string name = strtok(msg, "-");
                    string host = strtok(msg, "-");
                    sessions.insert(std::pair<int, std::pair<string, string>>(sessionID, std::pair<string, string> (name, host)));
                }
            }
            availableSessions = &sessions;
            GUI->setSessions(sessions);
        } else if (gettingDataType == GettingDataType::Players) {
            char userDataProcess[sizeof(ConnectionProcesses::USER_DATA)];
            strcpy(userDataProcess, ConnectionProcesses::USER_DATA);
            const char *process = strcat(userDataProcess, "-1"); //TODO: popraw
            activateConnectionProcess(process);
            char msg[512];
            readData(clientFd, msg, sizeof(msg));
            std::vector<std::string> players;
            if (strcmp(msg, "SESSION-QUIT\0") == 0){
                std::cout << "sesja is dead" << std::endl;
            } else {
                std::cout << msg << std::endl;
                char* s;
                s = strtok(msg,":");
                std::cout << "2" << std::endl;
                std::cout << 's' << s << std::endl;
                long int numPlayers = strtol(s, nullptr, 10);
                std::cout << "num = " << numPlayers << std::endl;
                for( int i =0; i < numPlayers; i++ ) {
                    s = strtok(msg, ",");
                    std::cout << "3" << std::endl;
                    players.push_back(std::string(s));
                }
                std::cout << "przeszlo" << std::endl;
                GUI->setPlayers(players);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void Client::runDataGetter() {
    gettingData = true;
    std::thread getterThread(&Client::dataGetter, this);
    getterThread.detach();
    dataGetterThread = &getterThread;
}

int Client::goToSession(int id) {
    activateConnectionProcess(ConnectionProcesses::SESSION_JOIN);
    std::cout<< "joi session wyslane" << std::endl;
    char msg1[20];
    snprintf(msg1, sizeof(msg1), "%d", id);
    writeData(clientFd, msg1, sizeof(msg1));
    if (id == 0) {
        char msg3[100];
        std::string s = GUI->getSrvName();
        strcpy(msg3, s.c_str());
        writeData(clientFd, msg3, sizeof(msg3));
    }
    char msg2[100];
    readData(clientFd, msg2, sizeof(msg2));
    if (strncmp(msg2, "SESSION-MAX\0", 9) == 0) {

    } else if (strcmp(msg2, "SESSION-BUSY\0") == 0) {

    } else if (strcmp(msg2, "SESSION-MAX\0") == 0) {

    } else if (strcmp(msg2, "SESSION-KILLED\0") == 0) {

    } else if (strcmp(msg2, "SESSION-GOOD\0") == 0) {
        isHost = true;
        return SessionMessage::CREATED;
    } else {
        std::cout << "msg2: " << msg2 << std::endl;
        std::cout << msg2 << std::endl;
    }
}

int Client::createSession() {
    return goToSession(0);
}

/*
int recv_all(int sockfd, void *buf, size_t len, int flags)
{
    size_t toread = len;
    char  *bufptr = (char*) buf;

    while (toread > 0)
    {
        ssize_t rsz = recv(sockfd, bufptr, toread, flags);
        if (rsz <= 0)
            return rsz;  // Errr or other end closed cnnection

        toread -= rsz;  // Read less next time
        bufptr += rsz;  // Next buffer position to read into
    }

    return len;
}

*/
