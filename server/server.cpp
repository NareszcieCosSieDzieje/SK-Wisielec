
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

#include "../statuses.hpp"
#include "../player.hpp"
#include "../player.cpp"
#include "../data_loader.hpp"


//=====================================GLOBALS============================================\\

std::vector<std::thread> threadVector;
std::vector<int> clientSockets; //FIXME:!

std::map<int, Player> clientMap;
std::mutex clientMapMutex;

std::map<int, std::vector<Player>> playerSessions;
std::map<int, std::vector<int>> playerSessionsFds;
std::mutex playerSessionsMutex;

int epollFd{};
int serverFd{};
const unsigned int localPort{55555};
sockaddr_in bindAddr {
        .sin_family = AF_INET,
        .sin_port = htons(localPort),
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
void sessionLoop(int sessionID);
void joinSession(int clientFd);
void addToEpoll(int fd);
void removeFromEpoll(int fd);
std::string loadUserData(char* filePath); //TODO: zwróć array stringów


/* TODO: ZADANIA SERWERA:
 * nasłuchuj połączeń;
 * pokazuj sesje dostępne i ile graczy;
 * obsługuj tworzenie sesji, zamykanie, dołączanie,
 * obsługuj przebieg rozgrywki sesji;
*/


//=======================================MAIN=============================================\\

int main(int argc, char* argv[]){


    signal(SIGINT, sigHandler);
    signal(SIGTSTP, sigHandler);

    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY); //TODO: ZMIEN NA ADRESOKRESLONY

    startServer();

    struct epoll_event events[maxEvents];

    bool polling = true;

    while(polling && !SERVER_SHUT_DOWN) {


        int event_count = epoll_wait(epollFd, events, maxEvents, -1);
        printf("Ready events: %d\n", event_count);
        for (int i = 0; i < event_count; i++) {
            
        	if (event_count < 0){
        		perror("Epoll events < 0\n");
        		break;
        	}

            struct epoll_event clientEvent = events[i];
            int clientFd = clientEvent.data.fd;
            printf("Czytanie z klienta o deskryptorze: '%d' -- \n", clientFd);

            char msg[512];
            int ret = readData(clientFd, msg, sizeof(msg));//TODO: jakies stale ilosci danych zeby dzialalo jakies minim z maxow

            if(ret == 0){
                polling = false;
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
            }
            else if( strcmp(msg, "JOIN-SESSION\0") == 0){
                removeFromEpoll(clientFd);
                std::thread jS(joinSession, clientFd);
                jS.detach(); //TODO: obczaj czy ok
            } else if (strcmp(msg, "START-SESSION\0") == 0){

                int session = 0;
                char msg[100];

                std::string nick = clientMap[clientFd].getNick();

                bool found = false;
                //TODO BLOKADA NA DODAWANIA PRZY STARCIE SESJI!!!!
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
                if (playerSessions[session].size() >= 2){
                    strcpy(msg, "START-SESSION-OK\0");
                    for(auto &client: playerSessionsFds[session]){
                        removeFromEpoll(client);
                        write(client, msg, sizeof(msg)); //spromptować każdego do uruchomienia sesji
                    }
                    // TODO: przywroc w sesji GRACZY do EPOLLA!--------------------------------------
                    std::thread sT(sessionLoop, session);
                    sT.detach(); //FIXME:??
                } else {
                    strcpy(msg, "START-SESSION-FAIL\0");
                    write(clientFd, msg, sizeof(msg));
                }
            } else if (strcmp(msg, "DISSOCIATE-SESSION\0") == 0) {

                //TODO: TUTUAJ JAKIES OGARNIANIE KIEDY MOZNA GO WYRZUCIC---------------------------------------------------------------

                Player p = clientMap[clientFd];

                playerSessionsMutex.lock();
                for(auto &s: playerSessions) {
                    auto it = s.second.begin();
                    while (it != s.second.end()) {
                        if (it->getNick() == p.getNick()) {
                            it = s.second.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
                for(auto &fds: playerSessionsFds){
                    std::vector<int> vec = fds.second;
                   //
                   vec.erase( std::remove_if( vec.begin(), vec.end(), [clientFd](int i){ return i == clientFd;}), vec.end());
                }
                playerSessionsMutex.unlock();
            }
            else if( strcmp(msg, "DISCONNECTING\0") == 0){
                removeFromEpoll(clientFd);
                stopConnection(clientFd);
            }
            else if ( strcmp(msg,"LOG-OUT\0") == 0){
                removeFromEpoll(clientFd);

                Player p = clientMap[clientFd];
                std::string nick = p.getNick();

                playerSessionsMutex.lock();
                for(auto &fds: playerSessionsFds){
                    std::vector<int> vec = fds.second;
                    vec.erase( std::remove_if( vec.begin(), vec.end(), [clientFd](int i){ return i == clientFd;}), vec.end());
                }
                for(auto &pl: playerSessions){
                    std::vector<Player> vec = pl.second;
                    vec.erase( std::remove_if( vec.begin(), vec.end(), [nick](Player py){ return py.getNick() == nick;}), vec.end());
                }
                playerSessionsMutex.unlock();

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


    /*while (!threadVector[0].joinable()){

    }
    threadVector[0].join();
	*/

    return 0;
}


//=======================================FUNC-DEC=========================================\\

void startServer(void){
    if( (serverFd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
        perror("Server failed to create socket.\n");
        exit(SOCKET_CREATE);
    }
    int enable = 1;
    if( setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0 ){
        perror("Setsockopt(SO_REUSEADDR) in server failed.\n");
        exit(SOCKET_REEUSE);
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
        if(clientMap.size() == maxSessions*playersPerSession){ //FIXME: CZY TO JEST OKEJ?
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        int newClient = accept(serverFd, (struct sockaddr *)&clientAddr, &cliLen); //Nawiąż nowe połączenie z klientem.
        if (newClient < 0) {
           if (errno == EINVAL && SERVER_SHUT_DOWN) {
               return;
           } else {
               perror("Server ERROR on accept.\n");
               exit(SOCKET_ACCEPT);
           }
        }
        addToEpoll(newClient);
        //std::thread validationThread(clientValidation, newClient); //Nowe połączenie przeslij do zweryfikowania
        //validationThread.detach();
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
    std::cout << msg << std::endl;
    char * pch;
    pch = strtok(msg, "-");
    char* login;
    char* pass;
    if(pch != nullptr ){
        login = pch;
        printf("%s\n", login);
        pch = strtok(nullptr, "-");
    }
    if(pch != nullptr ){
        pass = pch;
        printf("%s\n", pass);
    }
    bool userExists = false;
    std::string loginS(login);
    std::string passwordS(pass);

    if(cT == signup){
        if (!searchForUserData(loginS, passwordS, true)) {
            addUser(loginS, passwordS);
            userExists = true;
        } //else na koncu obejmuje jak nie przejdzie czyli AUTH-FAIL
    } else if (cT == signin){
        userExists = searchForUserData(loginS, passwordS, false); //WYWOŁANIE FUNKCJI CZYTAJĄCEJ Z PLIKU
    }
    char authMsg[100];
    if (userExists) {
        Player newPlayer(login, pass);
        //Dodaj do mapy klientow -graczy
        clientMapMutex.lock();
        clientMap.insert(std::pair<int, Player>(newClientFd, newPlayer));
        clientMapMutex.unlock();

        addToEpoll(newClientFd);

        strcpy(authMsg, "AUTH-OK\0");  //Wyslij ack ze sie zalogował
        writeData(newClientFd, authMsg, sizeof(authMsg));
    } else {
        strcpy(authMsg, "AUTH-FAIL\0");
        writeData(newClientFd, authMsg, sizeof(authMsg));
        std::cout << "Client authorization didnt succeed: " << newClientFd << std::endl;
        //stopConnection(newClientFd);
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}


void joinSession(int clientFd){
    Player player = clientMap[clientFd];
    int sessionMode = -1;
    int finalSessionId = -1;

    char buf[100];
    char sessionId[20];

    int ret = readData(clientFd, sessionId, sizeof(sessionId)); // int read = recv(clientFd, sessionId, sizeof(sessionId), MSG_DONTWAIT); //response non block get chosen session
    if(ret != 20){
        perror("Join session read error.\n");
        return;
    }
    sessionMode = (int) strtol(sessionId, NULL, 10);

    if(sessionMode < 0){
        perror("Server join session error - sessionMode < 0\n");
    }
    else if (sessionMode == 0){ //TODO: Czy jak usuniete niskie wartosci to uzywanie od nowa? szczegol tho
        std::vector<Player> playerVector;
        std::vector<int> playerFds;
        playerVector.push_back(player);
        playerFds.push_back(clientFd);
        if (playerSessions.empty()){
            finalSessionId = 1;
            playerSessionsMutex.lock();
            playerSessions.insert(std::pair<int, std::vector<Player>>(finalSessionId, playerVector));
            playerSessionsFds.insert(std::pair<int, std::vector<int>>(finalSessionId, playerFds));
            playerSessionsMutex.unlock();
            strcpy(buf, "SESSION-1\0");
        } else if (playerSessions.size() < maxSessions )   {
            finalSessionId = (int)playerSessions.size() +1;
            playerSessionsMutex.lock();
            playerSessions.insert(std::pair<int, std::vector<Player>>(finalSessionId, playerVector));
            playerSessionsFds.insert(std::pair<int, std::vector<int>>(finalSessionId, playerFds));
            playerSessionsMutex.unlock();
            char num[10];
            sprintf (num, "%d", finalSessionId);
            strcpy(buf, "SESSION-");
            strcat(buf, num);
            strcat(buf, "\0"); //czy potrzebne?
        } else {
            //error nie mozna zrobic sesji;
            strcpy(buf, "SESSION-MAX\0");
        }
        write(clientFd, buf, sizeof(buf));
    } else {
        playerSessionsMutex.lock();
        if (playerSessions.count(sessionMode) != 1){ //Jak nie ma klucza
            strcpy(buf, "SESSION-KILLED\0"); //TODO NWOE DLA KAMILA==================================================================================================================================
        }
        else {
            if (playerSessions[sessionMode].size() < playersPerSession){
            playerSessions[sessionMode].push_back(player);
            char num[10];
            sprintf (num, "%d", sessionMode);
            strcpy(buf, "SESSION-");
            strcat(buf, num);
            strcat(buf, "\0");
            } else {
                strcpy(buf, "SESSION-BUSY\0");
            }
        }
        playerSessionsMutex.lock();
        write(clientFd, buf, sizeof(buf));
    }
    addToEpoll(clientFd);
}


//TODO: JAKI BUFOR TUTUAJ JEST                TUTAJ daj JAKIES MUTEXY DO OCZYTU????!!
void sendSessionData(int clientSocket){
    std::string sessionData("");
    if (playerSessions.size() > 0){
        std::string sessionData(std::to_string(sessionData.size()));
        sessionData.append(":");
    }
    for( auto const& [key, val] : playerSessions) {
        int sessionID = key;
        sessionData.append(std::to_string(sessionID));
        sessionData.append("-");
        std::vector<Player> players = val;
        sessionData.append(std::to_string(players.size()));
        for (auto & element : players) {
            sessionData.append(",");
            sessionData.append(element.getNick());
        }
        sessionData.append(";");
        //CZY WYSYLAC POJEDYNCZE INFO O SESJI CZY WLASNIE NA KONCY FUNCJI CALOSC?
    }
    std::vector<char> chars(sessionData.c_str(), sessionData.c_str() + sessionData.size() + 1u); //TODO zobacz czy dziala
    writeData(clientSocket, &chars[0], sizeof(chars));
}


void sessionLoop(int sessionID) { //TODO: OBSŁUŻ wyjście z sesji!!

    //TODO jak sie odlaczy w trakcie to tylko nie wysylaj do niego danych, czyli wywal z sesji i sprawdz na koncu co sie pokrywa

    std::map<std::string, int> playerPoints{}; //TODO: wysyłaj tylko do ych co są w rundzie

    std::set<std::string> usedWords{};
    const unsigned int rounds = 5;

    for (int i = 0; i < rounds; i++) {

        char startMsg[50];
        std::vector<Player> players = playerSessions[sessionID];
        if (players.size() < 2) {

            if (players.size() == 1) {
                int playerFd = playerSessionsFds[sessionID][0];
                strcpy(startMsg, "SESSION-TIMEOUT\0");
                write(playerFd, startMsg, sizeof(startMsg));
            }
            std::this_thread::sleep_for(std::chrono::seconds(10));
            players = playerSessions[sessionID];
            if (players.size() < 2) {
                playerSessionsMutex.lock();

                if (players.size() == 1) { //TODO czy temu jednemu jeszcze wynik wysłać??
                    int playerFd = playerSessionsFds[sessionID][0];
                    strcpy(startMsg, "SESSION-KILL\0");
                    write(playerFd, startMsg, sizeof(startMsg));
                }

                playerSessions.erase(sessionID);
                playerSessionsFds.erase(sessionID);
                playerSessionsMutex.unlock();
                // koniec sesji
                return;
            }
        }

        std::map<std::string, int> currentPlayersFd{};

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

        // na początku rundy komunikat runda ok

        for(auto &pFd: currentPlayersFd){
            strcpy(startMsg, "ROUND-START\0");
            writeData(pFd.second, startMsg, sizeof(startMsg));
        }

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
            char buf[100];
            strcpy(buf, randomWord.c_str());
            strcat(buf, "\0");
            writeData(p.second, buf, sizeof(buf));
        }

        auto start = std::chrono::steady_clock::now();     // start timer
        double roundTime = 120.0 + 10.0; //2 minuty na rundę TODO: plus przesył laggi??
        while (true) {
            auto end = std::chrono::steady_clock::now();
            auto time_span = static_cast<std::chrono::duration<double>>(end - start);

            //TODO: nie blokujaca sprawdz po kazdym czy jest jakis wynik;
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
                }
            }
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
        for (auto &send: currentPlayersFd) {
            std::vector<int> vecFd = playerSessionsFds[sessionID];
            for(auto& i: vecFd){
                if(send.second == i){
                    writeData(send.second, endMsg, sizeof(endMsg));
                }
            }
        }

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
                write(p, scoreBoard, sizeof(scoreBoard));
            }
        }
    }
    playerSessionsMutex.lock();
    for (auto &p: playerSessionsFds[sessionID]) {
        addToEpoll(p);
    }
    //Dodaj do epolla

    playerSessions.erase(sessionID);
    playerSessionsFds.erase(sessionID);
    //Wyrzuć z sesji
    playerSessionsMutex.unlock();

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
        close(epollFd);
        exit(EPOLL_REMOVE);
    }
}


//TODO: jakiś send że zrywamy połączenie?? to raczej w instacji danego problemu dac
void stopConnection(int ClientFd){

    Player p = clientMap[ClientFd];
    std::string nick = p.getNick();

    playerSessionsMutex.lock();
    for(auto &fds: playerSessionsFds){
        std::vector<int> vec = fds.second;
        /*auto it = vec.begin();
        while(it != vec.end()){
            if( (*it) == ClientFd){
                it = vec.erase(it);
            }
            else {
                ++it;
            }
        }*/
        vec.erase( std::remove_if( vec.begin(), vec.end(), [ClientFd](int i){ return i == ClientFd;}), vec.end());
    }
    for(auto &pl: playerSessions){
        std::vector<Player> vec = pl.second;
       /* auto it = vec.begin();
        while(it != vec.end()){
            if( it->getNick() == nick){
                it = vec.erase(it);
            }
            else {
                ++it;
            }
        } */
        vec.erase( std::remove_if( vec.begin(), vec.end(), [nick](Player py){ return py.getNick() == nick;}), vec.end());
    }
    playerSessionsMutex.unlock();

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
    std::cout << "Read ret: " << ret << std::endl;

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
    std::cout << "Read ret: " << ret << std::endl;
    if(ret==-1) perror("write failed on descriptor %d\n");
    if(ret!=count) perror("wrote less than requested to descriptor %d (%ld/%ld)\n");
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

