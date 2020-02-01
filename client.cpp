#include <netinet/in.h>
#include <netdb.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/epoll.h>
#include <cstring>
#include <csignal>
#include <string>
#include <map>
#include <vector>
#include <errno.h>
#include <iostream>
#include <thread>

#include "statuses.hpp"
#include "player.hpp"
#include "client.h"
#include "constants.h"

Client::Client()
{

}

void Client::init(){

    // Tu się coś wypierdala srogo, na razie to zostawiam
//    signal(SIGINT, sigHandler);
//    signal(SIGTSTP, sigHandler);

    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    startClient();
    startConnection();

    while (connected) {
        sleep(0.5);
    }
    closeClient();
    return;

      // == KONIEC INIT'A ========= // ----------------------------------------------------------------------------------------------------
     // == to poniżej to są ====== // ----------------------------------------------------------------------------------------------------
    // ==== robocze kody ======== // ----------------------------------------------------------------------------------------------------

    int petla = 0;
    bool connectionValidated = false;
    while(!connectionValidated) {

        petla++;
        //sleep(1);
        if (petla < 2) {
            std::cout << "Connection no: " << petla << std::endl;
            startConnection();
        }
        //get nick i hasło
        char msg[100];
        strcpy(msg, login);
        strcat(msg, "-");
        strcat(msg, password); //Konkatenacja log haslo
//        std::cout << "Write no: " << petla << std::endl;
        writeData(clientFd, msg, sizeof(msg)); //wyslij dane użytkownika
        memset(msg, 0, sizeof(msg)); //odczytaj czy autoryzacja się powiodła
//        std::cout << "Read no: " << petla << std::endl;
        auto x = readData(clientFd, msg, sizeof(msg));
        //Jeśli nie powiodła się autoryzacja spróbuj połączyć sie od nowa.
        if (strncmp(msg, "AUTH-FAIL", 9) == 0) {
            printf("Log in failed! Try again.\n");
        } else if (strncmp(msg, "AUTH-OK", 7) == 0) {
            std::cout << "LOGIN SUCCESSFUL!!!" << std::endl;
            connectionValidated = true;
        }
    }
    //TODO: POŁĄCZ SIE Z DANĄ SESJA
    std::map<int, std::vector<std::string>> playerSessions;
    std::vector<std::string> players;

    close(clientFd);
    while (true) {sleep(1);}
    return;

    bool joinedSession = false;
    while(!joinedSession){


        //TODO: w jakiej pętli ma działać poniższy kod i odczytywanie/wybór sesji

        char msg[1024];
        readData(clientFd, msg, sizeof(msg));
        //sprawdz czy pusty??
        if(msg[0] == '\0'){
            printf("No sessions available.\n");
        }
        else {
            printf("Sessions found.\n");
            playerSessions.clear();
            players.clear();
            char* s;
            s = strtok(msg,":"); // TODO: jak nie zadziała to daj ze delimiter ma wsystkie znaki
            if (s == nullptr){ //TODO: czy to sprawdzać wogóle
                //error;
                printf("strtok error.\n");
            }
            long int numSessions = strtol(s, nullptr, 10);
            for( int i =0; i < numSessions; i++ ) {
                s = strtok(msg, "-");
                long int sessionID = strtol(s, nullptr, 10);
                s = strtok(msg, ",");
                long int numPlayers = strtol(s, nullptr, 10);
                for (int j = 0; j < numPlayers; j++) {
                    s = strtok(msg, ",;");
                    players.push_back(std::string(s));
                }
                playerSessions.insert(std::pair<int, std::vector<std::string>>(sessionID, players));
            }
        }


        //TODO:
        // DWIE OPCJE STWORZ I DOŁĄCZ
        // przetwórz i wyświetl dane sesji roznych one maja ID
        // na podstawie tego co klikniesz daj znac ktora wybierasz
        // wyslij ID SESJI do ktorej chcesz dołączyć
        // czekaj na odpowiedz do ktorej sesji dołaczyłes, i czy, jesli sukces to break
        joinedSessionID = 1;
        joinedSession = true;
    }

    //TODO:JAK OK TO WJEDZ W PETLE GRY

    while(true){


            //send jesli nie wyslal ile mial to od nopwa
            //read clientFd
            //write serverFd
       break;
    }

    closeClient();

    exit(0);
}

void Client::startClient(void){
    if((clientFd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
        perror("Failed to create socket.\n");
        exit(SOCKET_CREATE);
    }
    if( bind(clientFd, (sockaddr*)&bindAddr, sizeof(bindAddr)) < 0 ){
        perror("Failed to bind the socket.\n");
        exit(SOCKET_BIND);
    }
}

void Client::closeClient(void){
    shutdown(clientFd, SHUT_RDWR);
    close(clientFd);
}


void Client::startConnection(void){ //TODO: MOZE POLACZ ZE STARETEM ALE ZOBACZYMY
    int err = getaddrinfo("127.0.0.1", "55555", &hints, &resolved);
    if (err || !resolved){
        perror("Resolving address failed!\n");
        exit(GETADDRINFO_ERROR);
    }
    if ( connect(clientFd, resolved->ai_addr, resolved->ai_addrlen) < 0){
        printf("TU\n");
        perror("Failed to connect.\n");
        exit(SOCKET_CONNECT);
    }
    freeaddrinfo(resolved);
}


void Client::sigHandler(int signal){
    closeClient();
}


ssize_t Client::readData(int fd, char * buffer, ssize_t buffsize){
    auto ret = read(fd, buffer, buffsize);
    std::cout << "Read " << ret << " bytes from server" << std::endl;
    if(ret==-1) {
        // printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
        perror("Read failed\n");
    }
    return ret;
}

void Client::writeData(int fd, char * buffer, ssize_t count){
    auto ret = write(fd, buffer, count);
    std::cout << "Send " << ret << " bytes to server" << std::endl;
    if(ret==-1) perror("Sending failed\n");
    if(ret!=count) perror("Send less than requested to server\n");
}

int Client::tryToLogin(char *log, char *pass) {
    char msg[100];
    strcpy(msg, log);
    strcat(msg, "-");
    strcat(msg, pass);
    writeData(clientFd, msg, sizeof(msg));
    memset(msg, 0, sizeof(msg));
    auto x = readData(clientFd, msg, sizeof(msg));
    if (strncmp(msg, "AUTH-FAIL", 9) == 0) {
        return Constants::LOGIN_FAILED;
    } else if (strncmp(msg, "AUTH-OK", 7) == 0) {
        return Constants::LOGIN_SUCCESSFUL;
    } else {
        return Constants::LOGIN_ERROR;
    }
}

/*
int recv_all(int sockfd, void *buf, size_t len, int flags)
{
    size_t toread = len;
    char  *bufptr = (char*) buf;

    while (toread > 0)
    {
        ssize_t rsz = recv(sockfd, bufptr, toread, flags);
        if (rsz <= 0)
            return rsz;  // Errr or other end closed cnnection

        toread -= rsz;  // Read less next time
        bufptr += rsz;  // Next buffer position to read into
    }

    return len;
}

*/
