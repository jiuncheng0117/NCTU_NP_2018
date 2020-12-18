#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <stdlib.h>
#include <iostream>
#include <memory>
#include <utility>


using namespace std;
using namespace boost::asio;
extern char **environ;
io_service global_io_service;

class HttpSession : public enable_shared_from_this<HttpSession> {
 private:
  enum { max_length = 4096 };
  ip::tcp::socket _socket;
  array<char, max_length> _data;
  map<string,string> env;
  string filename;

 public:
  HttpSession(ip::tcp::socket socket) : _socket(move(socket)) {}

  void start() { do_read(); }

 private:
 
  void do_read() {
    auto self(shared_from_this());
    _socket.async_read_some(
        buffer(_data, max_length),
        [this, self](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
              string s=_data.data();
              istringstream ss(s);
              string str;
              getline(ss,str,' ');
              env.insert(make_pair("REQUEST_METHOD",str));
              getline(ss,str,' ');
              env.insert(make_pair("REQUEST_URI",str));
              string qs="";
            
              if(str.find('?')!=string::npos){
                qs=str.substr(str.find('?')+1);
                filename=str.substr(1,str.find('?')-1);
              }
              else
                filename=str.substr(1);
              env.insert(make_pair("QUERY_STRING",qs));
              getline(ss,str,' ');
              string sub=str.substr(0,str.find('\r'));
              env.insert(make_pair("SERVER_PROTOCOL",sub));
              getline(ss,str,' ');
              sub=str.substr(0,str.find('\r'));
              env.insert(make_pair("HTTP_HOST",sub));
              str=_socket.remote_endpoint().address().to_string();
              env.insert(make_pair("REMOTE_ADDR",str));
              char port[6];
              sprintf(port,"%hu",_socket.remote_endpoint().port());
              env.insert(make_pair("REMOTE_PORT",string(port)));
              str=_socket.local_endpoint().address().to_string();
              env.insert(make_pair("SERVER_ADDR",str));
              sprintf(port,"%hu",_socket.local_endpoint().port());
              env.insert(make_pair("SERVER_PORT",string(port)));

              
              cout<<filename<<endl;
              if(access(filename.c_str(),F_OK)!=-1){
                string okMsg="HTTP/1.1 200 OK\r\n";
                _socket.async_write_some(buffer(okMsg),[this,self](boost::system::error_code ec, std::size_t length){
                  if(!ec){
                    global_io_service.notify_fork(boost::asio::io_service::fork_prepare);
                    int pid=fork();
                    if(pid<0)
                        cout<<"fork error"<<endl;
                    if(pid==0){
                      global_io_service.notify_fork(boost::asio::io_service::fork_child);
                      int fd=_socket.native_handle();
                      close(0);
                      close(1);
                      close(2);
                      dup2(fd,0);
                      dup2(fd,1);
                      dup2(fd,2);
                      //close(fd);
                      environ=NULL;
                      for(auto it=env.begin();it!=env.end();it++){
                        setenv(it->first.c_str(),it->second.c_str(),1);
                      }
                      //setenv("PATH",".",1);
                      if(execl(filename.c_str(),filename.c_str(),NULL)<0)
                          cerr<<"EXEC ERROR"<<endl;
                    }
                    else{
                        //close(fd);
                        global_io_service.notify_fork(boost::asio::io_service::fork_parent);
                        //_socket.shutdown(ip::tcp::socket::shutdown_receive);
                        close(_socket.native_handle());
                    }
                  }
                });
              }
              else{
                  string errorMsg="HTTP/1.1 404 Not Found\r\n";
                  _socket.async_write_some(buffer(errorMsg),[this,self](boost::system::error_code ec, std::size_t length){
                  if(!ec){
                      _socket.shutdown(ip::tcp::socket::shutdown_both);
                  }
                 });
              }
          }
        });
  }
};

class HttpServer {
 private:
  ip::tcp::acceptor _acceptor;
  ip::tcp::socket _socket;

 public:
  HttpServer(unsigned short port)
      : _acceptor(global_io_service, ip::tcp::endpoint(ip::tcp::v4(), port)),
        _socket(global_io_service) {
    do_accept();
  }

 private:
  void do_accept() {
    _acceptor.async_accept(_socket, [this](boost::system::error_code ec) {
      if (!ec) make_shared<HttpSession>(move(_socket))->start();
      do_accept();
    });
  }
};

int main(int argc, char* const argv[]) {
  if (argc != 2) {
    std::cerr << "Usage:" << argv[0] << " [port]" << endl;
    return 1;
  }

  try {
    short port = atoi(argv[1]);
    HttpServer server(port);
    global_io_service.run();
  } catch (exception& e) {
    cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
