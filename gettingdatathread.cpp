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
    cout << "STARTED GETTING DATA" << endl;
    while (isGettingData) {
        if (gettingDataType == GettingDataType::Sessions) {
            cout << "( IN ) SESSIONS =========" << endl;
            client->activateConnectionProcess(ConnectionProcesses::SESSION_DATA);
            char msg[2048];
            client->readData(client->clientFd, msg, sizeof(msg));
            cout << "SESSIONS DATA = \"" << msg << "\"" << endl;
            std::map<int, std::pair<string, string>> sessions;
            if(msg[0] == '\0'){
                std::cout << "got NO-SESSIONS" << std::endl;
            } else {
                char *s;
                s = strtok(msg,":");
                long int numSessions = strtol(s, nullptr, 10);
                cout << "numSessions = " << numSessions << endl;
                for( int i =0; i < numSessions; i++ ) {
                    cout << "getter" << endl;
                    int sessionID = strtol(strtok(NULL, "-"), nullptr, 10);
                    cout << "getter" << endl;
                    string name = strtok(NULL, "-");
                    cout << "getter" << endl;
                    string host = strtok(NULL, ";");
                    cout << "getter" << endl;
                    sessions.insert(
                                std::pair<int, std::pair<string, string>>(sessionID,
                                std::pair<string, string> (name, host))
                    );
                }
            }
            client->availableSessions = &sessions;
            emit setSessionSig(sessions);
        } else if (gettingDataType == GettingDataType::Players) {
            cout << "( IN ) PLAYERS =========" << endl;
            char userDataProcess[sizeof(ConnectionProcesses::USER_DATA)];
            strcpy(userDataProcess, ConnectionProcesses::USER_DATA);
            char *userDataProcess2 = strcat(userDataProcess, "-");
            const char *process = strcat(userDataProcess2, client->inSessionID.c_str());
            cout << "process name: " << process << endl;
            client->activateConnectionProcess(process);
            char msg[512];
            client->readData(client->clientFd, msg, sizeof(msg));
            if (strcmp(msg, "SESSION-QUIT\0") == 0){
                emit onHostLeaveSig();
            } else {
                std::vector<std::string> players;
                std::cout << msg << std::endl;
                char* s;
                s = strtok(msg,":");
                std::cout << 's' << s << std::endl;
                long int numPlayers = strtol(s, nullptr, 10);
                std::cout << "num = " << numPlayers << std::endl;
                for( int i =0; i < numPlayers; i++ ) {
                    s = strtok(NULL, ",");
                    players.push_back(std::string(s));
                }
                emit setPlayersSig(players);
            }
        }
        cout << "( OUT ) THREAD LOOP =========" << endl;
        if (!isGettingData) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        cout << "( OUT ) THREAD LOOP AFTER SLEEP =========" << endl;
    }
}

void GettingDataThread::stopGettingData()
{
    isGettingData = false;
    cout << "set isGettingData = false" << endl;
    this->wait();
    cout << "KILLED AFTER WAIT" << endl;
}
