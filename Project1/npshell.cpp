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
#include <signal.h>

using namespace std;
const size_t COMMAND_BUFFER=15000;

struct Pipes{
   int rw[2];   // 0 = read, 1 = write
};
struct Numpipes{
    int rw[2];
    int count;
};

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


void childHandler(int signo){
    int status;
    while(waitpid(-1,&status,WNOHANG)>0){}
}

int main(){
    //inital env PATH
    setenv("PATH","bin:.",1);
    vector <Numpipes> numpipes;
    do{
        char *input=(char*)malloc(COMMAND_BUFFER);
        vector<vector<string> > command;
        int numOfCmd;
        bool directToFile=false;
        int numPipeType=-1;     //-1:None   0:stdout    1:stdout+stderr
        int numPipeCount=0;
        string fileName="";
        vector <int> pidTable;
        signal(SIGCHLD,childHandler);

        //Parsing-- 
        printf("%% ");
        if(!cin.getline(input,COMMAND_BUFFER)){
            exit(1);
        }
        if(strlen(input)==0){
            free(input);
            continue;
        }
        if(strcmp(input,"exit")==0){
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
                directToFile=true;
                str = strtok (NULL, " ");
                fileName=string(str);
                break;
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
        
        

        // Executing --

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
                if((numOfCmd-i)==1&& numPipeType>-1){    //Last Command
                    close(1);
                    dup2(numpipeOut.rw[1],1);
                    if(numPipeType==1){
                        close(2);
                        dup2(numpipeOut.rw[1],2);
                    }
                    close(numpipeOut.rw[0]);
                    close(numpipeOut.rw[1]);
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
                // if((numOfCmd-i)==1&& numPipeType>-1){
                //     close(numpipeOut.rw[1]);
                // }
                
                //wait(0);
            }

        }
        if(numPipeType==-1){
            for(int i=0;i<pidTable.size();i++){
                int pid=pidTable[i];
                int status;
                waitpid(pid,&status,0);
            }
        }

        
        free(input);
    }while(1); 
    return 0;
}
