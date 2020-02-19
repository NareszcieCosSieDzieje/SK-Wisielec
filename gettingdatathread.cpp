#include "gettingdatathread.h"
#include "mainwindow.h"

#include "client.h"

class MainWindow;

GettingDataThread::GettingDataThread(Client *c)
{
    client = c;
    GUI = client->GUI;
}

void GettingDataThread::run()
{
    isGettingData = true;
    while (isGettingData) {
        connectionMutex.lock();
        if (gettingDataType == GettingDataType::Sessions) {
            client->activateConnectionProcess(ConnectionProcesses::SESSION_DATA);
            char msg[2048];
            client->readData(client->clientFd, msg, sizeof(msg));
            std::map<int, std::pair<string, string>> sessions;
            cout << "MSG = " << msg << endl;
            if(msg[0] == '\0'){
                std::cout << "NO-SESSIONS" << std::endl;
            } else {
                char *s;
                s = strtok(msg,":");
                long int numSessions = strtol(s, nullptr, 10);
                for( int i =0; i < numSessions; i++ ) {
                    int sessionID = strtol(strtok(NULL, "-"), nullptr, 10);
                    string name = strtok(NULL, "-");
                    string host = strtok(NULL, ";");
                    sessions.insert(
                                std::pair<int, std::pair<string, string>>(sessionID,
                                std::pair<string, string> (name, host))
                    );
                }
            }
            client->availableSessions = &sessions;
            emit setSessionSig(sessions);
        } else if (gettingDataType == GettingDataType::Players) {
            char userDataProcess[sizeof(ConnectionProcesses::USER_DATA)];
            strcpy(userDataProcess, ConnectionProcesses::USER_DATA);
            char *userDataProcess2 = strcat(userDataProcess, "-");
            const char *process = strcat(userDataProcess2, client->inSessionID.c_str());
            client->activateConnectionProcess(process);
            char msg[512];
            client->readData(client->clientFd, msg, sizeof(msg));
            if (strcmp(msg, "SESSION-QUIT\0") == 0){
                emit onHostLeaveSig();
            } else if (strcmp(msg, "START-SESSION-OK\0") == 0) {
                emit onGameStart(SessionStart::OK, false);
            } else if (strcmp(msg, "START-SESSION-FAIL\0") == 0) {
                emit onGameStart(SessionStart::FAIL, false);
            } else {
                std::vector<std::string> players;
                std::cout << msg << std::endl;
                char* s;
                s = strtok(msg,":");
                std::cout << 's' << s << std::endl;
                long int numPlayers = strtol(s, nullptr, 10);
                for( int i =0; i < numPlayers; i++ ) {
                    s = strtok(NULL, ",");
                    players.push_back(std::string(s));
                }
                emit setPlayersSig(players);
            }
        } else if (gettingDataType == GettingDataType::Game) {
            char msg[600];
            client->readData(client->clientFd, msg, sizeof(msg));
            if (strcmp(msg, "WIN-0\0") == 0) {
                emit onGameFinish("");
                connectionMutex.unlock();
                break;
            } else if (strncmp(msg, "WIN-", 4) == 0) {
                strtok(msg, "-");
                char *c = strtok(NULL, "-");
                std::string winner(c);
                emit onGameFinish(winner);
                connectionMutex.unlock();
                break;
            } else {
                std::map<std::string, int> playersProgresses;
                char* s;
                s = strtok(msg,":");
                std::cout << 's' << s << std::endl;
                long int numPlayers = strtol(s, nullptr, 10);
                for( int i =0; i < numPlayers; i++ ) {
                    s = strtok(NULL, ":");
                    std::string player(s);
                    s = strtok(NULL, "-");
                    int progress = strtol(s, nullptr, 10);
                    s = strtok(NULL, ",");
                    playersProgresses.insert(std::pair(player, progress));
                }
                emit setPlayersProgressesSig(playersProgresses);
            }
        }
        connectionMutex.unlock();
        if (!isGettingData) break;
        if (!(gettingDataType == GettingDataType::Game))
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

void GettingDataThread::stopGettingData()
{
    isGettingData = false;
    this->wait();
}
