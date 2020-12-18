#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <string.h>
#include <arpa/inet.h>
#include <sstream>
#include <fstream>
#include "socks.h"
using namespace std;

string filename="socks.conf";

bool isBind=false;
bool permit;

Request req;
Reply reply;
uint8_t rep[8];
sockaddr_in cli_addr,sock_addr;

int sockfd;
fd_set rfds,afds;
int nfds;

int socks(sockaddr_in _cli_addr){
    cli_addr=_cli_addr;
    char buffer[1000];
    read(0,buffer,1000);
    memcpy(&req,buffer,8);
    if(req.vn!=4){
        return -1;
    }

    if(req.cd==1)
        isBind=false;
    else
        isBind=true;

    req.dest_port=ntohs(req.dest_port);
    permit=firewall();
    printSocks();
    if(!permit){
        reply.vn=0;
        reply.dest_port=htons(req.dest_port);
        reply.dest_ip=req.dest_ip;
        reply.cd=91;
        write(1,&reply,sizeof(Reply));
        exit(1);
    }
    reply.vn=0;
    reply.dest_port=htons(req.dest_port);
    reply.dest_ip=req.dest_ip;
    reply.cd=90;
    if(!isBind){
        createConnect();
        write(1,&reply,sizeof(Reply));
    }
    else{
        createBind();
        write(1,rep,8);
    }

    
    nfds=getdtablesize();
    FD_ZERO(&afds);
    FD_SET(0,&afds);
    FD_SET(sockfd,&afds);
    int cc;
    while (1) {
        memcpy(&rfds,&afds,sizeof(rfds));
        if(select(nfds,&rfds,(fd_set*)0,(fd_set*)0,0)<0){
            continue;
        }
        if(FD_ISSET(0,&rfds)){
            cc = read (0, buffer, 1000);
			if (cc == 0) {
                exit(0);
			} else {
				write (sockfd, buffer, cc);
			}
        }
        if(FD_ISSET(sockfd,&rfds)){
            cc = read (sockfd, buffer, 1000);
			if (cc == 0) {
                exit(0);
			} else {
				write (0, buffer, cc);
			}
        }
    }
}

void printSocks(){
    stringstream ss;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(cli_addr.sin_addr.s_addr), str, INET_ADDRSTRLEN);
    ss<<"<S_IP>: "<<str<<endl;
    ss<<"<S_PORT>: "<<ntohs(cli_addr.sin_port)<<endl;
    inet_ntop(AF_INET, &(req.dest_ip), str, INET_ADDRSTRLEN);
    ss<<"<D_IP>: "<<str<<endl;
    ss<<"<D_PORT>: "<<req.dest_port<<endl;
    ss<<"<Command>: "<< ((req.cd==1)?"CONNECT":"BIND")<<endl;
    ss<<"<Reply>: "<<((permit)?"Accept":"Reject")<<endl<<endl;
    cerr<<ss.str();
    //cerr<<"<Reply>: "<<Accept or Reject;
}

void createConnect(){
    sock_addr.sin_family=AF_INET;
    sock_addr.sin_addr=req.dest_ip;
    sock_addr.sin_port=htons(req.dest_port);
    sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(connect(sockfd,(sockaddr*)&sock_addr,sizeof(sock_addr))<0){
        cerr<<"Connect Fail"<<endl;
        exit(1);
    }
}

void createBind(){
    int msock;
    socklen_t clilen = sizeof (struct sockaddr_in);
	sockaddr_in	cli;
    sock_addr.sin_family=AF_INET;
    sock_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    sock_addr.sin_port=htons(INADDR_ANY);
    msock=socket(AF_INET,SOCK_STREAM,0);
    if(bind(msock,(sockaddr*)&sock_addr,sizeof(sock_addr))<0){
        cerr<<"Bind Fail"<<endl;
        exit(1);
    }
    if(listen(msock,5)<0){
        cerr<<"Bind Fail"<<endl;
        exit(1);
    }
    getsockname (msock, (struct sockaddr *) &cli, &clilen);
    rep[0]=0;
    rep[1]=90;
    rep[2]=(uint8_t)(ntohs(cli.sin_port)/256);
    rep[3]=(uint8_t)(ntohs(cli.sin_port)%256);
    rep[4]=0;
    rep[5]=0;
    rep[6]=0;
    rep[7]=0;
    write(1,rep,8);
    //cerr<<(int)cli.sin_port<<endl;
    clilen = sizeof (struct sockaddr_in);
	if ((sockfd = accept (msock, (struct sockaddr *) &cli, &clilen)) < 0) {
		cerr<<"Accept Fail"<<endl;
		exit(1);
	}
    //cerr<<"Accepted!!"<<endl;
}

bool firewall(){
    string ip[4];
    string dest[4];
    char _strip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(req.dest_ip), _strip, INET_ADDRSTRLEN);
    string strip=_strip;
    for(int i=0;i<4;i++){
        dest[i]=strip.substr(0,strip.find('.'));
        strip=strip.substr(strip.find('.')+1);
    }
    //cerr<<dest[0]<<" "<<dest[1]<<" "<<dest[2]<<" "<<dest[3]<<endl;

    ifstream ifs;
    ifs.open(filename.c_str());
    string str;
    while(getline(ifs,str)){
        if(str[0]=='#')
            continue;
        str=str.substr(str.find(' ')+1);
        if(str[0]=='c'){
            if(isBind){
                continue;
            }
            str=str.substr(str.find(' ')+1);
            str=str.substr(0,str.find(' '));
            for(int i=0;i<4;i++){
                ip[i]=str.substr(0,str.find('.'));
                str=str.substr(str.find('.')+1);
            }
            bool check=true;
            for(int i=0;i<4;i++){
                if(ip[i]=="*")
                    continue;
                if(ip[i]!=dest[i]){
                    check=false;
                    break;
                }
            }
            if(check)
                return true;
        }
        else if(str[0]=='b'){
            if(!isBind){
                continue;
            }
            str=str.substr(str.find(' ')+1);
            str=str.substr(0,str.find(' '));
            for(int i=0;i<4;i++){
                ip[i]=str.substr(0,str.find('.'));
                str=str.substr(str.find('.')+1);
            }
            bool check=true;
            for(int i=0;i<4;i++){
                if(ip[i]=="*")
                    continue;
                if(ip[i]!=dest[i]){
                    check=false;
                    break;
                }
            }
            if(check)
                return true;
        }
    }
    return false;
}
