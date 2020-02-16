#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <iostream>
#include <string>
#include <client.h>
#include <QStandardItem>
#include <QAbstractButton>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

using namespace std;

class Client;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Client *cl, QWidget *parent = nullptr);
    ~MainWindow();

    string getSrvName();
    void closeOnMaxPlayers();
    void onLostedConnection();
    void startGame();

    bool sendExitInfoToServer = true;

private slots:
    void on_pushButtonRegister_clicked();

    void on_pushButtonLogin_clicked();

    void on_pushButtonSignup_clicked();

    void on_pushButtonGoToCreateSrv_clicked();

    void on_pushButtonCreateSrv_clicked();

    void on_pushButtonStart_clicked();

    void letterClicked();

    void on_pushButtonBackToSessions_clicked();

    void on_pushButtonBackToLogin_clicked();

    void on_pushButtonLogout_clicked();

    void on_pushButtonLeave_clicked();

    void setSessions(std::map<int, std::pair<std::string, std::string> > sessions);

    void setPlayers(std::vector<std::string> players);

    void onHostLeave();

    void on_tableOfServers_clicked(const QModelIndex &index);

    void on_pushButtonJoinSrv_clicked();

    void startGame(SessionStart sessionMessage);
private:
    Ui::MainWindow *ui;

    Client *client;

    int clickedSessionIndex;

    string currentWord, hiddenWord;

    int badAnswers;

    std::atomic<bool> isTableCllickedOnce = false;

    QStandardItemModel *sessionsListModel;

    string generateWord();

    void lettersSetEnabled(bool isEnabled);

    void setHangmanPicture(int badAnswers);

    char *QStringToChar(QString qs);

    void moveToSessionPage();

    void closeEvent (QCloseEvent *event);

    void setButtonEnabled(QAbstractButton *button, bool enabled);

    void moveToSessionsPage();
};

#endif // MAINWINDOW_H
