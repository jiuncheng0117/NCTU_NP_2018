#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <signal.h>
#include "npshell.h"
#include "server3.h"


using namespace std;
const size_t COMMAND_BUFFER=15000;

char welcomemsg[]="****************************************\n\
** Welcome to the information server. **\n\
****************************************\n";

extern User *userList;
extern BroadcastMsg *msg;
extern int shmid1,shmid2;
extern int id;

void Broadcast(char *s);
void who();
void tell(int to,char*s);
void yell(char*s);
void name(char*s);
bool checkPipeExist(int from,int to);
void removeUserPipe(int id);

//Execute Command from Parsed Vector
int execVec(vector<string> line){
    vector<const char*> pointerVec;
    for(int i = 0; i < line.size(); i++){
        pointerVec.push_back(line[i].c_str());
    }
    pointerVec.push_back(NULL);
    const char** commands = &pointerVec[0];
    
    if(execvp(line[0].c_str(),(char* const*)commands)<0){
        fprintf(stderr,"Unknown command: [%s].\n",line[0].c_str());
        exit(1);
    }
}

void signalHandler(int signo){
    printf("%s",msg->str);
}


void childHandler(int signo){
    int status;
    while(waitpid(-1,&status,WNOHANG)>0){}
}

int npshell(){
    // shmid1=shmget(SHMKEY_UserList,sizeof(User)*30,0);
    // shmid2=shmget(SHMKEY_BroadcastMsg,sizeof(BroadcastMsg),0);
    // userList=(User*)shmat(shmid1,(char*)0,0);
    // msg=(BroadcastMsg*)shmat(shmid2,(char*)0,0);

    signal(SIGCHLD,childHandler);
    signal(SIGUSR1,signalHandler);
    //inital env PATH
    clearenv();
    printf("%s",welcomemsg);
    char s[100];
    sprintf(s,"*** User '%s' entered from %s. ***\n",userList[id].nickname,userList[id].ip_port);
    Broadcast(s);
    setenv("PATH","bin:.",1);
    vector <Numpipes> numpipes;
    do{
        char *input=(char*)malloc(COMMAND_BUFFER);
        char in[COMMAND_BUFFER];
        char inSave[COMMAND_BUFFER];
        vector<vector<string> > command;
        int numOfCmd;
        bool directToFile=false;
        int numPipeType=-1;     //-1:None   0:stdout    1:stdout+stderr
        int numPipeCount=0;
        int pipeToID=0;
        int pipeFromID=0;
        int userPipeIn=-1;
        int userPipeOut=-1;
        string fileName="";
        vector <int> pidTable;

        //Parsing-- 
        printf("%% ");
        if(!cin.getline(input,COMMAND_BUFFER)){
            exit(1);
        }

        //Handle \r\n for telnet
        char *current_pos = strchr(input,'\r');
        while (current_pos){
            *current_pos = '\0';
            current_pos = strchr(current_pos,'\r');
        }

        strcpy(in,input);
        strcpy(inSave,input);

        if(strlen(input)==0){
            free(input);
            continue;
        }

        if(strcmp(input,"exit")==0){
            char s[100];
            sprintf(s,"*** User '%s' left. ***\n",userList[id].nickname);
            Broadcast(s);

            userList[id].isOnline=false;
            //remove Pipe
            removeUserPipe(id);
            shmdt(msg);
            shmdt(userList);
            exit(0);
        }


        char *str;
        str = strtok(input," ");
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

        //  Check Numpipe
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
            if(!userList[pipeFromID-1].isOnline){
                printf("*** Error: user #%d does not exist yet. ***\n",pipeFromID);
                free(input);
                continue;
            }
        }
        
        if(pipeToID>0){
            if(!userList[pipeToID-1].isOnline){
                printf("*** Error: user #%d does not exist yet. ***\n",pipeToID);
                free(input);
                continue;
            }
        }
        
        if(pipeFromID>0){
            if(!checkPipeExist(pipeFromID,id+1)){
                printf("*** Error: the pipe #%d->#%d does not exist yet. ***\n",pipeFromID,id+1);
                free(input);
                continue;
            }
            else{
                char s1[20],s2[20];
                sprintf(s1,"user_pipe/%d_%d",pipeFromID,id+1);
                sprintf(s2,"user_pipe/_%d_%d",pipeFromID,id+1);
                rename(s1,s2);
                userPipeIn=open(s2,O_RDONLY);
                char s[100];
                sprintf(s,"*** %s (#%d) just received from %s (#%d) by '%s' ***\n",userList[id].nickname,id+1,userList[pipeFromID-1].nickname,pipeFromID,inSave);
                Broadcast(s);
                usleep(10);
            }
        }

        if(pipeToID>0){
            if(checkPipeExist(id+1,pipeToID)){
                printf("*** Error: the pipe #%d->#%d already exists. ***\n",id+1,pipeToID);
                free(input);
                continue;
            }
            else{
                char s1[20];
                sprintf(s1,"user_pipe/%d_%d",id+1,pipeToID);
                userPipeOut=open(s1,O_CREAT|O_WRONLY|O_TRUNC,S_IRUSR|S_IWUSR);;
                char s[100];
                sprintf(s,"*** %s (#%d) just piped '%s' to %s (#%d) ***\n",userList[id].nickname,id+1,inSave,userList[pipeToID-1].nickname,pipeToID);
                Broadcast(s);
            }
        }
        

        // Executing --

        if(command[0][0]=="who"){
            who();
            free(input);
            continue;
        }

        if(command[0][0]=="tell"){
            int to=atoi(command[0][1].c_str());
            char *s=strchr(in,' ');
            s=strchr(s+1,' ');
            s++;
            while(*s==' ')s++;
            tell(to,s);
            free(input);
            continue;
        }

        if(command[0][0]=="yell"){
            char *s=strchr(in,' ');
            s++;
            while(*s==' ')s++;
            yell(s);
            free(input);
            continue;
        }

        if(command[0][0]=="name"){
            char *s=strchr(in,' ');
            s++;
            while(*s==' ')s++;
            name(s);
            free(input);
            continue;
        }

        //handle printenv
        if(command[0][0]=="printenv"){
            char *str=getenv(command[0][1].c_str());
            if(str!=NULL){
                printf("%s\n",str);
            }
            free(input);
            continue;
        }
        
        //handle setenv
        if(command[0][0]=="setenv"){
            if(command[0].size()>=3)
                setenv(command[0][1].c_str(),command[0][2].c_str(),1);
            free(input);
            continue;
        }
        if(command[0][0]=="unsetenv"){
            if(command[0].size()>=2)
                unsetenv(command[0][1].c_str());
            free(input);
            continue;
        }

        //exec
        int pipe0[2];
        int prePipeRd=0;
        if(pipeIn==true){
            prePipeRd=numpipeIn.rw[0];
        }
        if(userPipeIn!=-1){
            prePipeRd=userPipeIn;
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
                if(((numOfCmd-i)==1)&&directToFile){
                    
                    //cout<<fd<<endl;
                    close(1);
                    dup2(filefd,1);
                    close(filefd);
                }

                if(prePipeRd>0){        //No pipe in
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
                    if(userPipeOut!=-1){
                        close(1);
                        dup2(userPipeOut,1);
                        close(2);
                        dup2(userPipeOut,2);
                        close(userPipeOut);
                    }  
                }
                execVec(command[i]);
            }
            else{
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
                if((numOfCmd-i)==1&&userPipeOut!=-1){
                    close(userPipeOut);
                }
            }

        }
        if(numPipeType==-1){
            for(int i=0;i<pidTable.size();i++){
                int pid=pidTable[i];
                int status;
                waitpid(pid,&status,0);
            }
            if(userPipeIn!=-1){
                char s[20];
                sprintf(s,"user_pipe/_%d_%d",pipeFromID,id+1);
                remove(s);
            }
        }
        free(input);
    }while(1); 
    return 0;
}


