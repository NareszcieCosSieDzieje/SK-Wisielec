
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

#include "../statuses.hpp"
#include "../player.hpp"
#include "../player.cpp"
#include "../data_loader.hpp"



// std::terminate ????

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

std::map<int, std::string> sessionHosts; // USUWANIE HOSTA !=========================================================
std::mutex sessionHostsMutex;

std::map<int, std::string> sessionNames;
std::mutex sessionNamesMutex;

std::mutex writeToFileMutex;


int epollFd{};
int serverFd{};
const unsigned int localPort{55555};
sockaddr_in bindAddr {
        .sin_family = AF_INET,
        .sin_port = htons(localPort),
        // randomowy port?
        //.sin_addr.s_addr = inet_addr("127.0.0.1");
        //.sin_addr = htonl(INADDR_ANY)
};

int maxSessions = 2; //TODO: ile sesji?
int playersPerSession = 4;
const int maxEvents = maxSessions * playersPerSession;

std::atomic<bool> SERVER_SHUT_DOWN(false);


//========================================FUNC PROTO========================================\\


ssize_t readData(int fd, char * buffer, ssize_t buffsize);
void writeData(int fd, char * buffer, ssize_t count);
void sigHandler(int signal); //TODO: DOPRACUJ TO !!!
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
std::string loadUserData(char* filePath); 


/* TODO: ZADANIA SERWERA:
 * nasłuchuj połączeń;
 * pokazuj sesje dostępne i ile graczy;
 * obsługuj tworzenie sesji, zamykanie, dołączanie,
 * obsługuj przebieg rozgrywki sesji;
*/


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

           
           // std::cout << << std::endl;
			
			/*
            std::cout << "Client map: "<< std::endl;
            for(auto &a: clientMap){
				std::cout << "ID gracza = " << a.first << std::endl; 
				std::cout << "Nick gracza = " << a.second.getNick() << std::endl;
			}

			std::cout << "\nPlayer sessions: "<< std::endl;
            for(auto &a: playerSessions){
				std::cout << "ID sesji = " << a.first << std::endl; 
				for(auto &b : a.second){
					std::cout << "Nick gracza = " << b.getNick() << std::endl;	
				}
			}

			std::cout << "\nPlayer sessions FDS: "<< std::endl;
            for(auto &a: playerSessionsFds){
				std::cout << "ID sesji = " << a.first << std::endl; 
				for(auto &b : a.second){
					std::cout << "deskryptor gracza = " << b << std::endl;	
				}
			}

			std::cout << "\nHOSTOWIE SESJI: "<< std::endl;
            for(auto &a: sessionHosts){
				std::cout << "ID sesji = " << a.first << std::endl; 
				std::cout << "Host nick = " << a.second << std::endl;
			}
			*/

			std::cout << "\nNAZWY SESJI: "<< std::endl;
            for(auto &a: sessionNames){
				std::cout << "ID sesji = " << a.first << std::endl; 
				std::cout << "Nazwa sesji = " << a.second << std::endl;
			}
			
		
            if(ret == 0){
                //polling = false;
                handlePlayerExit(clientFd);
                removeFromEpoll(clientFd);
                stopConnection(clientFd);
                continue;
            }
            if ( strcmp(msg, "CLIENT-VALIDATION\0") == 0){
                removeFromEpoll(clientFd);
                std::thread validationThread(clientValidation, clientFd); //Nowe połączenie przeslij do zweryfikowania
                validationThread.detach();
            } else if (strcmp(msg, "SEND-SESSION-DATA\0") == 0) {
                sendSessionData(clientFd);
                    //TODO: czy inny wątek na to
            } else if ( strncmp(msg, "SEND-USER-DATA", 14) == 0){
				sendUserData(clientFd, msg);
            }
            else if( strcmp(msg, "JOIN-SESSION\0") == 0){
                removeFromEpoll(clientFd);
                std::thread jS(joinSession, clientFd);
                jS.detach(); //TODO: obczaj czy ok
            } else if (strcmp(msg, "START-SESSION\0") == 0){

                int session = 0;
                char msg[100];

                clientMapMutex.lock();
                std::string nick = clientMap[clientFd].getNick();
 				clientMapMutex.unlock();

                bool found = false;
                //TODO BLOKADA NA DODAWANIA PRZY STARCIE SESJI!!!!

				
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
                    strcpy(msg, "START-SESSION-OK\0");
                    playerSessionsFdsMutex.lock();
                    for(auto &client: playerSessionsFds[session]){
                        removeFromEpoll(client);
                        writeData(client, msg, sizeof(msg)); //spromptować każdego do uruchomienia sesji
                    }
                    playerSessionsFdsMutex.unlock();
                    // TODO: przywroc w sesji GRACZY do EPOLLA!--------------------------------------
                    std::thread sT(sessionLoop, session);
                    sT.detach(); //FIXME:??
                } else {
                    strcpy(msg, "START-SESSION-FAIL\0");
                    writeData(clientFd, msg, sizeof(msg));
                }
            } else if (strcmp(msg, "DISSOCIATE-SESSION\0") == 0) {
            	//Wyjdz przed startem sesji
            	handlePlayerExit(clientFd);
            }
            else if( strcmp(msg, "DISCONNECTING\0") == 0){
                
                removeFromEpoll(clientFd);
                handlePlayerExit(clientFd); //FIXME: nowe obczaj czy ok!
                stopConnection(clientFd);
            }
            else if ( strcmp(msg,"LOG-OUT\0") == 0){
            	handlePlayerExit(clientFd);
                clientMapMutex.lock();
                clientMap.erase(clientFd);
                clientMapMutex.unlock();
            }
            /*
            if( clientEvent.events & EPOLLIN && clientEvent.data.fd == sock ){
                if read == 0 then zamknij klienta
            }

            */

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
        std::cout << "LISTEN SOCKETY = " << sockets << std::endl;
        clientSocketsMutex.unlock();
        char msg[20];
        if (sockets == maxEvents) {
        	strcpy(msg, "SERVER-MAX\0");
        	writeData(newClient, msg, sizeof(msg));
        	stopConnection(newClient);
        	//TODO: ZAMKNIJ GRACZA--------------------------------------------------------------------------------------
        } else {
            clientSocketsMutex.lock();
            clientSockets.push_back(newClient);
            clientSocketsMutex.unlock();
        	strcpy(msg, "SERVER-OK\0");
        	writeData(newClient, msg, sizeof(msg));
        	addToEpoll(newClient);
        } 
    }
    //TODO: jeśli jakis condition_variable to zakoncz prace?
}


