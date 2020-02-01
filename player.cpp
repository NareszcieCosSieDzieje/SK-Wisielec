
#include "player.hpp"

Player::Player(){
    this->nick = "default";
    this->password = "default";
}

Player::Player(char* nick, char* password) {
    this->nick = nick;
    this->password = password;
}

char *Player::getNick() const {
    return nick;
}

void Player::setNick(char *nick) {
    Player::nick = nick;
}

char *Player::getPassword() const {
    return password;
}

void Player::setPassword(char *password) {
    Player::password = password;
}
