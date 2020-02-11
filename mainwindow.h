#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <iostream>
#include <string>
#include <client.h>
#include <QStandardItem>

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
    void setSessions(std::map<int, std::pair<string, string> > sessions);
    void setPlayers(std::vector<string> players);

    string getSrvName();
private slots:
    void on_pushButtonRegister_clicked();

    void on_pushButtonLogin_clicked();

    void on_tableOfServers_doubleClicked(const QModelIndex &index);

    void on_pushButtonSignup_clicked();

    void on_pushButtonGoToCreateSrv_clicked();

    void on_pushButtonCreateSrv_clicked();

    void on_pushButtonStart_clicked();

    void letterClicked();

    void on_pushButtonBackToSessions_clicked();

    void on_pushButtonBackToLogin_clicked();

    void on_pushButtonLogout_clicked();

private:
    Ui::MainWindow *ui;

    Client *client;

    string currentWord, hiddenWord;

    int badAnswers;

    QStandardItemModel *sessionsListModel;

    string generateWord();

    void lettersSetEnabled(bool isEnabled);

    void setHangmanPicture(int badAnswers);

    char *QStringToChar(QString qs);

    void moveToSessionsPage();

    void moveToSessionPage();

    void closeEvent (QCloseEvent *event);
};

#endif // MAINWINDOW_H
