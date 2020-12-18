#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <array>
#include <arpa/inet.h>
#include "console.h"
using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

Hosts hosts[5];
string sh,sp;
bool useProxy=false;
int hostCount=0;
vector<int> pidTable;
io_service ioservice;
tcp::resolver resolv{ioservice};
tcp::socket tcp_socket{ioservice};
Hosts host;

ifstream ifs;
array<char,25000> bytes;
string session;
//Request req;
//Reply reply;
uint8_t req[8],reply[8];

int main(int, char* const[], char* const envp[]){
    cout << "Content-type: text/html" << endl << endl;
    cout << htmlHeader <<endl; 
    string s=string(getenv("QUERY_STRING"));
    parser(s);
    printBody();
    session="s ";
    for(int i=0;i<hostCount;i++){
        ioservice.notify_fork(boost::asio::io_service::fork_prepare);
        int childpid=fork();
        if(childpid==0){
            ioservice.notify_fork(boost::asio::io_service::fork_child);
            session[1]='0'+i;
            start(hosts[i]);
            exit(1);
        }
        else{
            ioservice.notify_fork(boost::asio::io_service::fork_parent);
            pidTable.push_back(childpid);
            usleep(100000);
        }
    }
    for(int i=0;i<pidTable.size();i++){
        int pid=pidTable[i];
        int status;
        waitpid(pid,&status,0);
    }
}

void parser(string s){
    regex reg("\\w+=[\\w.]*");
    smatch m;
    while(regex_search(s, m, reg)){
        //cout<<m.str(0)<<endl;
        string parse=m.str(0);
        int i=parse[1]-'0';
        switch(parse[0]){
            case 'h':
                hosts[i].host=parse.substr(3);
                if(hosts[i].host.length()>0)
                    hostCount++;
                break;
            case 'p':
                hosts[i].port=parse.substr(3);
                break;
            case 'f':
                hosts[i].file=parse.substr(3);
                break;
            default:
                if(parse[1]=='h'&&(parse.length()>3)){
                    useProxy=true;
                    sh=parse.substr(3);
                    break;
                }
                if(parse[1]=='p'){
                    sp=parse.substr(3);
                    break;
                }
        }
        s = m.suffix().str();
    }
}

void printBody(){
    cout<<"<body>"<<endl;
    cout<<"<table class=\"table table-dark table-bordered\">"<<endl;
    cout<<"<thead>"<<endl;
    cout<<"<tr>"<<endl;
    for(int i=0;i<hostCount;i++){
        cout<<"<th scope=\"col\">"<<hosts[i].host<<":"<<hosts[i].port<<"</th>"<<endl;
    }
    cout<<"</tr>"<<endl;
    cout<<"</thead>"<<endl;
    cout<<"<tbody>"<<endl;
    cout<<"<tr>"<<endl;
    for(int i=0;i<hostCount;i++){
        cout<<"<td><pre id=\"s"<<i<<"\" class=\"mb-0\"></pre></td>"<<endl;
    }
    cout<<"</tr>"<<endl;
    cout<<"</tbody>"<<endl;
    cout<<"</table>"<<endl;
    cout<<"</body>"<<endl;
}

void outputShell(string session,string content){
    encode(content);
    replaceAll(content,"\n","&NewLine;");
    cout<<"<script>document.getElementById('"<<session<<"').innerHTML += '"<<content<<"';</script>"<<endl;
    cout.flush();
}

void outputCommand(string session,string content){
    encode(content);
    replaceAll(content,"\n","&NewLine;");
    cout<<"<script>document.getElementById('"<<session<<"').innerHTML += '<b>"<<content<<"</b>';</script>"<<endl;
    cout.flush();
}

