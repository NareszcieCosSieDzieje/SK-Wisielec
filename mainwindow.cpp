#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QStandardItemModel>
#include <QStandardItem>
#include <QMessageBox>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <QString>
#include <QCloseEvent>

#include "constants.h"
#include "mainwindow.h"

using namespace std;

MainWindow::MainWindow(Client *cl, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , client(cl)
{
    ui->setupUi(this);
    qRegisterMetaType<std::map<int, std::pair<std::string, std::string>>>( "std::map<int, std::pair<std::string, std::string>>" );
    qRegisterMetaType<std::vector<std::string>>( "std::vector<std::string>" );
    qRegisterMetaType<string>( "string" );
    qRegisterMetaType<SessionStart>( "SessionStart" );
    connect(client->gettingDataThread, SIGNAL(setSessionSig(std::map<int, std::pair<std::string, std::string>>)),
            this, SLOT(setSessions(std::map<int, std::pair<std::string, std::string>>)));
    connect(client->gettingDataThread, SIGNAL(setPlayersSig(std::vector<std::string>)),
            this, SLOT(setPlayers(std::vector<std::string>)));
    connect(client->gettingDataThread, SIGNAL(onHostLeaveSig()),
            this, SLOT(onHostLeave()));
    connect(client->gettingDataThread, SIGNAL(onGameStart(SessionStart)),
            this, SLOT(startGame(SessionStart)));
    connect(client->gettingDataThread, SIGNAL(onGameFinish(string)),
            this, SLOT(finishRound(string)));
    QObjectList buttons = ui->groupBoxLetters->children();
    for (int i = 0; i < 26; ++i) {
        connect(buttons[i], SIGNAL(clicked()), this, SLOT(letterClicked()));
    }
    ui->pages->setCurrentWidget(ui->pageLogin);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setSessions(std::map<int, std::pair<string, string>> sessions)
{
    client->gettingDataThread->guiMutex.lock();
    QStandardItemModel* model =  qobject_cast<QStandardItemModel *>(ui->tableOfServers->model());

    QModelIndexList indexes = ui->tableOfServers->selectionModel()->selectedIndexes();
    int selectedRow = -1;
    if (!indexes.empty())
        selectedRow = indexes.at(0).row();

    if (ui->tableOfServers->verticalHeader()->count()) {
        if (!model->removeRows(0, ui->tableOfServers->verticalHeader()->count())) {
            cout << "RowCount() = " << ui->tableOfServers->verticalHeader()->count() << endl;
            cout << "ERROR: error during clearing the table" << endl;
        }
    }
    int i = 0;
    for( auto const& [key, val] : sessions) {
        cout << "i = " << i << endl;
        string name = val.first;
        string host = val.second;
        model->setItem(i, 0, new QStandardItem(QString::fromStdString(name)));
        model->setItem(i, 1, new QStandardItem(QString::fromStdString(host)));
        i++;
    }
    if (selectedRow != -1) {
        setButtonEnabled(ui->pushButtonJoinSrv, true);
        ui->tableOfServers->selectRow(selectedRow);
    }
    else {
        clickedSessionIndex = 0;
        setButtonEnabled(ui->pushButtonJoinSrv, false);
    }
    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::setPlayers(std::vector<string> players)
{
    client->gettingDataThread->guiMutex.lock();
    if (client->isHost) setButtonEnabled(ui->pushButtonStart, players.size() > 1);
    QStandardItemModel* model =  qobject_cast<QStandardItemModel *>(ui->tableOfPlayers->model());

    QModelIndexList indexes = ui->tableOfPlayers->selectionModel()->selectedIndexes();
    int selectedRow = -1;
    if (!indexes.empty())
        selectedRow = indexes.at(0).row();

    if (ui->tableOfPlayers->verticalHeader()->count()) {
        if (!model->removeRows(0, ui->tableOfPlayers->verticalHeader()->count())) {
            cout << "RowCount() = " << ui->tableOfServers->verticalHeader()->count() << endl;
            cout << "ERROR: error during clearing the table" << endl;
        }
    }
    int i = 0;
    for( std::string player : players) {
        if (i == 0)
            playersScores.clear();
        playersScores.insert(std::pair(player, 0));
        model->setItem(i, 0, new QStandardItem(QString::fromStdString(player)));
        if (i) {
            model->setItem(i, 1, new QStandardItem("Player"));
        } else {
            model->setItem(i, 1, new QStandardItem("Host"));
        }
        if (player == client->login) {
            model->item(i, 0)->setBackground(Qt::darkBlue);
            model->item(i, 1)->setBackground(Qt::darkBlue);
        }
        i++;
    }
    if (selectedRow != -1) {
        ui->tableOfPlayers->selectRow(selectedRow);
    }

    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::onHostLeave()
{
    client->gettingDataThread->guiMutex.lock();
    client->gettingDataThread->stopGettingData();
    moveToSessionsPage();
    client->gettingDataThread->guiMutex.unlock();
}


char *MainWindow::QStringToChar(QString qs) {
    const char *c = qs.toStdString().c_str();
    return strdup(c);
}

void MainWindow::on_pushButtonRegister_clicked()
{
    client->gettingDataThread->guiMutex.lock();
    ui->pages->setCurrentWidget(ui->pageRegister);
    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::on_pushButtonLogin_clicked()
{
    QMessageBox msgBox;
    if (ui->lineEditLogin->text() == "" || ui->lineEditPwd->text() == "") {
        msgBox.setText("All fields are required");
        msgBox.exec();
    } else {
        client->gettingDataThread->guiMutex.lock();
        char *login = QStringToChar(ui->lineEditLogin->text());
        char *password = QStringToChar(ui->lineEditPwd->text());
        switch (client->authorize(login, password, AuthorizationType::SIGNIN)) {
        case AuthorizationStatus::SUCCESSFUL: {
            moveToSessionsPage();
            break;
        }
        case AuthorizationStatus::FAILED: {
            msgBox.setText("Login failed! Bad username or password.");
            msgBox.exec();
            break;
        }
        case AuthorizationStatus::ERROR: {
            msgBox.setText("Ow, something went wrong. Please try again later.");
            msgBox.exec();
            break;
        }
        }
        client->gettingDataThread->guiMutex.unlock();
    }
}

void MainWindow::on_tableOfServers_clicked(const QModelIndex &index)
{
    client->gettingDataThread->guiMutex.lock();
    clickedSessionIndex = index.row();
    setButtonEnabled(ui->pushButtonJoinSrv, true);
    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::on_pushButtonJoinSrv_clicked()
{
    client->gettingDataThread->guiMutex.lock();
    QStandardItemModel* model =  qobject_cast<QStandardItemModel *>(ui->tableOfServers->model());
    std::string host = model->item(clickedSessionIndex, 1)->text().toStdString();
    int id;
    for(auto &elem : *(client->availableSessions)) {
        if (elem.second.second == host) {
            id = elem.first;
            break;
        }
    }
    std::cout << "JOINING SERVER" << std::endl;
    client->gettingDataThread->stopGettingData();
    QMessageBox msgBox;
    switch(client->goToSession(id)) {
    case SessionMessage::JOINED:
        setButtonEnabled(ui->pushButtonStart, false);
        moveToSessionPage();
        break;
    case SessionMessage::BUSY:
        msgBox.setText("Sorry, this server is full");
        msgBox.exec();
        client->gettingDataThread->start();
        break;
    case SessionMessage::KILLED:
        msgBox.setText("Sorry, the server doesn't exist");
        msgBox.exec();
        client->gettingDataThread->start();
        break;
    case SessionMessage::SESSION_ERROR:
        msgBox.setText("Ow, something went wrong. Please try again later.");
        msgBox.exec();
        client->gettingDataThread->start();
        break;
    case SessionMessage::MAX:
        msgBox.setText("Ow, something went wrong. Please try again later.");
        msgBox.exec();
        client->gettingDataThread->start();
        break;
    }
    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::on_pushButtonSignup_clicked()
{
    QMessageBox msgBox;
    if (ui->lineEditRegLogin->text() == "" || ui->lineEditRegPwd1->text() == "" || ui->lineEditRegPwd1->text() == "") {
        msgBox.setText("All fields are required");
        msgBox.exec();
    } else {
        client->gettingDataThread->guiMutex.lock();
        if (ui->lineEditRegPwd1->text() == ui->lineEditRegPwd2->text()) {
            char *login = QStringToChar(ui->lineEditRegLogin->text());
            char *password = QStringToChar(ui->lineEditRegPwd1->text());
            QMessageBox msgBox;
            switch (client->authorize(login, password, AuthorizationType::SIGNUP)) {
            case AuthorizationStatus::SUCCESSFUL: {
                moveToSessionsPage();
                break;
            }
            case AuthorizationStatus::FAILED: {
                msgBox.setText("User with that username already exists");
                msgBox.exec();
                break;
            }
            case AuthorizationStatus::ERROR: {
                msgBox.setText("Ow, something went wrong. Please try again later.");
                msgBox.exec();
                break;
            }
            }
        } else {
            msgBox.setText("Password fields must contain the same value");
            msgBox.exec();
        }
        client->gettingDataThread->guiMutex.unlock();
    }
}

void MainWindow::on_pushButtonGoToCreateSrv_clicked()
{
    client->gettingDataThread->guiMutex.lock();
    ui->pages->setCurrentWidget(ui->pageCreateSrv);
    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::onLostedConnection() {
    QMessageBox msgBox;
    msgBox.setText("Sorry, the server is down. Try again later.");
    msgBox.exec();
    terminate();
}

void MainWindow::on_pushButtonCreateSrv_clicked()
{
    cout << ">>>>>>>> ( IN ) CREATE CONN" << endl;
    client->gettingDataThread->guiMutex.lock();
    std::cout << "CREATING SERVER" << std::endl;
    client->gettingDataThread->stopGettingData();
    QMessageBox msgBox;
    switch(client->createSession()) {
    case SessionMessage::CREATED:
        setButtonEnabled(ui->pushButtonStart, true);
        moveToSessionPage();
        client->gettingDataThread->guiMutex.unlock();
        cout << ">>>>>>>> ( OUT ) CREATE CONN" << endl;
        break;
    case SessionMessage::MAX:
        client->gettingDataThread->guiMutex.unlock();
        msgBox.setText("Sorry, too many servers are created");
        msgBox.exec();
        break;
    }
}

void MainWindow::moveToSessionPage() {
    QStandardItemModel *model =  new QStandardItemModel(0, 1);
    model->setHorizontalHeaderItem(0, new QStandardItem("Player name"));
    model->setHorizontalHeaderItem(1, new QStandardItem("Role"));
    ui->tableOfPlayers->setModel(model);
    ui->tableOfPlayers->horizontalHeader()->resizeSection(0, 220);
    ui->tableOfPlayers->horizontalHeader()->setStretchLastSection(true);
    ui->tableOfPlayers->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableOfPlayers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableOfPlayers->verticalHeader()->hide();
    ui->labelSrvName->setText(QString::fromStdString(client->currentSessionName));
    setButtonEnabled(ui->pushButtonStart, false);
    ui->pages->setCurrentWidget(ui->pageWaitingRoom);
    client->gettingDataThread->gettingDataType = GettingDataType::Players;
    client->gettingDataThread->start();
}

void MainWindow::lettersSetEnabled(bool isEnabled)
{
    QObjectList buttons = ui->groupBoxLetters->children();
    for (int i = 0; i < 26; ++i) {
        QAbstractButton *ptr = qobject_cast<QAbstractButton*>(buttons[i]);
        setButtonEnabled(ptr, isEnabled);
    }
}

void MainWindow::setButtonEnabled(QAbstractButton * button, bool enabled) {
    button->setEnabled(enabled);
    if (enabled) {
        button->setStyleSheet("background-color: rgb(68, 10, 10); color: rgb(243, 243, 243)");
    } else {
        button->setStyleSheet("background-color: rgb(30, 4, 4); color: rgb(140, 140, 140)");
    }
    client->gettingDataThread->guiMutex.unlock();
}

string MainWindow::getSrvName() {
    return ui->lineEditSrvName->text().toStdString();
}

void MainWindow::setHangmanPicture(int badAnswers)
{
    string s = ":/resources/img/s";
    s.append(to_string(badAnswers));
    s.append(".jpg");
    QString qs = QString::fromStdString(s);
    QPixmap img(qs);
    ui->labelHangman->setPixmap(img.scaled(ui->labelHangman->width(), ui->labelHangman->height(), Qt::KeepAspectRatio));
}

void MainWindow::on_pushButtonStart_clicked()
{
    client->gettingDataThread->connectionMutex.lock();
    client->activateConnectionProcess(ConnectionProcesses::START_SESSION);
    setButtonEnabled(ui->pushButtonStart, false);
    ui->labelWinner->setVisible(false);
    client->gettingDataThread->connectionMutex.unlock();
}

void MainWindow::startGame(SessionStart sessionMessage) {
    client->gettingDataThread->guiMutex.lock();
    sendExitInfoToServer = false;
    QMessageBox msgBox;
    switch (sessionMessage) {
    case SessionStart::OK:
        client->gettingDataThread->stopGettingData();
        ui->PlayerInfo1->setVisible(false);
        ui->PlayerInfo2->setVisible(false);
        ui->PlayerInfo3->setVisible(false);
        client->startGame();
        break;
    case SessionStart::FAIL:
        setButtonEnabled(ui->pushButtonStart, true);
        msgBox.setText("You cannot play without other players");
        msgBox.exec();
        break;
    }
    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::prepareRound(std::string word) {
    lettersSetEnabled(false);
    ui->pages->setCurrentWidget(ui->pageGame);
    ui->labelCounter->setVisible(true);
    QPixmap pImg(":/resources/img/p0.jpg");
    ui->labelProgress1->setPixmap(pImg);
    int i = 0;
    for (std::pair<std::string, int> player : playersScores) {
        if (player.first != client->login) {
            i++;
            switch (i) {
            case 1:
                ui->labelPlayerName1->setText(QString::fromStdString(player.first));
                ui->labelPoints1->setText(QString::fromStdString(to_string(player.second)));
                ui->labelProgress1->setPixmap(pImg);
                ui->PlayerInfo1->setVisible(true);
                break;
            case 2:
                ui->labelPlayerName2->setText(QString::fromStdString(player.first));
                ui->labelPoints2->setText(QString::fromStdString(to_string(player.second)));
                ui->labelProgress2->setPixmap(pImg);
                ui->PlayerInfo2->setVisible(true);
                break;
            case 3:
                ui->labelPlayerName3->setText(QString::fromStdString(player.first));
                ui->labelPoints3->setText(QString::fromStdString(to_string(player.second)));
                ui->labelProgress3->setPixmap(pImg);
                ui->PlayerInfo3->setVisible(true);
                break;
            }
        }
    }
    this->repaint();
    for (int sec = 3; sec >= 1; --sec) {
        ui->labelCounter->setText(QString::fromStdString(to_string(sec)));
        this->repaint();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    ui->labelWinner->setVisible(false);
    ui->labelHangman->setVisible(true);
    ui->labelCounter->setVisible(false);
    currentWord = word;
    hiddenWord.clear();
    for (int i = 0; i < int(currentWord.size()); ++i) {
        if (currentWord[i] == ' ')
        {
            hiddenWord.append(" ");
        }
        else
        {
            hiddenWord.append("_");
        }
    }
    lettersSetEnabled(true);
    startTimeMeasuring = std::chrono::steady_clock::now();
    badAnswers = 0;
    setHangmanPicture(badAnswers);
    ui->labelWord->setText(QString::fromStdString(hiddenWord));
    client->gettingDataThread->gettingDataType = GettingDataType::Game;
    client->gettingDataThread->start();
}

void MainWindow::letterClicked()
{
    client->gettingDataThread->guiMutex.lock();
    QAbstractButton *button = qobject_cast<QAbstractButton*>(sender());
    setButtonEnabled(button, false);
    string letter = button->text().toStdString();
    bool correct = false;
    while (currentWord.find(letter) != string::npos)
    {
        hiddenWord[currentWord.find(letter)] = letter[0];
        currentWord[currentWord.find(letter)] = '*';
        correct = true;
    }
    if (!correct) {
        badAnswers++;
        setHangmanPicture(badAnswers);
        if (badAnswers == 9) {
            lettersSetEnabled(false);
            client->onRoundFinish(false);
        }
    }
    int checker = 0;
    while ((currentWord[checker] == '*') || (currentWord[checker] == ' ')) {
        checker++;
    }
    if (checker == int(currentWord.size())) {
        lettersSetEnabled(false);
        client->onRoundFinish(true);
    }
    ui->labelWord->setText(QString::fromStdString(hiddenWord));
    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::finishRound(string winner) {
    client->gettingDataThread->guiMutex.lock();
    cout << "beforeround start" << endl;
    lettersSetEnabled(false);
    for (std::pair<std::string, int> player : playersScores) {
        if (player.first == winner) {
            playersScores.at(player.first)++;
            if (player.first == client->login) {
                ui->labelScorePoints->setText(QString::fromStdString(to_string(++playerScore)));
            }
            break;
        }
    }
    ui->labelWinner->setVisible(true);
    ui->labelHangman->setVisible(false);
    if (winner != "") {
        winner.append(" won the round");
        ui->labelWinner->setText(QString::fromStdString(winner));
    } else {
        ui->labelWinner->setText("No one won the game :(");
    }
    client->startRound();
    cout << "afterround start" << endl;
    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::moveToSessionsPage() {
    QStandardItemModel* model =  new QStandardItemModel(0, 1);
    model->setHorizontalHeaderItem(0, new QStandardItem("Server name"));
    model->setHorizontalHeaderItem(1, new QStandardItem("Host"));
    ui->tableOfServers->setModel(model);
    ui->tableOfServers->horizontalHeader()->setStretchLastSection(true);
    ui->tableOfServers->horizontalHeader()->resizeSection(0, 400);
    ui->tableOfServers->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableOfServers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableOfServers->verticalHeader()->hide();
    setButtonEnabled(ui->pushButtonJoinSrv, false);
    ui->pages->setCurrentWidget(ui->pageSessions);
    client->gettingDataThread->gettingDataType = GettingDataType::Sessions;
    client->gettingDataThread->start();
}

void MainWindow::on_pushButtonBackToSessions_clicked()
{
    client->gettingDataThread->guiMutex.lock();
    moveToSessionsPage();
    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::on_pushButtonBackToLogin_clicked()
{
    client->gettingDataThread->guiMutex.lock();
    ui->pages->setCurrentWidget(ui->pageLogin);
    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::on_pushButtonLogout_clicked()
{
    client->gettingDataThread->guiMutex.lock();
    client->gettingDataThread->stopGettingData();
    client->activateConnectionProcess(ConnectionProcesses::LOGOUT);
    ui->pages->setCurrentWidget(ui->pageLogin);
    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::closeEvent (QCloseEvent *event)
{
    client->gettingDataThread->guiMutex.lock();
    if (sendExitInfoToServer)
        client->activateConnectionProcess(ConnectionProcesses::DISCONNECT);
    event->accept();
    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::on_pushButtonLeave_clicked()
{
    client->gettingDataThread->guiMutex.lock();
    cout << "######## ( IN ) LEAVE" << endl;
    client->gettingDataThread->stopGettingData();
    client->activateConnectionProcess(ConnectionProcesses::SESSION_OUT);
    moveToSessionsPage();
    cout << "######## ( OUT ) LEAVE" << endl;
    client->gettingDataThread->guiMutex.unlock();
}

void MainWindow::closeOnMaxPlayers() {
    sendExitInfoToServer = false;
    QMessageBox msgBox;
    msgBox.setText("Sorry, server is overloaded. Try again later.");
    msgBox.exec();
    close();
}