void clientValidation(int newClientFd){

    std::cout << "WERYFIKACJA KLIENTA - fd: " << newClientFd << std::endl;
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
        //printf("%s\n", login);
        pch = strtok(nullptr, "-");
    }
    if(pch != nullptr ){
        pass = pch;
        //printf("%s\n", pass);
    }

    bool userExists = false;
    std::string loginS(login);
    std::string passwordS(pass);

    //std::cout << "char login: " << login << "\tstr login: " << loginS << std::endl;
    //std::cout << "char pass: " << pass << "\tstr pass: " << passwordS << std::endl;

    if(cT == signup){
        if (searchForUserData(loginS, passwordS, true)) {
        	writeToFileMutex.lock();
            addUser(loginS, passwordS);
            writeToFileMutex.unlock();
            userExists = true;
        } //else na koncu obejmuje jak nie przejdzie czyli AUTH-FAIL
    } else if (cT == signin){
    	bool loggedIn = false;
    	for(auto &c: clientMap){ // jesli juz zalogowany na dany nick
    		std::cout<<"clientMap.first = " << c.first << "\tclientMap.second (nick) = " << c.second.getNick() << std::endl;
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
        writeData(newClientFd, authMsg, sizeof(authMsg));        //stopConnection(newClientFd);
        addToEpoll(newClientFd);
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
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

            std::cout << "NAZWA SESJI WKLADANA WLASNIE = " << std::string(buf) << std::endl;
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
    	std::cout << " WSZEDL W DOLACZENIE DO SESJI " << std::endl;
        playerSessionsMutex.lock();
        if (playerSessions.count(sessionMode) != 1){ //Jak nie ma klucza
            strcpy(msg, "SESSION-KILLED\0"); 
        	std::cout << " DOLACZANIE SESSION KILL  " << std::endl;
        }
        else {
        	if (playerSessions[sessionMode].size() < playersPerSession){
	            playerSessions[sessionMode].push_back(player);
	        
	            sprintf (sessionId, "%d", sessionMode);
	            std::cout << " DOLACZANIE SESSION GOOD " << std::endl;
	            strcpy(msg, "SESSION-GOOD\0");
	            //strcat(msg, "\0");
	            secondMsg = true;
            } else {
                strcpy(msg, "SESSION-BUSY\0");
                std::cout << " DOLACZANIE SESSION BUSY " << std::endl;
            }
        }
        playerSessionsMutex.unlock();
        writeData(clientFd, msg, sizeof(msg));
        std::cout << " DOLACZANIE WYSLAL DANE " << std::endl;
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
    } else {
    	strcpy(data, "NO-SESSIONS");
    }
    for( auto const& [key, val] : playerSessions) {
        int sessionID = key;
        char num[10];
        sprintf (num, "%d", key);
        strcat(data, num);
        strcat(data, "-");
        strcat(data, sessionNames[sessionID].c_str()); // nazwa sesji
        strcat(data, "-");
        strcat(data, sessionHosts[sessionID].c_str()); //NICK HOSTA
        //std::cout << "sessionHosts[sessionID]: " << sessionHosts[sessionID].c_str() << std::endl;
        strcat(data, ";");
    }
    playerSessionsMutex.unlock();
    strcat(data, "\0");
    std::cout << "===========================================> DANE w  sendSessionData: " << data << std::endl;
    writeData(clientSocket, data, sizeof(data));
}


