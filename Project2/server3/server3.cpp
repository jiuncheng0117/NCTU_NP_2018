#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sstream>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include "server3.h"
#include "npshell.h"

using namespace std;
User *userList;
BroadcastMsg *msg;
int shmid1,shmid2;
int id;

bool checkPipe(int from,int to);

void reaper(int signo){
    if(signo==SIGCHLD){
        int status;
        while(waitpid(-1,&status,WNOHANG)>0){}
        for(int i=0;i<30;i++){
            if(userList[i].isOnline){
                if(kill(userList[i].pid,0)<0){
                    userList[i].isOnline=false;
                    char s[100];
                    sprintf(s,"*** User '%s' left. ***\n",userList[id].nickname);
                    strcpy(msg->str,s);
                    for(int j=0;j<30;j++){
                        if(userList[j].isOnline){
                            kill(userList[j].pid,SIGUSR1);
                        }
                    }
                    char s1[20];
                    for(int j=0;j<30;j++){
                        if(checkPipe(i+1,j+1)){
                            sprintf(s1,"user_pipe/%d_%d",i+1,j+1);
                            remove(s1);
                        }
                        if(checkPipe(j+1,i+1)){
                            sprintf(s1,"user_pipe/%d_%d",j+1,i+1);
                            remove(s1);
                        }
                    }
                    //Remove Pipe;
                    break;
                }
            }
        }
    }
    else{
        shmdt(userList);
        shmdt(msg);
        shmctl(shmid1,IPC_RMID,(shmid_ds*)0);
        shmctl(shmid2,IPC_RMID,(shmid_ds*)0);
        exit(1);
    }
}

int main(int argc,char* argv[]){
    int sockfd,newsockfd;
    int port=7001;
    if(argc==2){
        port=atoi(argv[1]);
    }
    socklen_t clilen;
    sockaddr_in serv_addr,cli_addr;
    sockfd=socket(AF_INET,SOCK_STREAM,0);
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    serv_addr.sin_port=htons(port);
    if(bind(sockfd,(sockaddr*)&serv_addr,sizeof(serv_addr))<0){
        printf("Bind Error\n");
        exit(1);
    }
    listen(sockfd,5);

    

    shmid1=shmget(SHMKEY_UserList,sizeof(User)*30,0666|IPC_CREAT);
    shmid2=shmget(SHMKEY_BroadcastMsg,sizeof(BroadcastMsg),0666|IPC_CREAT);
    userList=(User*)shmat(shmid1,(char*)0,0);
    msg=(BroadcastMsg*)shmat(shmid2,(char*)0,0);

    for(int i=0;i<30;i++){
        userList[i].isOnline=false;
    }

    signal(SIGCHLD,reaper);
    signal(SIGINT,reaper);

    int childpid;
    while(1){
        clilen=sizeof(cli_addr);
        newsockfd=accept(sockfd,(sockaddr*)&cli_addr,&clilen);
        printf("Clinet in\n");
        for(id=0;id<30;id++){
            if(userList[id].isOnline==false){
                break;
            }
        }
        childpid=fork();
        if(childpid==0){
            close(sockfd);
            close(0);
            close(1);
            close(2);
            dup2(newsockfd,0);
            dup2(newsockfd,1);
            dup2(newsockfd,2);
            close(newsockfd);
            usleep(10);
            npshell();
            exit(0);
        }
        else{
            
            strcpy(userList[id].nickname,"(no name)");
            userList[id].isOnline=true;
            //userList[id]
            char str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(cli_addr.sin_addr), str, INET_ADDRSTRLEN);
            //get port
            int port=ntohs(cli_addr.sin_port);
            char s[100];
            sprintf(s,"%s/%d",str,port);
            strcpy(userList[id].ip_port,s);
            strcpy(userList[id].ip_port,"CGILAB/511");
            userList[id].pid=childpid;

            close(newsockfd);
        }
    }
}

bool checkPipe(int from,int to){
    char s[20];
    sprintf(s,"user_pipe/%d_%d",from,to);
    if(access(s,F_OK)!=-1)
        return true;
    return false;
}
