
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

#include "statuses.hpp"
#include "player.hpp"
#include "player.cpp"
#include "user_loader.hpp"


//=====================================GLOBALS============================================\\

std::vector<std::thread> threadVector;
std::vector<int> clientSockets;


std::map<int, Player> clientMap;
std::mutex clientMapMutex;

std::map<int, std::vector<Player>> playerSessions;
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
void sessionLoop(int sessionID);
void joinSession(int clientFd);
std::string* loadUserData(char* filePath); //TODO: zwróć array stringów


/* TODO: ZADANIA SERWERA:
 * nasłuchuj połączeń;
 * pokazuj sesje dostępne i ile graczy;
 * obsługuj tworzenie sesji, zamykanie, dołączanie,
 * obsługuj przebieg rozgrywki sesji;
*/


//=======================================MAIN=============================================\\

int main(int argc, char* argv[]){


    //TODO: SERVER MUSI WCZYTAC LISTE LOGINOW I HASEL I SPRAWDZAC KTORE SA ZUZYTE

    signal(SIGINT, sigHandler);
    signal(SIGTSTP, sigHandler);

    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY); //TODO: ZMIEN NA ADRESOKRESLONY


    startServer();


    std::cout<< "MAIN PO LOOP" << std::endl;


    //obsluguj deskryptory graczy


    struct epoll_event events[maxEvents];
    bool polling = true;

    while(polling) {

        printf("\nPolling for input...\n");
        int event_count = epoll_wait(epollFd, events, maxEvents, -1);
        printf("%d ready events\n", event_count);
        for (int i = 0; i < event_count; i++) {

            struct epoll_event clientEvent = events[i];
            int clientFd = clientEvent.data.fd;
            printf("Reading file descriptor '%d' -- ", clientFd);

            /*
            if( clientEvent.events & EPOLLIN && clientEvent.data.fd == sock ){
                if read == 0 then zamknij klienta
            }

            bytes_read = read(events[i].data.fd, read_buffer, READ_SIZE);
            printf("%zd bytes read.\n", bytes_read);
            read_buffer[bytes_read] = '\0';
            printf("Read '%s'\n", read_buffer);
            */

        }
    }

          //FIXME: xd
    if (close(epollFd) < 0){
        perror("Server epoll close error\n");
        exit(EPOLL_CLOSE);
    }


            //TODO: gdzieś indziej! ALE OGARNIJ CZEKANIE NA WĄTKI
    while (!threadVector[0].joinable()){

    }
    threadVector[0].join();


    return 0;
}


//=======================================FUNC-DEC=========================================\\

