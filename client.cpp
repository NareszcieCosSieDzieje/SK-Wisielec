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
    char msg[20];
    readData(clientFd, msg, sizeof(msg));
    cout << "msg read 20" << msg << endl;
    if (strcmp("SERVER-OK\0", msg) == 0) {
        while (connected) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        std::cout << "Out of main loop" << std::endl;
        closeClient();
    } else if (strcmp("SERVER-MAX\0", msg) == 0) {
        cout << "session max" << endl;
        closeClient();
        GUI->closeOnMaxPlayers();
    }
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
    activateConnectionProcess(ConnectionProcesses::VALIDATION);
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
                std::cout << "no sessions" << std::endl; //TODO: brak sesji
            } else {
                char *s;
                s = strtok(msg,":");
                long int numSessions = strtol(s, nullptr, 10);
                for( int i =0; i < numSessions; i++ ) {
                    int sessionID = strtol(strtok(NULL, "-"), nullptr, 10);
                    string name = strtok(NULL, "-");
                    string host = strtok(NULL, ";");
                    sessions.insert(std::pair<int, std::pair<string, string>>(sessionID, std::pair<string, string> (name, host)));
                }
            }
            availableSessions = &sessions;
            GUI->setSessions(sessions);
        } else if (gettingDataType == GettingDataType::Players) {
            char userDataProcess[sizeof(ConnectionProcesses::USER_DATA)];
            strcpy(userDataProcess, ConnectionProcesses::USER_DATA);
            char *userDataProcess2 = strcat(userDataProcess, "-");
            const char *process = strcat(userDataProcess2, inSessionID.c_str());
            cout << "process name: " << process << endl;
            activateConnectionProcess(process);
            char msg[512];
            readData(clientFd, msg, sizeof(msg));
            if (strcmp(msg, "SESSION-QUIT\0") == 0){
                std::cout << "sesja is dead" << std::endl; //TODO wyrzuć z sesji
            } else {
                std::vector<std::string> players;
                std::cout << msg << std::endl;
                char* s;
                s = strtok(msg,":");
                std::cout << 's' << s << std::endl;
                long int numPlayers = strtol(s, nullptr, 10);
                std::cout << "num = " << numPlayers << std::endl;
                for( int i =0; i < numPlayers; i++ ) {
                    s = strtok(NULL, ",");
                    players.push_back(std::string(s));
                }
                GUI->setPlayers(players);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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
    char msg1[20];
    snprintf(msg1, sizeof(msg1), "%d", id);
    writeData(clientFd, msg1, sizeof(msg1));
    if (id == 0) {
        char msg3[100]; //TODO: SPRAWDZ CZY NAZWA SESJI MIESCI SIE W TYM !!!!!!!!!!!
        std::string s = GUI->getSrvName();
        strcpy(msg3, s.c_str());
        writeData(clientFd, msg3, sizeof(msg3));
    }
    char msg2[100];
    readData(clientFd, msg2, sizeof(msg2));
    cout << "po readzie" << endl;
    if (strncmp(msg2, "SESSION-MAX\0", 9) == 0) {
        return SessionMessage::MAX;
    } else if (strcmp(msg2, "SESSION-BUSY\0") == 0) { // TODO
        return SessionMessage::BUSY;
    } else if (strcmp(msg2, "SESSION-KILLED\0") == 0) { // TODO
        return SessionMessage::KILLED;
    } else if (strcmp(msg2, "SESSION-GOOD\0") == 0) { // TODO
        char msgNum[20];
        readData(clientFd, msgNum, sizeof(msgNum));
        inSessionID = std::string(msgNum);
        std::cout << "inSessionID: " << inSessionID << std::endl;
        if (id == 0) {
            isHost = true;
            return SessionMessage::CREATED;
        } else {
            return SessionMessage::JOINED;
        }
    } else {
        std::cout << "ERROR. Readed data: " << msg2 << std::endl;
        return SessionMessage::SESSION_ERROR;
    }
}

void Client::killGettingDataProcess()
{
    gettingData = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

int Client::createSession() {
    return goToSession(0);
}
