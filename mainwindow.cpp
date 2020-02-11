#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QStandardItemModel>
#include <QStandardItem>
#include <QMessageBox>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <QString>

#include "constants.h"

using namespace std;

MainWindow::MainWindow(Client *cl, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , client(cl)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setSessions(std::map<int, std::pair<string, string>> sessions)
{
    QStandardItemModel* model =  qobject_cast<QStandardItemModel *>(ui->tableOfServers->model());
    int i = 0;
    for( auto const& [key, val] : sessions) {
        int sessionID = key;
        string name = val.first;
        string host = val.second;
        model->setItem(i, 0, new QStandardItem(QString::fromStdString(name)));
        model->setItem(i, 1, new QStandardItem(QString::fromStdString(host)));
        i++;
    }
}

void MainWindow::setPlayers(std::vector<string> players)
{
    QStandardItemModel* model =  qobject_cast<QStandardItemModel *>(ui->tableOfPlayers->model());
    int i = 0;
    for( std::string player : players) {
        cout << "PETLA -----------" << endl;
        cout << i << ": " << player << endl;
        model->setItem(i, 0, new QStandardItem(QString::fromStdString(player)));
        if ((client->isHost) && (client->login == player)) {
            model->setItem(i, 1, new QStandardItem("Host"));
        } else {
            model->setItem(i, 1, new QStandardItem("Player"));
        }
        i++;
        cout << "-----------------" << endl;
    }
}


char *MainWindow::QStringToChar(QString qs) {
    const char *c = qs.toStdString().c_str();
    return strdup(c);
}

void MainWindow::on_pushButtonRegister_clicked()
{
    ui->pages->setCurrentWidget(ui->pageRegister);
}

void MainWindow::on_pushButtonLogin_clicked()
{
    char *login = QStringToChar(ui->lineEditLogin->text());
    char *password = QStringToChar(ui->lineEditPwd->text());
    QMessageBox msgBox;
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


}

void MainWindow::on_tableOfServers_doubleClicked(const QModelIndex &index)
{
    QStandardItemModel* model =  qobject_cast<QStandardItemModel *>(ui->tableOfServers->model());
    std::string host = model->item(index.row(), 1)->text().toStdString();
    cout << "host: " << host << endl;
    int id;
    for(auto &elem : *(client->availableSessions)) {
        if (elem.second.second == host) {
            id = elem.first;
            break;
        }
    }
    cout << "id: " << id << endl;
    std::cout << "JOINING SERVER" << std::endl;
    client->gettingData = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    switch(client->goToSession(id)) {
    case SessionMessage::JOINED:
        moveToSessionPage();
        break;
    }
}

void MainWindow::on_pushButtonSignup_clicked()
{
    QMessageBox msgBox;
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
}

void MainWindow::on_pushButtonGoToCreateSrv_clicked()
{
    ui->pages->setCurrentWidget(ui->pageCreateSrv);
}

void MainWindow::on_pushButtonCreateSrv_clicked()
{
    std::cout << "CREATING SERVER" << std::endl;
    client->gettingData = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    switch(client->createSession()) {
    case SessionMessage::CREATED:
        moveToSessionPage();
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
    ui->labelSrvName->setText(ui->lineEditSrvName->text());
    ui->pages->setCurrentWidget(ui->pageWaitingRoom);
    client->gettingDataType = GettingDataType::Players;
    client->runDataGetter();
}

void MainWindow::lettersSetEnabled(bool isEnabled)
{
    QObjectList buttons = ui->groupBoxLetters->children();
    for (int i = 0; i < 26; ++i) {
        QAbstractButton *ptr = qobject_cast<QAbstractButton*>(buttons[i]);
        ptr->setEnabled(isEnabled);
        if (isEnabled) {
            ptr->setStyleSheet("background-color: rgb(68, 10, 10); color: rgb(243, 243, 243)");
        } else {
            ptr->setStyleSheet("background-color: rgb(30, 4, 4); color: rgb(140, 140, 140)");
        }
    }
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
    QObjectList buttons = ui->groupBoxLetters->children();
    for (int i = 0; i < 26; ++i) {
        connect(buttons[i], SIGNAL(clicked()), this, SLOT(letterClicked()));
    }
    lettersSetEnabled(false);
    ui->pages->setCurrentWidget(ui->pageGame);
    this->repaint();
    for (int sec = 3; sec >= 1; --sec) {
        ui->labelCounter->setText(QString::fromStdString(to_string(sec)));
        this->repaint();
        sleep(1);
    }
    ui->labelCounter->setVisible(false);
    currentWord = generateWord();
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
    setHangmanPicture(0);
    QPixmap pImg(":/resources/img/p2.jpg");
    ui->labelProgress1->setPixmap(pImg);
    ui->labelWord->setText(QString::fromStdString(hiddenWord));
}

void MainWindow::letterClicked()
{
    QAbstractButton *button = qobject_cast<QAbstractButton*>(sender());
    button->setEnabled(false);
    button->setStyleSheet("background-color: rgb(30, 4, 4); color: rgb(140, 140, 140)");
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
        }
    }
    int checker = 0;
    while ((currentWord[checker] == '*') || (currentWord[checker] == ' ')) {
        checker++;
    }
    if (checker == int(currentWord.size())) {
        lettersSetEnabled(false);
    }
    ui->labelWord->setText(QString::fromStdString(hiddenWord));
}

string MainWindow::generateWord()
{
    srand(time(NULL));
    const int n = 3;
    QString words[n] = {"CAR", "MOUN TAIN", "ROOMM ATE"};
    return words[rand() % n].toStdString();
}

void MainWindow::moveToSessionsPage() {
    ui->tableOfServers->horizontalHeader()->resizeSection(0, 400);
    ui->tableOfServers->horizontalHeader()->setStretchLastSection(true);
    ui->tableOfServers->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableOfServers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableOfServers->verticalHeader()->hide();
    ui->pages->setCurrentWidget(ui->pageSessions);
    QStandardItemModel* model =  new QStandardItemModel(0, 1);
    model->setHorizontalHeaderItem(0, new QStandardItem("Server name"));
    model->setHorizontalHeaderItem(1, new QStandardItem("Host"));
    ui->tableOfServers->setModel(model);
    client->gettingDataType = GettingDataType::Sessions;
    client->runDataGetter();
}

void MainWindow::on_pushButtonBackToSessions_clicked()
{
    moveToSessionsPage();
}

void MainWindow::on_pushButtonBackToLogin_clicked()
{
    ui->pages->setCurrentWidget(ui->pageLogin);
}