void startServer(void){
    if( (serverFd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
        perror("Failed to create socket.\n");
        exit(SOCKET_CREATE);
    }
    int enable = 1;
    if( setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0 ){
        perror("Setsockopt(SO_REUSEADDR) failed.\n");
        exit(SOCKET_REEUSE);
    }
    if( bind(serverFd, (sockaddr*)&bindAddr, sizeof(bindAddr)) < 0 ){
        perror("Failed to bind the socket.\n");
        exit(SOCKET_BIND);
    }
    if( listen(serverFd, 5) < 0 ){
        perror("Failed to listen.\n");
        exit(SOCKET_LISTEN);
    }
    printf("1\n");
    epollFd = epoll_create1(0);
    if (epollFd < 0){
        perror("Server epoll error!\n");
        exit(EPOLL_ERROR);
    }
    std::thread loopThread(listenLoop); //Uruchom wątek nasłuchujący nowych połączeń.
    threadVector.push_back(std::move(loopThread)); //Wątek nie może być skopiowany.
}


//TODO: czy wątek/funkcja ma cały czas w pętli nawalać accepty? czy lepsze rozwiązanie jest?
void listenLoop(void){
    while(true){
        sockaddr_in clientAddr{};
        socklen_t cliLen = sizeof(clientAddr);
        int newClient = accept(serverFd, (struct sockaddr *)&clientAddr, &cliLen); //Nawiąż nowe połączenie z klientem.
        if (newClient < 0) {
            perror("ERROR on accept.\n");
            exit(SOCKET_ACCEPT);
        }
        std::thread validationThread(clientValidation, newClient); //Nowe połączenie przeslij do zweryfikowania
        validationThread.detach();
    }
    //TODO: jeśli jakis condition_variable to zakoncz prace?
}


void clientValidation(int newClientFd){

    std::cout << "WERYFIKACJA KLIENTA\n" << std::endl;
    //TODO: sprawdz login haslo jesli rip to wywal, jak ok to dodaj, mozliwe jeszcze sprawdzanie portu ale jak jest haslo to raczej bez sensu?

    const unsigned int signin = 1;
    const unsigned int signup = 2;

    char conType[10];
    auto xRead = readData(newClientFd, conType, sizeof(conType));
    if(xRead != 10){
        perror("User data sending error 1.\n");
    }
    std::cout << "con type: " << conType << std::endl; ;
    int cT = (int) strtol(conType, nullptr, 10);
    char msg[100];
    xRead = readData(newClientFd, msg, sizeof(msg) );
    if(xRead != 100){
        perror("User data sending error 2.\n");
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
        if (!searchForUserData(loginS, passwordS)) {
            addUser(loginS, passwordS);
            userExists = true;
        } else {
            // TODO: WHAT TO DO THEN cant log error send
        }
    } else if (cT == signin){
        userExists = searchForUserData(loginS, passwordS); //WYWOŁANIE FUNKCJI CZYTAJĄCEJ Z PLIKU
    }
    if (userExists) {
        Player newPlayer(login, pass);
        //Dodaj do mapy klientow -graczy
        clientMapMutex.lock();
        clientMap.insert(std::pair<int, Player>(newClientFd, newPlayer));
        clientMapMutex.unlock();

        //TODO: czy events okej??
        struct epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = newClientFd;

        if(epoll_ctl(epollFd, EPOLL_CTL_ADD, newClientFd, &event))
        {
            fprintf(stderr, "Failed to add file descriptor to epoll.\n");
            close(epollFd);
            exit(12); //FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!:
        }
        //Wyslij ack ze sie zalogował
        writeData(newClientFd, "AUTH-OK\0", sizeof("AUTH-OK\0"));
        joinSession(newClientFd);
    } else {
        writeData(newClientFd, "AUTH-FAIL\0", sizeof("AUTH-FAIL\0"));
        std::cout << "zamykam połączenie z klientem: " << newClientFd << std::endl;
        stopConnection(newClientFd);
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}


void joinSession(int clientFd){

    Player player = clientMap[clientFd];

    bool joinedSession = false;
    int sessionMode = -1;
    while(!joinedSession){
        sendSessionData(clientFd);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        char* sessionId;
        int read = recv(clientFd, sessionId, sizeof(sessionId), MSG_DONTWAIT); //response non block get chosen session
        if (read != 0){
            sessionMode = (int) strtol(sessionId, NULL, 10);
            joinedSession = true;
        }
    }

    int finalSessionId = -1;
    char buf[100];
    if(sessionMode < 0){
        //error
    }
    else if (sessionMode == 0){ //TODO: JAKIES ODSWEIZANIE NUMERKOW - ZA WYSOKIE WARTOSCI?
        if (playerSessions.empty()){
            finalSessionId = 1;
            std::vector<Player> playerVector;
            playerVector.push_back(player);
            playerSessionsMutex.lock();
            playerSessions.insert(std::pair<int, std::vector<Player>>(finalSessionId, playerVector));
            playerSessionsMutex.unlock();
            strcpy(buf, "SESSION-1\0");
        } else if (playerSessions.size() < maxSessions )   {
            finalSessionId = playerSessions.size() +1;
            std::vector<Player> playerVector;
            playerVector.push_back(player);
            playerSessionsMutex.lock();
            playerSessions.insert(std::pair<int, std::vector<Player>>(finalSessionId, playerVector));
            playerSessionsMutex.unlock();
            char num[10];
            sprintf (num, "%d", finalSessionId);
            strcpy(buf, "SESSION-");
            strcat(buf, num);
            strcat(buf, "\0"); //czy potrzebne?
        } else {
            //error nie mozna zrobic sesji;
            strcpy(buf, "SESSION-MAX\0");
            //TODO: !!!!!!!!!!!!!!!!!!_______________________________________ WYJDZ
        }
        write(clientFd, buf, sizeof(buf));
        //read czekaj na start od hosta!//TODO:
        sessionLoop(finalSessionId);
    } else {
        playerSessionsMutex.lock();
        // TODO SPRAWDZ CZY SIZE 4 jak tak to daj erro ze juz sie nie da; klient niech tez to sprawdza
        if (playerSessions[sessionMode].size() < playersPerSession){
            playerSessions[sessionMode].push_back(player);
        } else {
            strcpy(buf, "SESSION-BUSY\0");
        }
        playerSessionsMutex.lock();
        write(clientFd, buf, sizeof(buf));
    }
}



//TODO: JAKI BUFOR TUTUAJ JEST                TUTAJ daj JAKIES MUTEXY DO OCZYTU????!!
void sendSessionData(int clientSocket){
    std::string sessionData("");
    if (playerSessions.size() > 0){
        std::string sessionData(std::to_string(sessionData.size()));
        sessionData.append(":");
    }
    for( auto const& [key, val] : playerSessions )
    {
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


// DOROBIC START SESJI WGL!!
void sessionLoop(int sessionID) { //TODO: jak to rozwiązać
    //id do rozpoznawania gracyz w sesji



    while(true){


        //conduct session
    }


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


//TODO: jakiś send że zrywamy połączenie?? to raczej w instacji danego problemu dac
void stopConnection(int ClientFd){
    std::cout << "Zamykam polaczenie w stopCon" << std::endl;
    if (shutdown(ClientFd, SHUT_RDWR) < 0 ){
        perror("Failed to disconnect with the client.\n");
        //FIXME: exit?
    }
    close(ClientFd);
}


void sigHandler(int signal){
    printf("CTRL + C\n");
    if (threadVector[0].joinable()) {
        threadVector[0].join();
    }
    //TODO: closeServer();
}


ssize_t readData(int fd, char * buffer, ssize_t buffsize){
    auto ret = read(fd, buffer, buffsize);
    std::cout << "Read ret: " << ret << std::endl;
    if(ret==-1) {
       // printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
        perror("read failed on descriptor %d\n");
    }
    //perror("read failed on descriptor %d\n");
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

