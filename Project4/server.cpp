#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <string.h>
#include <signal.h>
#include "socks.h"

using namespace std;


void reaper(int signo){
    int status;
    while(waitpid(-1,&status,WNOHANG)>0){}
}


int main(int argc,char* argv[]){
    int reuse = 1;
    
    int sockfd,newsockfd;
    
    int port=7001;
    if(argc==2){
        port=atoi(argv[1]);
    }
    socklen_t clilen;
    sockaddr_in serv_addr,cli_addr;
    sockfd=socket(AF_INET,SOCK_STREAM,0);
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");
    
    #ifdef SO_REUSEPORT
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) 
            perror("setsockopt(SO_REUSEPORT) failed");
    #endif
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    serv_addr.sin_port=htons(port);
    if(bind(sockfd,(sockaddr*)&serv_addr,sizeof(serv_addr))<0){
        printf("Bind Error\n");
        return -1;
    }
    listen(sockfd,5);
    

    signal(SIGCHLD,reaper);

    while(1){
        clilen=sizeof(cli_addr);
        newsockfd=accept(sockfd,(sockaddr*)&cli_addr,&clilen);
        //printf("Clinet in\n");
        int childpid=fork();
        if(childpid==0){
            close(sockfd);
            close(0);
            close(1);
            //close(2);
            dup2(newsockfd,0);
            dup2(newsockfd,1);
            //dup2(newsockfd,2);
            close(newsockfd);
            socks(cli_addr);
            exit(0);
        }
        else{
            close(newsockfd);
        }
    }
}