void sendUserData(int clientSocket, char* msg){ //wyslij hsota
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
    playerSessionsMutex.lock(); //=================================================================================MUTEX-NEW!
  	std::vector<Player> players = playerSessions[sID];
  	playerSessionsMutex.unlock(); //=================================================================================MUTEX-NEW!

    if (players.size() > 0 ){   
	    char num[10];
	    sprintf (num, "%d", players.size());
	    strcpy(data, num);
	    strcat(data,":");
	    sessionHostsMutex.lock(); //=================================================================================MUTEX-NEW!
	    std::string host = sessionHosts[sID]; 
	    sessionHostsMutex.unlock(); //=================================================================================MUTEX-NEW!
	    strcat(data, host.c_str());
	    strcat(data, ",");
	    for (auto & element : players) {
	    	if (element.getNick() != host){
	    		strcat(data, element.getNick().c_str() );
	        	strcat(data, ",");	
	    	}
	    }
	} else {
		strcpy(data, "SESSION-QUIT\0"); //TODO: jezeli host wyjdzie to niech usunie ludzi i rozwiaze sesje
	}
	 // std::cout << "=======================================> DANE W senduserdata: " << sID<< std::endl;
	writeData(clientSocket, data, sizeof(data));
}

 


void sessionLoop(int sessionID) { //TODO: OBSŁUŻ wyjście z sesji!!

    //TODO jak sie odlaczy w trakcie to tylko nie wysylaj do niego danych, czyli wywal z sesji i sprawdz na koncu co sie pokrywa

    std::map<std::string, int> playerPoints{}; //TODO: wysyłaj tylko do ych co są w rundzie

    std::set<std::string> usedWords{};
    const unsigned int rounds = 5;

    sessionHostsMutex.lock();
    sessionHosts.erase(sessionID); // USUWANIE HOSTA PO STARCIE RUNDY!
	sessionHostsMutex.unlock();

    for (int i = 0; i < rounds; i++) {

        char startMsg[50];

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

                if (players.size() == 1) { //TODO czy temu jednemu jeszcze wynik wysłać??
                    playerSessionsFdsMutex.lock();
                    int playerFd = playerSessionsFds[sessionID][0];
                    playerSessionsFdsMutex.unlock();
                    strcpy(startMsg, "SESSION-KILL\0");
                    writeData(playerFd, startMsg, sizeof(startMsg));
                }
                //sessionHosts.erase(sessionID);
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

        // na początku rundy komunikat runda ok

        playerSessionsFdsMutex.lock();
        for(auto &pFd: currentPlayersFd){
            strcpy(startMsg, "ROUND-START\0");
            writeData(pFd.second, startMsg, sizeof(startMsg));
        }
        playerSessionsFdsMutex.unlock();

        std::map<std::string, double> winners{};
        bool closing = false;
        std::string randomWord;

        while (true) {
            randomWord = getRandomWord(); // MUTEX-NEW????????????????????????????????????????
            auto it = usedWords.find(randomWord);
            if (it == usedWords.end()) {
                usedWords.insert(randomWord);
                break;
            }
        }

        //Wyślij każdemu graczowi słowo.
        playerSessionsFdsMutex.lock();
        for (auto &p : currentPlayersFd) {
            char buf[100];
            strcpy(buf, randomWord.c_str());
            strcat(buf, "\0");
            writeData(p.second, buf, sizeof(buf));
        }
        playerSessionsFdsMutex.unlock();

        auto start = std::chrono::steady_clock::now();     // start timer
        double roundTime = 120.0 + 10.0; //2 minuty na rundę TODO: plus przesył laggi??
        while (true) {
            auto end = std::chrono::steady_clock::now();
            auto time_span = static_cast<std::chrono::duration<double>>(end - start);

            //TODO: nie blokujaca sprawdz po kazdym czy jest jakis wynik;
            playerSessionsFdsMutex.lock();
            for (auto &p : currentPlayersFd) {
                int keyFd = p.second;
                std::string player = p.first;
                char winner_buf[50];
                int ret = recv(keyFd, winner_buf, sizeof(winner_buf), MSG_DONTWAIT);
                if (ret > 0) {
                    if (!closing) {
                        roundTime += 3.0;
                        closing = true;
                    }
                    double time = strtod(winner_buf, nullptr);
                    winners.insert(std::pair<std::string, double>(player, time));
                } else {
                	//TODO: WYRZUC GRACZA
                } 
            }
            playerSessionsFdsMutex.unlock();
            if (time_span.count() > roundTime) {
                break;
            }
        }
        char endMsg[50];
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
        //TODO: teraz wyslij wiadomosc, srpwdza czy jeszcze sa gracze co byli
        playerSessionsFdsMutex.lock();
        for (auto &send: currentPlayersFd) {
            std::vector<int> vecFd = playerSessionsFds[sessionID];
            for(auto& i: vecFd){
                if(send.second == i){
                    writeData(send.second, endMsg, sizeof(endMsg));
                }
            }
        }
        playerSessionsFdsMutex.unlock();

        if(i == rounds - 2){
            int maxScore = -9999;
            for (auto &points : playerPoints) {
                if(points.second > maxScore){
                    maxScore = points.second;
                }
            }
            std::vector<std::string> checkForTie;
            for (auto &points : playerPoints) {
                if(points.second == maxScore){
                    checkForTie.push_back(points.first);
                }
            }
            if(checkForTie.size() > 1){
                //TODO: DOGRYWKA
            }else {
                break;
            }
        }
    }

    //std::map<Player, int> playerPoints{}; //TODO: wysyłaj tylko do ych co są w rundzie

    char scoreBoard[1024];
    for(auto &points: playerPoints){
        strcpy(scoreBoard, points.first.c_str());
        strcat(scoreBoard, ":");
        char num[10];
        sprintf (num, "%d", points.second);
        strcat(scoreBoard, num);
        strcat(scoreBoard, "\n");
    }

    char check[10];
    playerSessionsFdsMutex.lock();
    for (auto &p: playerSessionsFds[sessionID]) {
        //TODO problematyczne bo usuwanie nie dzialajacych w loopie auto??
        /*
        int ret = recv(p, check, sizeof(check), MSG_DONTWAIT);
        if (ret == 0){
            continue;
        }
        */
        Player playa = clientMap[p];
        for(auto pointz: playerPoints){
            if (pointz.first == playa.getNick() ) {
                writeData(p, scoreBoard, sizeof(scoreBoard));
            }
        }
    }
    for (auto &p: playerSessionsFds[sessionID]) {
        addToEpoll(p);
    }
    //Dodaj do epolla
 	std::cout << "PRZED USUNIECIEM ROZMIAR MAPY NAZW = " << sessionNames.size() << std::endl;
 	sessionNamesMutex.lock();
    sessionNames.erase(sessionID);
    sessionNamesMutex.unlock();
    std::cout << "PO USUNIECIU ROZMIAR MAPY NAZW = " << sessionNames.size() << std::endl;

    playerSessionsMutex.lock();
    playerSessions.erase(sessionID);
    playerSessionsMutex.unlock();
    playerSessionsFdsMutex.lock();
    playerSessionsFds.erase(sessionID);
    playerSessionsFdsMutex.unlock();
    
    //Wyrzuć z sesji


    // TODO COS TAKIEGO ZE READUJE NON BLOCKIEM PO KAZDYM CURRENT PLAYER ZOBACYZC CZY ZWROCI 0  JAK TAK TO WYWAL Z GRY

 }


 /*
        Po rozpoczeciu gry host traktowany jest jak zwykly gracz. Tzn: jeśli opuści sesję, a przynajmniej dwóch innych graczy gra to 		   sesja dalej trwa.
        6. Serwer obsługuje przerwania połączeń, lub wyjście z gry. Przechowuje dane gracza na czas trwania sesji, w tym jego login. 			   Pozwalaja dołączyć do następnej rundy, w przypadku zerwania połączenia przez klienta, identyfikując go przez login.
        7. W trakcie rundy, serwer losuje słowo i wysyła je graczom, którzy muszą w określonym czasie je odgadnąć.
           Ten kto poprawnie odgadnie jako pierwszy wygrywa. Sesja trwa n rund. Gracze mogą zdobyć 1 punkt za zwycięstwo,
           lub 0 punktów za przegraną.
        8. Każdy klient osobno mierzy czas rundy i wysła go do serwera, w momencie zgadnięcia hasła, żeby rozwiązać
           problem wyróżnienia zwycięzcy (różne szybkości połączeń). Serwer po otrzymaniu komunikatu o wygranej jednego z graczy czeka 2-3 sekundy na ewentualny komunikat o wygranej innego gracza - wtedy porównuje ich czasy i wybiera zwycięzce. W przypadku gdy jeden z dwóch użytkowników wygra jako pierwszy, ale jego komunikat o wygranej dojdzie do serwera dużo później (3 sek+) niż komunikat o wygranej drugiego gracza, to drugi gracz jest wygranym, mimo iż w rzeczywistości odgadnął jako drugi.
        9. Podczas rundy gracz widzi punkty przeciwnika, czas do końca rundy,
            numer rozgrywanej rundy/maksymalną liczbę rund, wskaźnik postępu przeciwnika.
        10. W momencie osiągnięcia limitu błedów, gracz jest zawieszony do końca rundy.
        11. Co każdą rundę ilość możliwości do popłenienia błedów resetuje się.
        12. W przypadku remisu, dogrywka w formie kolejnej rundy.
        13. Sesja kończy się podsumowaniem punktów graczy (scoreboard). Następnie wszyscy biorący udział w grze użytkownicy przenoszeni są do panelu wyboru/stworzenia sesji.
*/


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
        perror("Server failed to delete clientfd from epoll.\n");
        //close(epollFd);
        //exit(EPOLL_REMOVE);
    }
}


