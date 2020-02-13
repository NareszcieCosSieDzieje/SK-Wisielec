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

#include "statuses.hpp"
#include "player.hpp"
#include "client.h"
#include "constants.h"
#include "mainwindow.h"

class Client;

class MainWindow;

class GettingDataThread : public QThread
{
    Q_OBJECT

public:

    GettingDataThread(Client * c);

    void run() override;
    GettingDataType gettingDataType;

public slots:
    void stopGettingData();

private:
    Client * client;
    MainWindow * GUI;
};

#endif // GETTINGDATATHREAD_H
