#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <iostream>
#include <string>
#include <client.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

using namespace std;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Client *cl, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButtonRegister_clicked();

    void on_pushButtonLogin_clicked();

    void on_tableOfServers_doubleClicked(const QModelIndex &index);

    void on_pushButtonSignup_clicked();

    void on_pushButtonGoToCreateSrv_clicked();

    void on_pushButtonCreateSrv_clicked();

    void on_pushButtonStart_clicked();

    void letterClicked();

private:
    Ui::MainWindow *ui;

    Client *client;

    string currentWord, hiddenWord;

    int badAnswers;

    string generateWord();

    void lettersSetEnabled(bool isEnabled);

    void setHangmanPicture(int badAnswers);

    char *QStringToChar(QString qs);
};
#endif // MAINWINDOW_H
