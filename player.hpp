
#ifndef WISIELEC_PLAYER_HPP
#define WISIELEC_PLAYER_HPP

#include "string"

class Player {
private:
    char* nick;
    char* password;

public:
    Player();
    Player(char* nick, char* password);
    //~Player();

    char *getNick() const;

    void setNick(char *nick);

    char *getPassword() const;

    void setPassword(char *password);

};


#endif //WISIELEC_PLAYER_HPP
