#ifndef _SERVER3_H_
#define _SERVER3_H_

#define SHMKEY_UserList 10701
#define SHMKEY_BroadcastMsg 10702

struct BroadcastMsg{
    char str[1000];
};

struct User{
    bool isOnline;
    char nickname[50];
    char ip_port[25];
    int pid;
};

#endif