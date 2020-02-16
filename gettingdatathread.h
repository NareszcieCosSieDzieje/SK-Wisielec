#ifndef GETTINGDATATHREAD_H
#define GETTINGDATATHREAD_H

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
#include <QThread>
#include <string>
#include <QMutex>

#include "statuses.hpp"
#include "player.hpp"
#include "client.h"
#include "constants.h"
#include "mainwindow.h"

using namespace std;

class Client;

class MainWindow;

class GettingDataThread : public QThread
{
    Q_OBJECT

public:

    GettingDataThread(Client * c);

    void run() override;
    GettingDataType gettingDataType;
    QMutex guiMutex;
    QMutex connectionMutex;

public slots:
    void stopGettingData();

signals:
    void setSessionSig(std::map<int, std::pair<std::string, std::string>>);
    void setPlayersSig(std::vector<std::string>);
    void onHostLeaveSig();
    void onGameStart(SessionStart sessionMessage);
    void onGameFinish(string winner);

private:
    Client * client;
    MainWindow * GUI;
    std::atomic<bool> isGettingData;
};

#endif // GETTINGDATATHREAD_H
