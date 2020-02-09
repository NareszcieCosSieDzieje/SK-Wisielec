#include <QApplication>
#include <QDesktopWidget>
#include <thread>
#include "mainwindow.h"
#include "client.h"

Client *client;

void initializeClient();

int main(int argc, char *argv[])
{
    client = new Client;
    QApplication application(argc, argv);
    MainWindow w(client);
    client->GUI = &w;
    std::thread clientThread(initializeClient);
    int width = w.frameGeometry().width();
    int height = w.frameGeometry().height();
    QDesktopWidget wid;
    int screenWidth = wid.screen()->width();
    int screenHeight = wid.screen()->height();
    w.setGeometry((screenWidth/2)-(width/2),(screenHeight/2)-(height/2),width,height);
    w.show();
    application.exec();
}

void initializeClient()
{
    client->init();
}









