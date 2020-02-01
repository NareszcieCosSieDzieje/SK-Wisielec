#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <string>
#include <iostream>

using namespace std;

class Constants
{
public:

    static inline const string STYLE_BUTTON = "background-color: rgb(68, 10, 10);\ncolor: rgb(243, 243, 243)";
    static const inline string STYLE_BUTTON_DISABLED = "background-color: rgb(30, 4, 4); color: rgb(140, 140, 140)";

    static inline const int LOGIN_SUCCESSFUL = 1;
    static inline const int LOGIN_FAILED = 0;
    static inline const int LOGIN_ERROR = -1;
};

#endif // CONSTANTS_H
