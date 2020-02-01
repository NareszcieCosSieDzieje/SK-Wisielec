#include <QApplication>
#include <QDesktopWidget>
#include <thread>
#include "mainwindow.h"
#include "client.h"

Client *client;

void initializeClient();

int main(int argc, char *argv[])
{
    client = new Client;;
    std::thread clientThread(initializeClient);

    QApplication application(argc, argv);
    MainWindow w(client);
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









