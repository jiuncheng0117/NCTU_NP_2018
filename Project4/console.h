#ifndef CONSOLE_H
#define CONSOLE_H
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;
struct Hosts{
    string host,port,file;
};

void parser(string);
void printBody();
void encode(string&);
void replaceAll(string&,const string&,const string&);
void outputShell(string,string);
void outputCommand(string,string);
void start(Hosts);

void write_handler(const boost::system::error_code &ec,size_t bytes_transferred);
void read_handler(const boost::system::error_code &ec,size_t bytes_transferred);
void connect_handler(const boost::system::error_code &ec);
void resolve_handler(const boost::system::error_code &ec,tcp::resolver::iterator it);

void socks_connect_handler(const boost::system::error_code &ec);
void socks_resolve_handler(const boost::system::error_code &ec,tcp::resolver::iterator it);
void socks_reply_handler(const boost::system::error_code &ec,size_t bytes_transferred);

char htmlHeader[]="\
<!DOCTYPE html>\n\
<html lang=\"en\">\n\
<head>\n\
<meta charset=\"UTF-8\" />\n\
<title>NP Project 3 Console</title>\n\
<link\n\
rel=\"stylesheet\"\n\
href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"\n\
integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"\n\
crossorigin=\"anonymous\"\n\
/>\n\
<link\n\
href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n\
rel=\"stylesheet\"\n\
/>\n\
<link\n\
rel=\"icon\"\n\
type=\"image/png\"\n\
href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n\
/>\n\
<style>\n\
* {\n\
font-family: 'Source Code Pro', monospace;\n\
font-size: 1rem !important;\n\
}\n\
body {\n\
background-color: #212529;\n\
}\n\
pre {\n\
color: #cccccc;\n\
}\n\
b {\n\
color: #ffffff;\n\
}\n\
</style>\n\
</head>";

#endif