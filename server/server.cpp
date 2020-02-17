#include <thread>
#include <vector>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>
#include <sys/epoll.h>
#include <cstring>
#include <map>
#include <mutex>
#include <errno.h>
#include <iostream>
#include <set>
#include <algorithm>
#include <atomic>
#include <arpa/inet.h>
#include <condition_variable>

#include "../statuses.hpp"
#include "../player.hpp"
#include "../player.cpp"
#include "../data_loader.hpp"


//=====================================GLOBALS============================================\\

std::vector<std::thread> threadVector;

std::vector<int> clientSockets; 
std::mutex clientSocketsMutex;

std::map<int, Player> clientMap;
std::mutex clientMapMutex;

std::map<int, std::vector<Player>> playerSessions;
std::mutex playerSessionsMutex;

std::map<int, std::vector<int>> playerSessionsFds;
std::mutex playerSessionsFdsMutex;

std::map<int, std::string> sessionHosts;
std::mutex sessionHostsMutex;

std::map<int, std::string> sessionNames;
std::mutex sessionNamesMutex;

std::map<int, std::string> sessionStartData;
std::mutex sessionStartDataMutex;

std::mutex writeToFileMutex;

std::mutex conditionMutex;
std::unique_lock<std::mutex> uqLock(conditionMutex);
std::condition_variable cV;

std::map<int, int> sendUserDataCounter;
std::mutex sendUserDataCounterMutex;

int epollFd{};
int serverFd{};
const unsigned int localPort{55555};
sockaddr_in bindAddr {
        .sin_family = AF_INET,
        .sin_port = htons(localPort),
        //.sin_addr.s_addr = inet_addr("127.0.0.1");
        //.sin_addr = htonl(INADDR_ANY)
};

int maxSessions = 2;
int playersPerSession = 4;
const int maxEvents = maxSessions * playersPerSession;

std::atomic<bool> SERVER_SHUT_DOWN(false);


//========================================FUNC PROTO========================================\\


ssize_t readData(int fd, char * buffer, ssize_t buffsize);
void writeData(int fd, char * buffer, ssize_t count);
void sigHandler(int signal);
void startServer(void);
void listenLoop(void);
void sendAvailableSessions(void);
void stopConnection(int ClientFd);
void clientValidation(int newClientFd);
void sendSessionData(int clientSocket);
void sendUserData(int clientSocket, char* msg);
void sessionLoop(int sessionID);
void joinSession(int clientFd);
void addToEpoll(int fd);
void removeFromEpoll(int fd);
void handlePlayerExit(int clientFd);
bool validateIpAddress(const std::string &ipAddress);
void updateCurrentPlayers(int sessionId, std::map<std::string, int> &currentPlayersFd);
std::string loadUserData(char* filePath); 


//=======================================MAIN=============================================\\