//TODO: jakiś send że zrywamy połączenie?? to raczej w instacji danego problemu dac
void stopConnection(int ClientFd){

	/*
    Player p = clientMap[ClientFd];
    std::string nick = p.getNick();

    playerSessionsMutex.lock();
    for(auto &fds: playerSessionsFds){
        std::vector<int> vec = fds.second;
        
        vec.erase( std::remove_if( vec.begin(), vec.end(), [ClientFd](int i){ return i == ClientFd;}), vec.end());
    }
    for(auto &pl: playerSessions){
        std::vector<Player> vec = pl.second;
        
        vec.erase( std::remove_if( vec.begin(), vec.end(), [nick](Player py){ return py.getNick() == nick;}), vec.end());
    }
    playerSessionsMutex.unlock();
    */
    clientSocketsMutex.lock();
    clientSockets.erase(std::remove(clientSockets.begin(), clientSockets.end(), ClientFd), clientSockets.end());
    clientSocketsMutex.unlock();

    clientMapMutex.lock();
    clientMap.erase(ClientFd);
    clientMapMutex.unlock();

    if (shutdown(ClientFd, SHUT_RDWR) < 0 ){
        perror("Failed to disconnect with the client.\n");
        //FIXME: exit?
    }
    close(ClientFd);
}



