
#ifndef WISIELEC_PLAYER_HPP
#define WISIELEC_PLAYER_HPP

#include "string"

class Player {
private:
    std::string nick;
    std::string password;

public:
    Player();

    Player(const std::string &nick, const std::string &password);

    const std::string &getNick() const;

    void setNick(const std::string &nick);

    const std::string &getPassword() const;

    void setPassword(const std::string &password);

};


#endif //WISIELEC_PLAYER_HPP