int main(int argc, char* argv[]){

	if (argc > 1){ 
		if (!validateIpAddress(argv[1])){
			perror("Wrong ip address format!\n");
			exit(WRONG_IP);
		} else {
			inet_pton(AF_INET, argv[1], &(bindAddr.sin_addr));
		}
	} else {
    	bindAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	}

    srand (time(NULL)); //SEEDOWANIE W TYM

    signal(SIGINT, sigHandler);
    signal(SIGTSTP, sigHandler);

    startServer();

    struct epoll_event events[maxEvents];

    bool polling = true;

    while(polling && !SERVER_SHUT_DOWN) {

        int event_count = epoll_wait(epollFd, events, maxEvents, -1);
        //printf("Ready events: %d\n", event_count);
        for (int i = 0; i < event_count; i++) {
            
        	if (event_count < 0){
        		perror("Epoll events < 0\n");
        		break;
        	}

            struct epoll_event clientEvent = events[i];
            int clientFd = clientEvent.data.fd;
            
            //printf("Czytanie z klienta o deskryptorze: '%d' -- \n", clientFd);

            char msg[512];
            int ret = readData(clientFd, msg, sizeof(msg));

            clientMapMutex.lock();
            std::cout << "\n----------------(W EPOLLU!)---------------------\nClient map: "<< std::endl;
            for(auto &a: clientMap){
                std::cout << "ID gracza = " << a.first << std::endl; 
                std::cout << "Nick gracza = " << a.second.getNick() << std::endl;
            }
            clientMapMutex.unlock();
            playerSessionsMutex.lock();
            std::cout << "\nPlayer sessions: "<< std::endl;
            for(auto &a: playerSessions){
                std::cout << "ID sesji = " << a.first << std::endl; 
                for(auto &b : a.second){
                    std::cout << "Nick gracza = " << b.getNick() << std::endl;  
                }
            }
            playerSessionsMutex.unlock();
            playerSessionsFdsMutex.lock();
            std::cout << "\nPlayer sessions FDS: "<< std::endl;
            for(auto &a: playerSessionsFds){
                std::cout << "ID sesji = " << a.first << std::endl; 
                for(auto &b : a.second){
                    std::cout << "deskryptor gracza = " << b << std::endl;  
                }
            }
            playerSessionsFdsMutex.unlock();
            sessionHostsMutex.lock();
            std::cout << "\nHOSTOWIE SESJI: "<< std::endl;
            for(auto &a: sessionHosts){
                std::cout << "ID sesji = " << a.first << std::endl; 
                std::cout << "Host nick = " << a.second << std::endl;
            }
            sessionHostsMutex.unlock();
            sessionNamesMutex.lock();
            std::cout << "\nNAZWY SESJI: "<< std::endl;
            for(auto &a: sessionNames){
                std::cout << "ID sesji = " << a.first << std::endl; 
                std::cout << "Nazwa sesji = " << a.second << std::endl;
            }
            sessionNamesMutex.unlock();
            
            std::cout << "\nDeskryptor w epollu fd = " << clientFd << "\t Dane = " << msg << std::endl;
			
            if(ret == 0){
                //usuwany
                continue;
            }
            if ( strcmp(msg, "CLIENT-VALIDATION\0") == 0){
                removeFromEpoll(clientFd);
                std::thread validationThread(clientValidation, clientFd); //Nowe połączenie przeslij do zweryfikowania
                validationThread.detach();
            } else if (strcmp(msg, "SEND-SESSION-DATA\0") == 0) {
                sendSessionData(clientFd);
            } else if ( strncmp(msg, "SEND-USER-DATA", 14) == 0){
				sendUserData(clientFd, msg);
            }
            else if( strcmp(msg, "JOIN-SESSION\0") == 0){
                std::cout << "JOIN SESSION" << std::endl;

                removeFromEpoll(clientFd);
                std::thread jS(joinSession, clientFd);
                jS.detach();
            } else if (strcmp(msg, "START-SESSION\0") == 0){
                std::cout << "START-SESSION" << std::endl;
                int session = 0;
                std::string msg;

                clientMapMutex.lock();
                std::string nick = clientMap[clientFd].getNick();
 				clientMapMutex.unlock();

                bool found = false;

				playerSessionsMutex.lock();
                for(auto &elem: playerSessions){
                    for(auto &plr : elem.second){
                        if (plr.getNick() == nick){
                            session = elem.first;
                            found = true;
                            break;
                        }
                    }
                    if (found){
                        break;
                    }
                }
                int sessionSize = playerSessions[session].size();
                playerSessionsMutex.unlock();
                if ( sessionSize >= 2){
                    msg = "START-SESSION-OK";
                    
                    playerSessionsFdsMutex.lock();

                    playerSessionsFdsMutex.unlock();
                    sessionStartDataMutex.lock();
                    sessionStartData.insert(std::pair<int, std::string>(session, msg));
                    sessionStartDataMutex.unlock();

                    sendUserDataCounterMutex.lock();
                    sendUserDataCounter.insert(std::pair<int,int>(session,0));
                    sendUserDataCounterMutex.unlock();

                    std::thread sT(sessionLoop, session);
                    sT.detach();
                } else {
                    msg = "START-SESSION-FAIL";
                    sessionStartDataMutex.lock();
                    sessionStartData.insert(std::pair<int, std::string>(session, msg));
                    sessionStartDataMutex.unlock();
                }
            } else if (strcmp(msg, "DISSOCIATE-SESSION\0") == 0) {
            	//Wyjdz przed startem sesji
                std::cout << "DISSOCIATE-SESSION" << std::endl;
            	handlePlayerExit(clientFd);
            }
            else if( strcmp(msg, "DISCONNECTING\0") == 0){
                std::cout << "DISCONNECTING" << std::endl;
                removeFromEpoll(clientFd);
                //handlePlayerExit(clientFd); //FIXME: nowe obczaj czy ok!
                stopConnection(clientFd);
            }
            else if ( strcmp(msg,"LOG-OUT\0") == 0){
                std::cout << "LOG-OUT" << std::endl;
            	handlePlayerExit(clientFd);
                clientMapMutex.lock();
                clientMap.erase(clientFd);
                clientMapMutex.unlock();
            }
        }
    }

    if (close(epollFd) < 0){
        perror("Server can't close epoll! - error.\n");
        exit(EPOLL_CLOSE);
    }


    /*
    while (!threadVector[0].joinable()){

    }
    threadVector[0].join();
	*/

    return 0;
}