//TODO: HANDLE ZAMKNIECIE
void sigHandler(int signal){
    printf("CTRL + C\n");

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
    if(ret==-1) perror("write failed on descriptor %d\n");
    if(ret!=count) perror("wrote less than requested to descriptor %d (%ld/%ld)\n");
}


void handlePlayerExit(int clientFd){

	clientMapMutex.lock(); //==================MUTEX-NEW!

	std::cout << "Handle (rozmiar mapy)przed = " << clientMap.size() << std::endl; 

	std::string playerNick = clientMap[clientFd].getNick();

	clientMapMutex.unlock();

	std::cout << "Handle (player nick)1 = " << playerNick << std::endl; //FIXME: NICK PUSTY!!!!!!!!!!!!!!!!!!!!!

	
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

    std::cout << "Handle (Session id) = " << session << std::endl;


	sessionHostsMutex.lock();
    int isHost = sessionHosts.count(session);
    sessionHostsMutex.unlock();
    
    if ( isHost != 0){ // jesli jest hostem sesji to usuń sesje
    	std::cout << "Handle (host usuwa sesje)!" << std::endl;
  	    playerSessionsMutex.lock();
    	playerSessions.erase(session);
    	playerSessionsMutex.unlock();
    	playerSessionsFdsMutex.lock();
    	playerSessionsFds.erase(session);
    	playerSessionsFdsMutex.unlock();
    	std::cout << "PRZED USUNIECIEM ROZMIAR MAPY NAZW = " << sessionNames.size() << std::endl;
    	sessionNamesMutex.lock();
	    sessionNames.erase(session);
	    sessionNamesMutex.unlock();
	    std::cout << "PO USUNIECIU ROZMIAR MAPY NAZW = " << sessionNames.size() << std::endl;
		sessionHostsMutex.lock();
    	sessionHosts.erase(session);
    	sessionHostsMutex.unlock();
    } else {                                //jesli to nie to wyrzuc z sesji
        std::cout << "Handle (host nie usuwa sesji!)" << std::endl; 
        playerSessionsMutex.lock();
        for(auto &s: playerSessions) {
            auto it = s.second.begin();
            while (it != s.second.end()) {
                if (it->getNick() == playerNick) {
                    it = s.second.erase(it);
                } else {
                    ++it;
                }
            }
        }
        playerSessionsMutex.unlock();
        playerSessionsFdsMutex.lock();
        for(auto &fds: playerSessionsFds){
            std::vector<int> vec = fds.second;
            vec.erase( std::remove_if( vec.begin(), vec.end(), [clientFd](int i){ return i == clientFd;}), vec.end());
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

