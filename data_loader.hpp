
#ifndef SIECI_WISIELEC_DATA_LOADER_HPP
#define SIECI_WISIELEC_DATA_LOADER_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>


const std::string delims("{,;:}\n"); //\n\t
inline bool isDelim(char c) {
    for (int i = 0; i < delims.size(); ++i)
        if (delims[i] == c)
            return true;
    return false;
}

//DODAŁEM PARAMETR DO ROZROZNIENIA REJESTRACJI I LOGOWANIA
bool searchForUserData(std::string login, std::string password, bool newAccount) {

    std::ifstream userFile;
    userFile.open ("../user_data.txt", std::ios::in);
    if (userFile.is_open()) {
        //TODO: if empty error??
        std::string line;
        while (std::getline(userFile, line)) {
            std::stringstream stringStream(line);
            int i = 0;
            int found = 0;
            stringStream.seekg(1);
            while (stringStream) {
                std::string word;
                char c;
                while (!isDelim((c = stringStream.get()))) {
                    word.push_back(c);
                }
                if (c != EOF) {
                    stringStream.unget();
                }
                if (i%2 == 0) {
                    if (login == word){
                        found++;
                        if (newAccount == true){
                            return true;
                        }
                    }
                } else {
                    if(password == word){
                        found++;
                    }
                    if (found < 2){
                        found = 0;
                    } else {
                        userFile.close();
                        return true;
                    }
                }
                while (isDelim((c = stringStream.get())));
                if (c != EOF) {
                    stringStream.unget();
                }
                i++;
            }
        }
        userFile.close();
    } else {
        std::cout << "Couldn't open file!" << std::endl;
    }
    return false;
}


void addUser(std::string user, std::string password){
    std::fstream userFile;
    userFile.open("../user_data.txt",  std::fstream::in | std::fstream::out | std::fstream::ate ); // append instead of overwrite
    if (userFile.is_open()){
        long pos = userFile.tellp();
        userFile.seekp(pos-2);
        userFile << ",{" + user + ":" + password + "};";
        userFile.close();
        std::cout<<"Successfully appended file!" <<std::endl;
    } else {
        std::cout<< "Couldn't open file!" <<std::endl;
    }
}


std::string getRandomWord(void){
    
    std::string randomWord {};
    int numberOfLines = 0;
    std::string line {};
    std::ifstream wordsFile;
    wordsFile.open("../words.txt", std::ifstream::in);
    if(wordsFile.is_open()){
        while (std::getline(wordsFile, line)){
            ++numberOfLines;
        }
        if(numberOfLines > 0){
            wordsFile.clear();
            wordsFile.seekg(0, std::ios_base::beg);
            int randomIndex = rand() % numberOfLines + 1;
            for(int i = 0; i < randomIndex; i++){ //lepiej by bylo zrobic jakies seekg o te x * \n pewnie
                std::getline(wordsFile, line);
            }
            line.resize(line.size()-1);
            randomWord = line;
        } else {
            randomWord = "SuchEmptiness";
        }
    } else {
        std::cout<<"Couldn't open the word file!" <<std::endl;
    }
    wordsFile.close();
    return randomWord;
}


#endif //SIECI_WISIELEC_DATA_LOADER_HPP