//=======================================FUNC-DEC=========================================\\


bool validateIpAddress(const std::string &ipAddress){
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr));
    return result == 1;
}


void startServer(void){
    if( (serverFd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
        perror("Server failed to create socket.\n");
        exit(SOCKET_CREATE);
    }
    int enable = 1;
    if( setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0 ){
        perror("Setsockopt(SO_REUSEADDR) in server failed.\n");
        exit(SOCKET_REUSE);
    }
    if( bind(serverFd, (sockaddr*)&bindAddr, sizeof(bindAddr)) < 0 ){
        perror("Server failed to bind the socket.\n");
        exit(SOCKET_BIND);
    }
    if( listen(serverFd, 5) < 0 ){
        perror("Server failed to listen.\n");
        exit(SOCKET_LISTEN);
    }
    printf("1\n");
    epollFd = epoll_create1(0);
    if (epollFd < 0){
        perror("Server epoll create error!\n");
        exit(EPOLL_ERROR);
    }
    std::thread loopThread(listenLoop); //Uruchom wątek nasłuchujący nowych połączeń.
    threadVector.push_back(std::move(loopThread)); //Wątek nie może być skopiowany.
}



void listenLoop(void){
    while(true){
        sockaddr_in clientAddr{};
        socklen_t cliLen = sizeof(clientAddr);

        int newClient = accept(serverFd, (struct sockaddr *)&clientAddr, &cliLen); //Nawiąż nowe połączenie z klientem.
        if (newClient < 0) {
           if (errno == EINVAL && SERVER_SHUT_DOWN) {
               return;
           } else {
               perror("Server ERROR on accept.\n");
               exit(SOCKET_ACCEPT);
           }
        }

        clientSocketsMutex.lock();
        int sockets = clientSockets.size();
        clientSocketsMutex.unlock();
        char msg[20];
        if (sockets == maxEvents) {
        	strcpy(msg, "SERVER-MAX\0");
        	writeData(newClient, msg, sizeof(msg));
        	stopConnection(newClient);
        } else {
            clientSocketsMutex.lock();
            clientSockets.push_back(newClient);
            clientSocketsMutex.unlock();
        	strcpy(msg, "SERVER-OK\0");
        	writeData(newClient, msg, sizeof(msg));
        	addToEpoll(newClient);
        } 
    }
}


void clientValidation(int newClientFd){
    //TODO: Czy sprawdzać port klienta?

    const unsigned int signin = 1;
    const unsigned int signup = 2;

    char conType[10];
    auto ret = readData(newClientFd, conType, sizeof(conType));
    if(ret != 10){
        perror("User data sending error 1.\n");
        return;
    }
    int cT = (int) strtol(conType, nullptr, 10);
    char msg[100];
    ret = readData(newClientFd, msg, sizeof(msg) );
    if(ret != 100){
        perror("User data sending error 2.\n");
        return;
    }
    char * pch;
    pch = strtok(msg, "-");
    char* login;
    char* pass;
    if(pch != nullptr ){
        login = pch;
        pch = strtok(nullptr, "-");
    }
    if(pch != nullptr ){
        pass = pch;
    }

    bool userExists = false;
    std::string loginS(login);
    std::string passwordS(pass);

    if(cT == signup){
        if (!searchForUserData(loginS, passwordS, true)) {
        	writeToFileMutex.lock();
            addUser(loginS, passwordS);
            writeToFileMutex.unlock();
            userExists = true;
        } //else na koncu obejmuje jak nie przejdzie czyli AUTH-FAIL
    } else if (cT == signin){
    	bool loggedIn = false;
    	for(auto &c: clientMap){ // jesli juz zalogowany na dany nick
    		if (c.second.getNick() == loginS){
				loggedIn = true;
    		}
    	}
    	if(!loggedIn){
        	userExists = searchForUserData(loginS, passwordS, false); //WYWOŁANIE FUNKCJI CZYTAJĄCEJ Z PLIKU
    	}
    }
    char authMsg[100];
    if (userExists) {
        Player newPlayer(login, pass);
        //Dodaj do mapy klientow -graczy
        clientMapMutex.lock();
        clientMap.insert(std::pair<int, Player>(newClientFd, newPlayer));
        clientMapMutex.unlock();
        
        strcpy(authMsg, "AUTH-OK\0");  //Wyslij ack ze sie zalogował
        writeData(newClientFd, authMsg, sizeof(authMsg));
        addToEpoll(newClientFd);
    } else {
        strcpy(authMsg, "AUTH-FAIL\0");
        writeData(newClientFd, authMsg, sizeof(authMsg));
        addToEpoll(newClientFd);
    }
}


void joinSession(int clientFd){ 
    Player player = clientMap[clientFd];
    std::string nicker =  std::string(player.getNick());
    int sessionMode = -1;
    int finalSessionId = -1;

    char buf[100];
    char msg[100];
    char sessionId[20];
    int ret = readData(clientFd, sessionId, sizeof(sessionId)); // int read = recv(clientFd, sessionId, sizeof(sessionId), MSG_DONTWAIT); 
    if(ret != 20){
        perror("Join session read error 1.\n");
        return;
    }
    
    sessionMode = (int) strtol(sessionId, NULL, 10);
    memset(sessionId, 0 , sizeof(sessionId));
    bool secondMsg = false;

    if(sessionMode < 0){
        perror("Server join session error - sessionMode < 0\n");
    }
    else if (sessionMode == 0){ //TODO: Czy jak usuniete niskie wartosci to uzywanie od nowa? szczegol tho
	    ret = readData(clientFd, buf, sizeof(buf)); 
	   
	    if(ret != 100){
	        perror("Join session read error 2.\n");
	        return;
	    }

	    playerSessionsMutex.lock();
	    bool emptySessions = playerSessions.empty();
	    int sessionSize = playerSessions.size();
	    playerSessionsMutex.unlock();

        if ( emptySessions || (sessionSize < maxSessions) ) {
        	
        	strcpy(msg, "SESSION-GOOD\0");
        	secondMsg = true;
        	if ( emptySessions ){
	            finalSessionId = 1;
		        strcpy(sessionId, "1\0");       
        	} 
        	else if ( sessionSize < maxSessions )   {
	            finalSessionId = (int) sessionSize + 1;
	            sprintf (sessionId, "%d", finalSessionId);
	        }

        	std::vector<Player> playerVector;
	        std::vector<int> playerFds;
	        playerVector.push_back(player);
	        playerFds.push_back(clientFd);
            playerSessionsMutex.lock();
            playerSessions.insert(std::pair<int, std::vector<Player>>(finalSessionId, playerVector));
            playerSessionsMutex.unlock();
            playerSessionsFdsMutex.lock();
            playerSessionsFds.insert(std::pair<int, std::vector<int>>(finalSessionId, playerFds));
            playerSessionsFdsMutex.unlock();
            sessionNamesMutex.lock();
            sessionNames.insert(std::pair<int, std::string>(finalSessionId, std::string(buf) ));
            sessionNamesMutex.unlock();
            sessionHostsMutex.lock();
            sessionHosts.insert(std::pair<int, std::string>(finalSessionId, nicker ));
			sessionHostsMutex.unlock();

            memset(buf, 0, sizeof(buf));
		} else {            //error nie mozna zrobic sesji;
            strcpy(msg, "SESSION-MAX\0");
        }
        writeData(clientFd, msg, sizeof(msg));
        if (secondMsg){
        	writeData(clientFd, sessionId, sizeof(sessionId)); 
        }
    } else {
        playerSessionsMutex.lock();
        if (playerSessions.count(sessionMode) != 1){ //Jak nie ma klucza
            strcpy(msg, "SESSION-KILLED\0"); 
        }
        else {
        	if (playerSessions[sessionMode].size() < playersPerSession){
	            playerSessions[sessionMode].push_back(player);
	            
                playerSessionsFdsMutex.lock();
                playerSessionsFds[sessionMode].push_back(clientFd);
                playerSessionsFdsMutex.unlock();

	            sprintf (sessionId, "%d", sessionMode);
	            strcpy(msg, "SESSION-GOOD\0");
	            //strcat(msg, "\0");
	            secondMsg = true;
            } else {
                strcpy(msg, "SESSION-BUSY\0");
            }
        }
        playerSessionsMutex.unlock();
        writeData(clientFd, msg, sizeof(msg));
        if (secondMsg){
        	writeData(clientFd, sessionId, sizeof(sessionId)); 
        }
    }
    addToEpoll(clientFd);
}


//    id-nazwa-host
void sendSessionData(int clientSocket){ 
	char data[2048]; //is it enough			
	memset(data, 0, sizeof(data));

    playerSessionsMutex.lock();
    int sessionSize = playerSessions.size();
    if ( sessionSize > 0){
  		char num[10];
        sprintf (num, "%d", sessionSize );
        strcpy(data, num);
      	strcat(data, ":");
        for( auto const& [key, val] : playerSessions) {
            int sessionID = key;
            char num[10];
            sprintf (num, "%d", key);
            strcat(data, num);
            strcat(data, "-");
            strcat(data, sessionNames[sessionID].c_str()); // nazwa sesji
            strcat(data, "-");
            strcat(data, sessionHosts[sessionID].c_str()); //NICK HOSTA
            strcat(data, ";");
        }
    } else {
    	strcpy(data, "NO-SESSIONS");
    }
    playerSessionsMutex.unlock();
    strcat(data, "\0");
    writeData(clientSocket, data, sizeof(data));
}


void sendUserData(int clientSocket, char* msg){
	char data[512];
	memset(data, 0, sizeof(data));
    char * pch;
    pch = strtok(msg, "-");
    std::stringstream strValue;
    int sID;
    for (int hcp = 0; hcp < 3; hcp ++){
        if(pch != nullptr ){
            pch = strtok(nullptr, "-");
        }
    }
    if(pch != nullptr ){
        strValue << pch;
        strValue >> sID; //convert to int
    }
    std::vector<Player> players;
    playerSessionsMutex.lock();
    int sessionExists = playerSessions.count(sID);
    if ( sessionExists != 0){
        players = playerSessions[sID];
    }
    playerSessionsMutex.unlock();
    if ( sessionExists ){   
        clientMapMutex.lock();
        std::string clientNick = clientMap[clientSocket].getNick();
        clientMapMutex.unlock();
        sessionHostsMutex.lock();
        std::string host = sessionHosts[sID];
        sessionHostsMutex.unlock();
        sessionStartDataMutex.lock();
        std::string sessionMsg = ""; 
        if (sessionStartData.count(sID) == 1) {
            sessionMsg = sessionStartData[sID];
        }
        sessionStartDataMutex.unlock();
        if (sessionMsg != ""){
            if(sessionMsg == "START-SESSION-OK\0"){
                removeFromEpoll(clientSocket);
                strcpy(data, "START-SESSION-OK\0");
                sendUserDataCounterMutex.lock();
                sendUserDataCounter[sID] += 1;
                sendUserDataCounterMutex.unlock();
            } else if ( (sessionMsg == "START-SESSION-FAIL\0") && (clientNick == host)) {
                strcpy(data, "START-SESSION-FAIL\0");
                sessionStartDataMutex.lock();
                sessionStartData.erase(sID);
                sessionStartDataMutex.unlock();
            }
        } else {
            char num[10];
            sprintf (num, "%d", players.size());
            strcpy(data, num);
            strcat(data,":");
            strcat(data, host.c_str());
            strcat(data, ",");
            for (auto & element : players) {
                if (element.getNick() != host){
                    strcat(data, element.getNick().c_str() );
                    strcat(data, ",");  
                }
            }    
        }    
    } else {
        strcpy(data, "SESSION-QUIT\0");
    }
    writeData(clientSocket, data, sizeof(data));

    int notifySize = 0;
    int notifyCounter = -1;
    sendUserDataCounterMutex.lock();
    if( sendUserDataCounter.count(sID) == 1 ){
        playerSessionsMutex.lock();
        notifySize = playerSessions[sID].size();
        playerSessionsMutex.unlock();
        notifyCounter = sendUserDataCounter[sID];
        if (notifyCounter >= notifySize){ //TODO: to nie jest idealne ex: nie zmapowane a liczone
            sendUserDataCounter.erase(sID);
            cV.notify_one();
        }
    }
    sendUserDataCounterMutex.unlock();
}


void updateCurrentPlayers(int sessionId, std::map<std::string, int> &currentPlayersFd){
    currentPlayersFd.clear();
    clientMapMutex.lock();
    auto mapC = clientMap;
    clientMapMutex.unlock();
    playerSessionsMutex.lock();
    auto mapP = playerSessions[sessionId];
    playerSessionsMutex.unlock();
    for (auto &p : mapP) {
        int keyFd = 0;
        for (auto &playerPair : mapC) {
            if (playerPair.second.getNick() == p.getNick()) {
                keyFd = playerPair.first;
                break;
            }
        }
        currentPlayersFd.insert(std::pair<std::string, int>(p.getNick(), keyFd));
    }
}

void sessionLoop(int sessionID) {

    cV.wait_for(uqLock, std::chrono::seconds(4)) == std::cv_status::timeout; //TODO: usun porownanie

    sendUserDataCounterMutex.lock();
    if (sendUserDataCounter.count(sessionID) == 1) {
        sendUserDataCounter.erase(sessionID);
    }
    sendUserDataCounterMutex.unlock();

    char synchMsg[100];
    playerSessionsFdsMutex.lock();
    auto sessionsFds = playerSessionsFds[sessionID];
    playerSessionsFdsMutex.unlock();
    for (int i = 0; i < sessionsFds.size(); i++) {
        readData(sessionsFds.at(i), synchMsg, sizeof(synchMsg));
        if (strcmp(synchMsg, "PLAYER-READY\0") == 0) {
            std::cout << synchMsg << std::endl;
        }
    }

    sessionStartDataMutex.lock();
    sessionStartData.erase(sessionID);
    sessionStartDataMutex.unlock();

    sessionHostsMutex.lock();
    sessionHosts.erase(sessionID);
    sessionHostsMutex.unlock();

    std::map<std::string, int> playerPoints{};

    std::set<std::string> usedWords{};
    const unsigned int rounds = 5;

    for (int i = 0; i < rounds; i++) {

        char startMsg[100];

        playerSessionsMutex.lock();
        std::vector<Player> players = playerSessions[sessionID];
        playerSessionsMutex.unlock();

        if (players.size() < 2) {

            if (players.size() == 1) {
                playerSessionsFdsMutex.lock();
                int playerFd = playerSessionsFds[sessionID][0];
                playerSessionsFdsMutex.unlock();
                strcpy(startMsg, "SESSION-TIMEOUT\0");
                writeData(playerFd, startMsg, sizeof(startMsg));
            }
            std::this_thread::sleep_for(std::chrono::seconds(10));

            playerSessionsMutex.lock();
            players = playerSessions[sessionID];
            playerSessionsMutex.unlock();

            if (players.size() < 2) {

                if (players.size() == 1) {
                    playerSessionsFdsMutex.lock();
                    int playerFd = playerSessionsFds[sessionID][0];
                    playerSessionsFdsMutex.unlock();
                    strcpy(startMsg, "SESSION-KILL\0");
                    writeData(playerFd, startMsg, sizeof(startMsg));
                }
                sessionNamesMutex.lock();
                sessionNames.erase(sessionID);
                sessionNamesMutex.unlock();
                playerSessionsMutex.lock();
                playerSessions.erase(sessionID);
                playerSessionsMutex.unlock();
                playerSessionsFdsMutex.lock();
                playerSessionsFds.erase(sessionID);
                playerSessionsFdsMutex.lock();
                // koniec sesji
                return;
            }
        }

        std::map<std::string, int> currentPlayersFd{};

        clientMapMutex.lock();
        for (auto &p : players) {
            int keyFd = 0;
            for (auto &playerPair : clientMap) {
                if (playerPair.second.getNick() == p.getNick()) {
                    keyFd = playerPair.first;
                    break;
                }
            }
            currentPlayersFd.insert(std::pair<std::string, int>(p.getNick(), keyFd));
            playerPoints.insert(std::pair<std::string, int>(p.getNick(), 0)); //TODO: obczaj czy ok
        }
        clientMapMutex.unlock();

        //updateCurrentPlayers(sessionID, currentPlayersFd);

        for (auto &pFd: currentPlayersFd) {
            strcpy(startMsg, "ROUND-START\0");
            writeData(pFd.second, startMsg, sizeof(startMsg));
        }

        updateCurrentPlayers(sessionID, currentPlayersFd);

        std::map<std::string, double> winners{};
        bool closing = false;
        std::string randomWord;

        while (true) {
            randomWord = getRandomWord();
            auto it = usedWords.find(randomWord);
            if (it == usedWords.end()) {
                usedWords.insert(randomWord);
                break;
            }
        }

        //Wyślij każdemu graczowi słowo.
        for (auto &p : currentPlayersFd) {
            char buf[200];
            strcpy(buf, randomWord.c_str());
            strcat(buf, "\0");
            writeData(p.second, buf, sizeof(buf));
        }

        updateCurrentPlayers(sessionID, currentPlayersFd);

        auto start = std::chrono::steady_clock::now();     // start timer
        double roundTime = 60.0 + 10.0; //1 minuta na rundę TODO: plus przesył laggi??
        std::map<std::string, bool> lostMap;
        while (true) {
            auto end = std::chrono::steady_clock::now();
            auto time_span = static_cast<std::chrono::duration<double>>(end - start);

            for (auto &p : currentPlayersFd) {
                int keyFd = p.second;
                std::string player = p.first;
                char winner_buf[100];
                int ret = recv(keyFd, winner_buf, sizeof(winner_buf), MSG_DONTWAIT);
                if (ret > 0) {
                    if (strcmp(winner_buf, "PLAYER-LOST\0") == 0) {
                        lostMap.insert(std::pair<std::string, bool>(player, true));
                    } else {
                        if (!closing) {
                            start = std::chrono::steady_clock::now();
                            end = std::chrono::steady_clock::now();
                            time_span = static_cast<std::chrono::duration<double>>(end - start);
                            roundTime = 3.0;
                            closing = true;
                        }
                        double time = strtod(winner_buf, nullptr);
                        winners.insert(std::pair<std::string, double>(player, time));
                    }
                }
            }
            updateCurrentPlayers(sessionID, currentPlayersFd);

            int actualSize = currentPlayersFd.size();
            int checkSize = 0;
            for (auto &checkP: currentPlayersFd) {
                if (lostMap.count(checkP.first) == 1) {
                    checkSize++;
                }
            }
            if ((actualSize == checkSize) || (time_span.count() > roundTime)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        char endMsg[200];
        //char numWin[20];
        std::string winner;
        double minTime = 9999;
        if (winners.empty()) {
            strcpy(endMsg, "WIN-0\0");
        } else if (winners.size() == 1) {
            for (auto &item : winners) {
                winner = item.first;
            }
            strcpy(endMsg, "WIN-");
            strcat(endMsg, winner.c_str());
            strcat(endMsg, "\0");
        } else if (winners.size() > 1) {
            for (auto &item : winners) {
                if (item.second < minTime) {
                    winner = item.first;
                    minTime = item.second;
                }
            }
            //sprintf (numWin, "%d", winner);
            strcpy(endMsg, "WIN-");
            //strcat(endMsg, numWin);
            strcat(endMsg, winner.c_str());
            strcat(endMsg, "\0");
        }
        for (auto &points : playerPoints) {
            if (points.first == winner) {
                points.second += 1;
                break;
            }
        }
        playerSessionsFdsMutex.lock();
        for (auto &send: currentPlayersFd) {
            std::vector<int> vecFd = playerSessionsFds[sessionID];
            for (auto &i: vecFd) {
                if (send.second == i) {
                    writeData(send.second, endMsg, sizeof(endMsg));
                }
            }
        }
        playerSessionsFdsMutex.unlock();
        if (i == rounds - 2) {
            int maxScore = -9999;
            for (auto &points : playerPoints) {
                if (points.second > maxScore) {
                    maxScore = points.second;
                }
            }
            std::vector<std::string> checkForTie;
            for (auto &points : playerPoints) {
                if (points.second == maxScore) {
                    checkForTie.push_back(points.first);
                }
            }
            if (checkForTie.size() <= 1) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    char finishMsg[100];
    playerSessionsFdsMutex.lock();
    auto playerFds = playerSessionsFds[sessionID];
    playerSessionsFdsMutex.unlock();

    for (auto &pFd: playerFds) {
        strcpy(finishMsg, "GAME-OVER\0");
        writeData(pFd, finishMsg, sizeof(finishMsg));
    }

    playerSessionsFdsMutex.lock();
    for (auto &p: playerSessionsFds[sessionID]) {
        addToEpoll(p);
    }
    playerSessionsFdsMutex.unlock();

    sessionNamesMutex.lock();
    sessionNames.erase(sessionID);
    sessionNamesMutex.unlock();
    playerSessionsMutex.lock();
    playerSessions.erase(sessionID);
    playerSessionsMutex.unlock();
    playerSessionsFdsMutex.lock();
    playerSessionsFds.erase(sessionID);
    playerSessionsFdsMutex.unlock();
 }


void addToEpoll(int fd){
    struct epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = fd;
    if( epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) < 0 ){
        fprintf(stderr, "Failed to add file descriptor to epoll.\n");
        close(epollFd);
        exit(EPOLL_ADD);
    }
}

void removeFromEpoll(int fd){
    if ( epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr) < 0 ){
        perror("Server failed to delete clientfd from epoll.\t");
        printf("%s\n", strerror(errno));
        //close(epollFd);
        //exit(EPOLL_REMOVE);
    }
}


void stopConnection(int ClientFd){

    handlePlayerExit(ClientFd);
    
    clientMapMutex.lock();
    if (clientMap.count(ClientFd) == 1){
        clientMap.erase(ClientFd);
    }
    clientMapMutex.unlock();

    clientSocketsMutex.lock();
    clientSockets.erase(std::remove(clientSockets.begin(), clientSockets.end(), ClientFd), clientSockets.end());
    clientSocketsMutex.unlock();


    if (shutdown(ClientFd, SHUT_RDWR) < 0 ){
        perror("Failed to disconnect with the client.");
        printf("%s\n", strerror(errno));
        //FIXME: exit?
    }
    close(ClientFd);
}


void sigHandler(int signal){
    printf(" = CTRL + C\n");

    shutdown(serverFd, SHUT_RDWR);
    close(serverFd);
    SERVER_SHUT_DOWN = true;

    if (threadVector[0].joinable()) { //SPRAWDZ JOINOWANIE
        threadVector[0].join();
    }
}


ssize_t readData(int fd, char * buffer, ssize_t buffsize){
    auto ret = read(fd, buffer, buffsize);
    //std::cout << "Read ret: " << ret << std::endl;
    if(ret == 0){
        //CLOSE CONNECTION WITH CLIENT
        clientMapMutex.lock();
        clientMapMutex.unlock();
        stopConnection(fd);
    }

    if(ret==-1) {
        perror("read failed on descriptor %d\n");
    }
    return ret;
}

void writeData(int fd, char * buffer, ssize_t count){
    auto ret = write(fd, buffer, count);
    //std::cout << "Write ret: " << ret << std::endl;
    if (ret==-1) {
        perror("write failed on descriptor %d\n");
        clientMapMutex.lock();
        clientMapMutex.unlock();
        stopConnection(fd);
    }
    if (ret!=count) perror("wrote less than requested to descriptor %d (%ld/%ld)\n");
}


void handlePlayerExit(int clientFd){

	clientMapMutex.lock(); 
	std::string playerNick = clientMap[clientFd].getNick();
	clientMapMutex.unlock();

	playerSessionsFdsMutex.lock();

    int session = 0;
    bool findEnd= false;
    for (auto & fds: playerSessionsFds){
  		session = fds.first;
    	for (auto &f: fds.second){
    		if(f == clientFd){
    			findEnd=true;
    			break;
    		}
    	}
    	if (findEnd){
    		break;
    	}
    }
    playerSessionsFdsMutex.unlock();

	sessionHostsMutex.lock();    
    std::string hostNick = "";
    if (sessionHosts.count(session) == 1) {
        hostNick = sessionHosts[session]; 
    }
    sessionHostsMutex.unlock();
    bool isHost = (hostNick == playerNick);

    if ( isHost ){ // jesli jest hostem sesji to usuń sesje
    	playerSessionsMutex.lock();
    	playerSessions.erase(session);
    	playerSessionsMutex.unlock();
    	playerSessionsFdsMutex.lock();
    	playerSessionsFds.erase(session);
    	playerSessionsFdsMutex.unlock();
    	sessionNamesMutex.lock();
	    sessionNames.erase(session);
	    sessionNamesMutex.unlock();
		sessionHostsMutex.lock();
    	sessionHosts.erase(session);
    	sessionHostsMutex.unlock();
    } else {             

        playerSessionsMutex.lock();
        
        if (playerSessions.count(session) == 1) {

            auto it = playerSessions[session].begin();
            while (it != playerSessions[session].end()) {
                if (it->getNick() == playerNick) {
                    it = playerSessions[session].erase(it);
                } else {
                    ++it;
                }
            }
        }

        playerSessionsMutex.unlock();

        playerSessionsFdsMutex.lock();
        if (playerSessionsFds.count(session) == 1) {

            auto it = playerSessionsFds[session].begin();
            while (it != playerSessionsFds[session].end()) {
                if (*it == clientFd) {
                    it = playerSessionsFds[session].erase(it);
                } else {
                    ++it;
                }
            }

        }
        playerSessionsFdsMutex.unlock();
        
    }
}

/*
void joinAllThreads(){
    for (std::thread & th : sessionThreads)
    {
        // If thread Object is Joinable then Join that thread.
        if (th.joinable()){
            th.join();
        }
    }
}
*/

