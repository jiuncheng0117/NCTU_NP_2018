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
#include "server2.h"

using namespace std;

int PORT=7000;

int userFdTable[30];
vector<User> userList;
vector<UserEnv> userEnv;
vector<UserPipe> userPipe;
int userOnline=0;

int msock;
fd_set rfds,afds;
int nfds;

int main(int argc,char* argv[]){
    clearenv();
    if(argc==2){
        PORT=atoi(argv[1]);
    }
    for(int i=0;i<30;i++){
        userFdTable[i]=-1;
    }

    userList.resize(30);
    userEnv.resize(30);
    

    socklen_t alen;

    sockaddr_in serv_addr,fsin;
    msock=socket(AF_INET,SOCK_STREAM,0);
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    serv_addr.sin_port=htons(PORT);
    if(bind(msock,(sockaddr*)&serv_addr,sizeof(serv_addr))<0){
        printf("Bind Error\n");
        exit(1);
    }
    listen(msock,5);

    nfds=getdtablesize();
    FD_ZERO(&afds);
    FD_SET(msock,&afds);

    signal(SIGCHLD,childHandler);
    int c=0;
    while(1){
        memcpy(&rfds,&afds,sizeof(rfds));
        if(select(nfds,&rfds,(fd_set*)0,(fd_set*)0,0)<0){
            continue;
        }
        
        if(FD_ISSET(msock,&rfds)){
            int ssock;
            alen=sizeof(fsin);
            ssock=accept(msock,(sockaddr*)&fsin,&alen);
            FD_SET(ssock,&afds);
            printLoginMsg(ssock);
            int i;
            for(i=0;i<30;i++){
                if(userFdTable[i]==-1)
                    break;
            }
            userFdTable[i]=ssock;
            User user;
            user.id=i;
            user.nickname="(no name)";
            //get ip
            char str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(fsin.sin_addr), str, INET_ADDRSTRLEN);
            //get port
            int port=ntohs(fsin.sin_port);
            ostringstream ss;
            ss<<port;
            user.ip_port=string(str)+"/"+ss.str();
            user.ip_port="CGILAB/511";
            userList[i]=user;
            userOnline++;
            Env e;
            e.name="PATH";
            e.value="bin:.";
            UserEnv env;
            env.env.push_back(e);
            env.numpipes.clear();
            userEnv[i]=env;

            broadcastLogin(i);
            write(ssock,pa,strlen(pa));
            
        }
        for(int fd=3;fd<nfds;fd++){
            if(fd!=msock&&FD_ISSET(fd,&rfds)){
                FD_CLR(msock,&rfds);
                int id=getIDfromFD(fd);
                char in[COMMAND_BUFFER]="\0";
                char input[COMMAND_BUFFER]="\0";
                char inputSave[COMMAND_BUFFER]="\0";
                int cc;
                cc=read(fd,in,sizeof(in));
                if(cc==0){
                    char s[1000];
                    sprintf(s,"*** User '%s' left. ***\n",userList[id].nickname.c_str());
                    for(int i=0;i<30;i++){
                        if(userFdTable[i]!=-1){
                            write(userFdTable[i],s,strlen(s));
                        }
                    }
                    clearUserPipe(id);
                    close(fd);
                    FD_CLR(fd,&afds);
                    userOnline--;
                    userFdTable[id]=-1;
                    
                    userEnv[id].env.clear();
                    userEnv[id].numpipes.clear();
                    
                    continue;
                }
                char *current_pos = strchr(in,'\r');
                while (current_pos){
                    *current_pos = '\n';
                    *current_pos = '\0';
                    current_pos = strchr(current_pos,'\r');
                }
                if(in[strlen(in)-1]=='\n')
                    in[strlen(in)-1]='\0';
                strcpy(input,in);
                strcpy(inputSave,in);
                //printf("%d:%s\n",strlen(in),in);

                if(strlen(in)==0){
                    write(fd,pa,strlen(pa));
                    continue;
                }
                if(strcmp(in,"exit")==0){
                    char s[1000];
                    sprintf(s,"*** User '%s' left. ***\n",userList[id].nickname.c_str());
                    for(int i=0;i<30;i++){
                        if(userFdTable[i]!=-1){
                            write(userFdTable[i],s,strlen(s));
                        }
                    }
                    clearUserPipe(id);
                    close(fd);
                    FD_CLR(fd,&afds);
                    userOnline--;
                    userFdTable[id]=-1;

                    userEnv[id].env.clear();
                    userEnv[id].numpipes.clear();
                    
                    continue;
                }

                clearenv();
                for(int i=0;i<userEnv[id].env.size();i++){
                    setenv(userEnv[id].env[i].name.c_str(),userEnv[id].env[i].value.c_str(),1);
                }
                
                vector <Numpipes> numpipes=userEnv[id].numpipes;

                vector<vector<string> > command;
                int numOfCmd;
                bool directToFile=false;
                int numPipeType=-1;     //-1:None   0:stdout    1:stdout+stderr
                int numPipeCount=0;
                int pipeToID=0;
                int pipeFromID=0;
                int userPipefdInR=-1;
                int userPipefdInW=-1;
                int userPipefdOutR=-1;
                int userPipefdOutW=-1;
                string fileName="";
                vector <int> pidTable;

                char *str;
                str = strtok(in," ");
                int i=0;
                command.push_back(vector<string>());
                
                while (str != NULL){
                    if(str[0]=='|' || str[0]=='!'){
                        if(strlen(str)>1){
                            numPipeType=(str[0]=='|'?0:1);
                            str[0]=' ';
                            numPipeCount=atoi(str);
                        }
                        else{
                            command.push_back(vector<string>());
                            i++;
                        }
                        str = strtok (NULL, " ");
                        continue;
                    }
                    else if (str[0]=='>'){
                        if(strlen(str)==1){
                            directToFile=true;
                            str = strtok (NULL, " ");
                            fileName=string(str);
                            break;
                        }
                        else{
                            str[0]=' ';
                            pipeToID=atoi(str);
                            str = strtok (NULL, " ");
                            continue;
                        }
                    }
                    else if(str[0]=='<'){
                        str[0]=' ';
                        pipeFromID=atoi(str);
                        str = strtok (NULL, " ");
                        continue;
                    }
                    command[i].push_back(string(str));
                    str = strtok (NULL, " ");
                }
                numOfCmd=command.size();
                /*
                for(int i=0;i<command.size();i++){
                    for(int j=0;j<command[i].size();j++){
                        printf("%s ",command[i][j].c_str());
                    }
                    printf("\n");
                }
                if(directToFile){
                    printf("FileName:%s\n",fileName.c_str());
                } 
                if(pipeToID){
                    printf("Pipe to %d\n",pipeToID);
                }
                if(pipeFromID){
                    printf("Pipe from %d\n",pipeFromID);
                }
                */

               for(int i=0;i<numpipes.size();i++){
                    --numpipes[i].count;
                }

                Numpipes numpipeOut,numpipeIn;
                bool pipeIn=false;
                
                if(numPipeType>-1){
                    int same=-1;
                    for(int i=0;i<numpipes.size();i++){
                        if(numpipes[i].count==numPipeCount){
                            same=i;
                            break;
                        }
                    }
                    if(same>-1){
                        numpipeOut=numpipes[same];
                    }
                    else{
                        numpipeOut.count=numPipeCount;
                        pipe(numpipeOut.rw);
                        numpipes.push_back(numpipeOut);
                    }
                }

                for(int i=0;i<numpipes.size();i++){
                    if(numpipes[i].count==0){
                        pipeIn=true;
                        numpipeIn=numpipes[i];
                        numpipes.erase(numpipes.begin()+i);
                        close(numpipeIn.rw[1]);
                        break;
                    }
                }


                //User Pipe
                if(pipeFromID>0){
                    if(userFdTable[pipeFromID-1]==-1){
                        char s[100];
                        sprintf(s,"*** Error: user #%d does not exist yet. ***\n",pipeFromID);
                        write(fd,s,strlen(s));
                        write(fd,pa,strlen(pa));
                        userEnv[id].numpipes=numpipes;
                        continue;
                    }
                }
                
                if(pipeToID>0){
                    if(userFdTable[pipeToID-1]==-1){
                        char s[100];
                        sprintf(s,"*** Error: user #%d does not exist yet. ***\n",pipeToID);
                        write(fd,s,strlen(s));
                        write(fd,pa,strlen(pa));
                        userEnv[id].numpipes=numpipes;
                        continue;
                    }
                }
                
                if(pipeFromID>0){
                    bool check=false;
                    UserPipe p;
                    vector<UserPipe>::iterator it;
                    for(it=userPipe.begin();it<userPipe.end();it++){
                        if((it->from==(pipeFromID-1))&&(it->to==id)){
                            check=true;
                            p=(*it);
                            break;
                        }
                    }
                    if(!check){
                        char s[100];
                        sprintf(s,"*** Error: the pipe #%d->#%d does not exist yet. ***\n",pipeFromID,id+1);
                        write(fd,s,strlen(s));
                        write(fd,pa,strlen(pa));
                        userEnv[id].numpipes=numpipes;
                        continue;
                    }
                    else{
                        userPipefdInR=p.rw[0];
                        userPipefdInW=p.rw[1];
                        char s[100];
                        sprintf(s,"*** %s (#%d) just received from %s (#%d) by '%s' ***\n",userList[id].nickname.c_str(),id+1,userList[pipeFromID-1].nickname.c_str(),pipeFromID,inputSave);
                        for(int i=0;i<30;i++){
                            if(userFdTable[i]!=-1){
                                write(userFdTable[i],s,strlen(s));
                            }
                        }
                        userPipe.erase(it);
                    }
                }

                if(pipeToID>0){
                    bool check=true;
                    for(vector<UserPipe>::iterator it=userPipe.begin();it<userPipe.end();it++){
                        if((it->from==id)&&(it->to==(pipeToID-1))){
                            check=false;
                            break;
                        }
                    }
                    if(!check){
                        char s[100];
                        sprintf(s,"*** Error: the pipe #%d->#%d already exists. ***\n",id+1,pipeToID);
                        write(fd,s,strlen(s));
                        write(fd,pa,strlen(pa));
                        userEnv[id].numpipes=numpipes;
                        continue;
                    }
                    else{
                        UserPipe p;
                        p.from=id;
                        p.to=pipeToID-1;
                        pipe(p.rw);
                        userPipe.push_back(p);
                        userPipefdOutR=p.rw[0];
                        userPipefdOutW=p.rw[1];
                        char s[100];
                        sprintf(s,"*** %s (#%d) just piped '%s' to %s (#%d) ***\n",userList[id].nickname.c_str(),id+1,inputSave,userList[pipeToID-1].nickname.c_str(),pipeToID);
                        for(int i=0;i<30;i++){
                            if(userFdTable[i]!=-1){
                                write(userFdTable[i],s,strlen(s));
                            }
                        }
                    }
                }
               
               

                if(command[0][0]=="who"){
                    who(fd);
                    write(fd,pa,strlen(pa));
                    userEnv[id].numpipes=numpipes;
                    continue;
                }

                if(command[0][0]=="tell"){
                    int to=atoi(command[0][1].c_str());
                    char *s=strchr(input,' ');
                    s=strchr(s+1,' ');
                    s++;
                    while(*s==' ')s++;
                    string msg=string(s);
                    tell(fd,to,msg);
                    write(fd,pa,strlen(pa));
                    userEnv[id].numpipes=numpipes;
                    continue;
                }

                if(command[0][0]=="yell"){
                    char *s=strchr(input,' ');
                    s++;
                    while(*s==' ')s++;
                    string msg=string(s);
                    yell(fd,msg);
                    write(fd,pa,strlen(pa));
                    userEnv[id].numpipes=numpipes;
                    continue;
                }

                if(command[0][0]=="name"){
                    char *s=strchr(input,' ');
                    s++;
                    while(*s==' ')s++;
                    string msg=string(s);
                    name(fd,msg);
                    write(fd,pa,strlen(pa));
                    userEnv[id].numpipes=numpipes;
                    continue;
                }

                // Executing --

                //handle printenv
                if(command[0][0]=="printenv"){
                    char *str=getenv(command[0][1].c_str());
                    if(str!=NULL){
                        write(fd,str,strlen(str));
                        write(fd,"\n",1);
                        //printf("%s\n",str);
                    }
                    write(fd,pa,strlen(pa));
                    userEnv[id].numpipes=numpipes;
                    continue;
                }
                
                //handle setenv
                bool check=false;
                if(command[0][0]=="setenv"){
                    if(command[0].size()>=3){
                        setenv(command[0][1].c_str(),command[0][2].c_str(),1);
                        for(int i=0;i<userEnv[id].env.size();i++){
                            if(userEnv[id].env[i].name==command[0][1]){
                                check=true;
                                userEnv[id].env[i].value=command[0][2];
                                break;
                            }
                        }
                        if(!check){
                            Env e;
                            e.name=command[0][1];
                            e.value=command[0][2];
                            userEnv[id].env.push_back(e);
                        }
                    }
                    write(fd,pa,strlen(pa));
                    userEnv[id].numpipes=numpipes;
                    continue;
                }

                int pipe0[2];
                int prePipeRd=0;
                if(pipeIn==true){
                    prePipeRd=numpipeIn.rw[0];
                }
                if(userPipefdInR!=-1){
                    prePipeRd=userPipefdInR;
                }

                int filefd;
                if(directToFile){
                    filefd=open(fileName.c_str(),O_CREAT|O_WRONLY|O_TRUNC,S_IRUSR|S_IWUSR);
                }
                for(int i=0;i<numOfCmd;i++){
                    if((numOfCmd-i)>1){
                        if(pipe(pipe0)<0) printf("Pipe Error\n");
                        //cout << pipe0[0] << " "<<pipe0[1]<<endl;
                    }
                    pid_t childpid;
                    while((childpid=fork())<0){
                        usleep(1000);
                    }
                    pidTable.push_back(childpid);
                    if(childpid==0){
                        close(msock);
                        close(0);
                        close(1);
                        close(2);
                        dup2(fd,0);
                        dup2(fd,1);
                        dup2(fd,2);
                        for(int k=0;k<30;k++){
                            if(userFdTable[k]!=-1)
                                close(userFdTable[k]);
                        }

                        if(((numOfCmd-i)==1)&&directToFile){
                            
                            //cout<<fd<<endl;
                            close(1);
                            dup2(filefd,1);
                            close(filefd);
                        }

                        if(prePipeRd>0){        //Pipe in
                            close(0);           //close stdin
                            dup2(prePipeRd,0);  //pre read to stdin
                            close(prePipeRd);   //close pre
                        }
                        if((numOfCmd-i)>1){     //Not last command
                            close(1);           //close stdout
                            dup2(pipe0[1],1);   //write to stdout
                            close(pipe0[0]);    //close read
                            close(pipe0[1]);    //close write
                        }
                        if((numOfCmd-i)==1){    //Last Command
                            if(numPipeType>-1){
                                close(1);
                                dup2(numpipeOut.rw[1],1);
                                if(numPipeType==1){
                                    close(2);
                                    dup2(numpipeOut.rw[1],2);
                                }
                                close(numpipeOut.rw[0]);
                                close(numpipeOut.rw[1]);
                            }
                            if(userPipefdOutW!=-1){
                                close(1);
                                dup2(userPipefdOutW,1);
                                close(2);
                                dup2(userPipefdOutW,2);
                                close(userPipefdOutW);
                                close(userPipefdOutR);
                            }  
                        }
                        execVec(command[i]);
                    }
                    else{                       //Main process
                        if(prePipeRd>0){        
                            close(prePipeRd);   //close pre read
                        }
                        if((numOfCmd-i)>1){
                            close(pipe0[1]);    //close write
                            prePipeRd=pipe0[0]; //read to next process
                        }
                        if(directToFile&&(numOfCmd-i)==1){
                            close(filefd);
                        }
                        if((numOfCmd-i)==1&&userPipefdOutW!=-1){
                            close(userPipefdOutW);
                        }
                    }
                }
                if(numPipeType==-1&&pipeToID==0){
                    for(int i=0;i<pidTable.size();i++){
                        int pid=pidTable[i];
                        int status;
                        waitpid(pid,&status,0);
                    }
                    pidTable.clear();
                }
                userEnv[id].numpipes=numpipes;
                write(fd,pa,strlen(pa));
            }
        }
    }

    return 0;
}

