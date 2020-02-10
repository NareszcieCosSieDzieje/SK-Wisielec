#ifndef CONSTANTS_H
#define CONSTANTS_H

enum AuthorizationStatus {
    SUCCESSFUL,
    FAILED,
    ERROR
};

enum AuthorizationType {
    SIGNIN = 1,
    SIGNUP = 2
};

enum ServerDataType {
    SESSIONS,
    PLAYERS
};

class ConnectionProcesses {
public:
    static inline const char* VALIDATION = "CLIENT-VALIDATION\0";
    static inline const char* SESSION_DATA = "SEND-SESSION-DATA\0";
    static inline const char* USER_DATA = "SEND-USER-DATA\0";
    static inline const char* SESSION_JOIN = "JOIN-SESSION\0";
    static inline const char* SESSION_OUT = "DISSOCIATE-SESSION\0";
    static inline const char* DISCONNECT = "DISCONNECTING\0";
    static inline const char* LOGOUT = "LOG-OUT\0";
};

enum SessionMessage {
    CREATED,
    JOINED,
    MAX,
    BUSY,
    KILLED
};

enum GettingDataType {
    Players,
    Sessions
};

#endif // CONSTANTS_H
