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
    static inline const char* START_SESSION = "START-SESSION\0";
    static inline const char* JOINING_TO_EXISTING_SESSION = "AUTH-JOIN-YES\0";
    static inline const char* NO_JOINING_TO_EXISTING_SESSION = "AUTH-JOIN-NO\0";
};

enum AlreadyInGame {
    YES,
    NO
};

enum SessionMessage {
    CREATED,
    JOINED,
    MAX,
    BUSY,
    KILLED,
    SESSION_ERROR
};

enum SessionStart {
    OK,
    FAIL
};

enum GettingDataType {
    Players,
    Sessions,
    Game
};

#endif // CONSTANTS_H