void printLoginMsg(int fd){
    write(fd,welcomeMsg,strlen(welcomeMsg));
}

void broadcastLogin(int id){
    User user=userList[id];
    string s="*** User '"+user.nickname+"' entered from "+user.ip_port+". ***\n";
    for(int i=0;i<30;i++){
        if(userFdTable[i]!=-1){
            write(userFdTable[i],s.c_str(),s.length());
        }
    }
}

int getIDfromFD(int fd){
    int id;
    for(id=0;id<30;id++){
        if(userFdTable[id]==fd){
            break;
        }
    }
    return id;
}

void who(int fd){
    char *s=(char*)malloc(COMMAND_BUFFER);
    int id=getIDfromFD(fd);
    sprintf(s,"<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
    //printf("%s",s);
    write(fd,s,strlen(s));
    for(int i=0;i<30;i++){
        if(userFdTable[i]!=-1){
            sprintf(s,"%d\t%s\t%s",userList[i].id+1,userList[i].nickname.c_str(),userList[i].ip_port.c_str());
            //printf("%s",s);
            write(fd,s,strlen(s));
            if(id==userList[i].id){
                sprintf(s,"\t<-me\n");
            }
            else{
                sprintf(s,"\n");
            }
            //printf("%s",s);
            write(fd,s,strlen(s));
        }
    }
}

void tell(int fd, int to, string msg){
    char s[10000];
    to--;
    if(userFdTable[to]==-1){
        sprintf(s,"*** Error: user #%d does not exist yet. ***\n",to+1);
        write(fd,s,strlen(s));
    }
    else{
        sprintf(s,"*** %s told you ***: %s\n",userList[getIDfromFD(fd)].nickname.c_str(),msg.c_str());
        write(userFdTable[to],s,strlen(s));
    }
}

void yell(int fd,string msg){
    char s[10000];
    sprintf(s,"*** %s yelled ***: %s\n",userList[getIDfromFD(fd)].nickname.c_str(),msg.c_str());
    for(int i=0;i<30;i++){
        if(userFdTable[i]!=-1){
            write(userFdTable[i],s,strlen(s));
        }
    }
}

void name(int fd,string name){
    bool check=true;
    char s[1000];
    for(int i=0;i<30;i++){
        if(userFdTable[i]!=-1&&userList[i].nickname==name){
            check=false;
            break;
        }
    }
    if(!check){
        sprintf(s,"*** User '%s' already exists. ***\n",name.c_str());
        write(fd,s,strlen(s));
    }
    else{
        userList[getIDfromFD(fd)].nickname=name;
        sprintf(s,"*** User from %s is named '%s'. ***\n",userList[getIDfromFD(fd)].ip_port.c_str(),name.c_str());
        for(int i=0;i<30;i++){
            if(userFdTable[i]!=-1){
                write(userFdTable[i],s,strlen(s));
            }
        }
    }
}

//Execute Command from Parsed Vector
int execVec(vector<string> line){
    vector<const char*> pointerVec;
    for(int i = 0; i < line.size(); i++){
        pointerVec.push_back(line[i].c_str());
    }
    pointerVec.push_back(NULL);
    const char** commands = &pointerVec[0];
    if(execvp(line[0].c_str(),(char* const*)commands)<0){
        char s[100];
        sprintf(s,"Unknown command: [%s].\n",line[0].c_str());
        write(2,s,strlen(s));
        exit(1);
    }
}

void childHandler(int signo){
    int status;
    while(waitpid(-1,&status,WNOHANG)>0){}
}

void clearUserPipe(int id){
    vector<UserPipe>::iterator it;
    for(it=userPipe.begin();it!=userPipe.end();){
        if(it->from==id||it->to==id){
            close(it->rw[0]);
            close(it->rw[1]);
            userPipe.erase(it);
        }
        else{
            it++;
        }
    }
}