void Broadcast(char *s){
    strcpy(msg->str,s);
    for(int i=0;i<30;i++){
        if(userList[i].isOnline){
            kill(userList[i].pid,SIGUSR1);
        }
    }
}

void who(){
    printf("<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
    for(int i=0;i<30;i++){
        if(userList[i].isOnline){
            printf("%d\t%s\t%s",i+1,userList[i].nickname,userList[i].ip_port);
            if(id==i)
                printf("\t<-me");
            printf("\n");
        } 
    }
}

void tell(int to,char *m){
    char s[10000];
    to--;
    if(!userList[to].isOnline){
        printf("*** Error: user #%d does not exist yet. ***\n",to+1);
    }
    else{
        sprintf(s,"*** %s told you ***: %s\n",userList[id].nickname,m);
        strcpy(msg->str,s);
        kill(userList[to].pid,SIGUSR1);
    }
}

void yell(char *m){
    char s[10000];
    sprintf(s,"*** %s yelled ***: %s\n",userList[id].nickname,m);
    Broadcast(s);
}

void name(char *n){
    bool check=true;
    char s[1000];
    for(int i=0;i<30;i++){
        if(userList[i].isOnline&&(strcmp(userList[i].nickname,n)==0)){
            check=false;
            break;
        }
    }
    if(!check){
        printf("*** User '%s' already exists. ***\n",n);
    }
    else{
        strcpy(userList[id].nickname,n);
        sprintf(s,"*** User from %s is named '%s'. ***\n",userList[id].ip_port,n);
        Broadcast(s);
    }
}

bool checkPipeExist(int from,int to){
    char s[20];
    sprintf(s,"user_pipe/%d_%d",from,to);
    if(access(s,F_OK)!=-1)
        return true;
    return false;
}

void removeUserPipe(int id){
    char s[20];
    for(int i=0;i<30;i++){
        if(checkPipeExist(id+1,i+1)){
            sprintf(s,"user_pipe/%d_%d",id+1,i+1);
            remove(s);
        }
        if(checkPipeExist(i+1,id+1)){
            sprintf(s,"user_pipe/%d_%d",i+1,id+1);
            remove(s);
        }
    }
}