void encode(string& data) {
    std::string buffer;
    buffer.reserve(data.size());
    for(size_t pos = 0; pos != data.size(); ++pos) {
        switch(data[pos]) {
            case '&':  buffer.append("&amp;");       break;
            case '\"': buffer.append("&quot;");      break;
            case '\'': buffer.append("&apos;");      break;
            case '<':  buffer.append("&lt;");        break;
            case '>':  buffer.append("&gt;");        break;
            case '\r': break;
            default:   buffer.append(&data[pos], 1); break;
        }
    }
    data.swap(buffer);
}

void replaceAll(string &str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

void start(Hosts _host){
    host=_host;
    string filename="test_case/"+host.file;
    ifs.open(filename);
    string str;
    if(!useProxy){
        tcp::resolver::query q{host.host,host.port};
        resolv.async_resolve(q,resolve_handler);
    }
    else{
        uint32_t ip;
        struct hostent        *he;
        if ( (he = gethostbyname(host.host.c_str()) ) == NULL ) {
            exit(1);
        }
        memcpy(&ip, he->h_addr_list[0], he->h_length);
        //cout<<ip<<endl;
    
        char s[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip, s, INET_ADDRSTRLEN);
        //cout<<s<<endl;
        unsigned short port=atoi(host.port.c_str());
        req[0]=4;
        req[1]=1;
        req[2]=port>>8;
        req[3]=port&0xFF;
        //cout<<req[2]<<" "<<req[3]<<endl;
        req[7]=ip>>24;
        req[6]=(ip>>16)&0xFF;
        req[5]=(ip>>8)&0xFF;
        req[4]=ip&0xFF;
        tcp::resolver::query q{sh,sp};
        resolv.async_resolve(q,socks_resolve_handler);
    }
    
    ioservice.run();
}

void write_handler(const boost::system::error_code &ec,size_t bytes_transferred){
    if(!ec){
        tcp_socket.async_read_some(buffer(bytes),read_handler);
    }
    else{
        //cout<<"write:"<<ec.message()<<"<br>"<<endl;
    }
}

void read_handler(const boost::system::error_code &ec,size_t bytes_transferred){
    if(!ec){
        string s=string(bytes.data(),bytes_transferred);
        outputShell(session,s);
        if(s.find("% ")!=string::npos){
            string str;
            getline(ifs, str);
            if(str.length()==0)
                return;
            if(str.find('\r')!=string::npos){
                str.erase(str.find('\r'),1);
            }
            str+="\n";
            outputCommand(session,str);
            tcp_socket.async_write_some(buffer(str),write_handler);
        }
        else
            tcp_socket.async_read_some(buffer(bytes),read_handler);
    }
    else{
        //cout<<"read:"<<ec.message()<<"<br>"<<endl;
    }
}

void connect_handler(const boost::system::error_code &ec){
    if(!ec){
        tcp_socket.async_read_some(buffer(bytes),read_handler);
    }
    else{
        //cout<<"connect:"<<ec.message()<<"<br>"<<endl;
    }
}

void resolve_handler(const boost::system::error_code &ec,tcp::resolver::iterator it){
    if(!ec)
        tcp_socket.async_connect(*it,connect_handler);
    else{
        //cout<<"resolve:"<<ec.message()<<"<br>"<<endl;
    }
}

void socks_reply_handler(const boost::system::error_code &ec,size_t bytes_transferred){
    if(!ec){
        if(reply[1]==90)
          tcp_socket.async_read_some(buffer(bytes),read_handler);
    }
}

void socks_connect_handler(const boost::system::error_code &ec){
    if(!ec){
        tcp_socket.write_some(buffer(req));
        tcp_socket.async_read_some(buffer(reply),socks_reply_handler);
    }
    else{
        //cout<<"connect:"<<ec.message()<<"<br>"<<endl;
    }
}

void socks_resolve_handler(const boost::system::error_code &ec,tcp::resolver::iterator it){
    if(!ec)
        tcp_socket.async_connect(*it,socks_connect_handler);
    else{
        //cout<<"resolve:"<<ec.message()<<"<br>"<<endl;
    }
}
