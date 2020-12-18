#ifndef SERVER2_H
#define SERVER2_H
#include <string>
#include <vector>

using namespace std;

struct Pipes{
   int rw[2];   // 0 = read, 1 = write
};
struct Numpipes{
    int rw[2];
    int count;
};
struct Env{
    string name;
    string value;
};


struct UserEnv{
    vector<Env> env;
    vector<Numpipes> numpipes;
};

struct User{
    int id;
    string nickname;
    string ip_port;
};

struct UserPipe{
    int from;
    int to;
    int rw[2];
};

const size_t COMMAND_BUFFER=15000;

char welcomeMsg[]="****************************************\n\
** Welcome to the information server. **\n\
****************************************\n";

char pa[]="% ";

int getIDfromFD(int fd);
void printLoginMsg(int fd);
void broadcastLogin(int id);
void who(int fd);
void tell(int fd,int to,string msg);
void yell(int fd,string msg);
void name(int fd,string name);
int execVec(vector<string> line);
void childHandler(int);
void clearUserPipe(int id);


#endif
