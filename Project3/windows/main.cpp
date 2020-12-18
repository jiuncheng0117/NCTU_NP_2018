#include <array>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <stdlib.h>
#include <iostream>
#include <memory>
#include <utility>
#include <regex>
#include <string>
#include <fstream>
#include <sys/utime.h>


using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

struct Hosts {
	string host, port, file;
};

string consoleHeader = "\
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


io_service global_io_service;


class ConsoleSession : public enable_shared_from_this<ConsoleSession> {
private:
	enum { max_length = 4096 };
	//ip::tcp::socket &_socket;
	array<char, max_length> _data;
	string id;
	string payload, payload2;
	string cmdStr;
	Hosts host;
	ifstream ifs;
	ip::tcp::resolver resolv;
	ip::tcp::socket tcp_socket;
	shared_ptr<ip::tcp::socket> socket_ptr;
	array<char, 25000> bytes;
public:
	ConsoleSession(shared_ptr<ip::tcp::socket> _socket_ptr,string _id,Hosts _hosts):socket_ptr(_socket_ptr),id(_id),host(_hosts),resolv(global_io_service),tcp_socket(global_io_service){}
	void start() {
		init();
	};
private:
	void outputShell(string content) {
		auto self(shared_from_this());
		encode(content);
		replaceAll(content, "\n", "&NewLine;");
		stringstream ss;
		ss << "<script>document.getElementById('" << id << "').innerHTML += '" << content << "';</script>" << endl;
		payload = ss.str();
		socket_ptr->async_write_some(buffer(payload),[this, self](boost::system::error_code ec, std::size_t length) {
			if (!ec);
		});
	}

	void outputCommand(string content) {
		auto self(shared_from_this());
		encode(content);
		replaceAll(content, "\n", "&NewLine;");
		stringstream ss;
		ss << "<script>document.getElementById('" << id << "').innerHTML += '<b>" << content << "</b>';</script>" << endl;
		payload2 = ss.str();
		socket_ptr->async_write_some(buffer(payload2), [this, self](boost::system::error_code ec, std::size_t length) {
			if (!ec);
		});
	}

	void encode(string& data) {
		std::string buffer;
		buffer.reserve(data.size());
		for (size_t pos = 0; pos != data.size(); ++pos) {
			switch (data[pos]) {
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
		while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
			str.replace(start_pos, from.length(), to);
			start_pos += to.length();
		}
	}

	void init() {
		auto self(shared_from_this());
		string filename = "test_case/" + host.file;
		ifs.open(filename);
		string str;
		//getline(ifs, str);
		//cout << str << endl;
		ip::tcp::resolver::query q{ host.host,host.port };
		resolv.async_resolve(q, [this, self](const boost::system::error_code &ec, ip::tcp::resolver::iterator it) {
			if (!ec) {
				//cout << id << "Resolve" << endl;
				tcp_socket.async_connect(*it, [this, self](const boost::system::error_code &ec) {
					if (!ec) {
						//cout << id << "Connect" << endl;
						doRead();
					}
					else {
						cout << ec.message() << endl;
					}
				});
			}
			else
				cout << ec.message() << endl;
		});
	}

	void doRead() {
		auto self = shared_from_this();
		tcp_socket.async_read_some(buffer(bytes), [this, self](const boost::system::error_code &ec, size_t bytes_transferred) {
			if (!ec) {
				string s = string(bytes.data(), bytes_transferred);
				//cerr << s << endl;
				outputShell(s);
				if (s.find("% ") != string::npos) {
					string str;
					getline(ifs, str);
					if (str.length() == 0)
						return;
					if (str.find('\r') != string::npos) {
						str.erase(str.find('\r'), 1);
					}
					str += "\n";
					//cerr << str << endl;
					cmdStr = str;
					outputCommand(str);
					doWrite();
				}
				else
					doRead();
			}
		});
	}
	
	void doWrite() {
		auto self = shared_from_this();
		tcp_socket.async_write_some(buffer(cmdStr), [this, self](const boost::system::error_code &ec, size_t bytes_transferred) {
			if (!ec)
				doRead();
		});
	}
};

class HttpSession : public enable_shared_from_this<HttpSession> {
private:
	enum { max_length = 4096 };
	//ip::tcp::socket _socket;
	shared_ptr<ip::tcp::socket> socket_ptr;
	array<char, max_length> _data;
	string filename;
	string qs;
	string payload;
	Hosts hosts[5];
	int hostCount;
public:
	HttpSession(shared_ptr<ip::tcp::socket> _socket_ptr) : socket_ptr(_socket_ptr){}

	void start() { do_read(); }

private:
	void parseQS() {
		hostCount = 0;
		regex reg("\\w+=[\\w.]*");
		smatch m;
		while (regex_search(qs, m, reg)) {
			string parse = m.str(0);
			int i = parse[1] - '0';
			switch (parse[0]) {
			case 'h':
				hosts[i].host = parse.substr(3);
				if (hosts[i].host.length() > 0)
					hostCount++;
				break;
			case 'p':
				hosts[i].port = parse.substr(3);
				break;
			case 'f':
				hosts[i].file = parse.substr(3);
				break;
			}
			qs = m.suffix().str();
		}
	}

	void generateHTML() {
		string httpHeader = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
		stringstream ss;
		ss << "<body>" << endl;
		ss << "<table class=\"table table-dark table-bordered\">" << endl;
		ss << "<thead>" << endl;
		ss << "<tr>" << endl;
		for (int i = 0; i < hostCount; i++) {
			ss << "<th scope=\"col\">" << hosts[i].host << ":" << hosts[i].port << "</th>" << endl;
		}
		ss << "</tr>" << endl;
		ss << "</thead>" << endl;
		ss << "<tbody>" << endl;
		ss << "<tr>" << endl;
		for (int i = 0; i < hostCount; i++) {
			ss << "<td><pre id=\"s" << i << "\" class=\"mb-0\"></pre></td>" << endl;
		}
		ss << "</tr>" << endl;
		ss << "</tbody>" << endl;
		ss << "</table>" << endl;
		ss << "</body>" << endl;

		payload = httpHeader + consoleHeader + ss.str();
		//cout << payload << endl;
	}

	void do_read() {
		auto self(shared_from_this());
		socket_ptr->async_read_some(
			buffer(_data, max_length),
			[this, self](boost::system::error_code ec, std::size_t length) {
			if (!ec) {
				//string error = "HTTP/1.1 404 Not Found\r\n";
				//_socket.send(buffer(error));
				string s = _data.data();
				istringstream ss(s);
				string str;
				getline(ss, str, ' ');	//GET
				getline(ss, str, ' ');	//URI
				string URI = str;
				qs = "";
				if (str.find('?') != string::npos) {
					qs = str.substr(str.find('?') + 1);
					filename = str.substr(1, str.find('?') - 1);
				}
				else
					filename = str.substr(1);
				cout << "URI:" << URI << endl;
				//cout << "filename:" << filename << endl;
				//cout << "qs:" << qs << endl;
				if (filename == "panel.cgi") {
					string httpHeader = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
					string file[10] = { "t1.txt","t2.txt","t3.txt","t4.txt","t5.txt","t6.txt","t7.txt","t8.txt","t9.txt" ,"t10.txt" };
					string test_case_menu = "";
					for (int i = 0; i < 10; i++) {
						test_case_menu += "<option value=\"" + file[i] + "\">" + file[i] + "</option>";
					}
					string server[10];
					string domain = ".cs.nctu.edu.tw";
					string serverString = "";
					for (int i = 0; i < 5; i++) {
						stringstream ss;
						ss << (i + 1);
						string _i = ss.str();
						server[i] = "nplinux" + _i;
						server[i + 5] = "npbsd" + _i;
					}
					for (int i = 0; i < 10; i++) {
						serverString += "<option value=\"" + server[i] + domain + "\">" + server[i] + "</option>";
					}
					string html_header = "<!DOCTYPE html>\n"
						"<html lang = \"en\">"
						"<head>"
						"<title>NP Project 3 Panel</title>"
						"<link rel = \"stylesheet\" href = \"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\""
						"integrity = \"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\" crossorigin = \"anonymous\" />"
						"<link href = \"https://fonts.googleapis.com/css?family=Source+Code+Pro\" rel = \"stylesheet\"/>"
						"<link rel = \"icon\" type = \"image/png\" href = \"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"/>"
						"<style>"
						"* {"
						"font - family: 'Source Code Pro', monospace;"
						"}"
						"</style>"
						"</head>"
						"<body class = \"bg-secondary pt-5\">"
						"<form action = \"console.cgi\" method = \"GET\">"
						"<table class = \"table mx-auto bg-light\" style = \"width: inherit\">"
						"<thead class = \"thead-dark\">"
						"<tr>"
						"<th scope = \"col\">#</th>"
						"<th scope = \"col\">Host</th>"
						"<th scope = \"col\">Port</th>"
						"<th scope = \"col\">Input File</th>"
						"</tr>	</thead>"
						"<tbody>";
					for (int i = 0; i < 5; i++) {
						stringstream ss;
						ss << i;
						string _i = ss.str();
						stringstream ss2;
						ss2 << (i + 1);
						string _ip1 = ss2.str();
						string s1 = "<tr>"
							"<th scope = \"row\" class = \"align-middle\">Session " + _ip1 + "</th>"
							"<td>"
							"<div class = \"input-group\">"
							"<select name = \"h" + _i + "\" class = \"custom-select\">"
							"<option></option>" + serverString +
							"</select>"
							"<div class = \"input-group-append\">"
							"<span class = \"input-group-text\">.cs.nctu.edu.tw</span>"
							"</div> </div> </td>"
							"<td>"
							"<input name = \"p" + _i + "\" type = \"text\" class = \"form-control\" size = \"5\" / >"
							"</td>"
							"<td>"
							"<select name = \"f" + _i + "\" class = \"custom-select\">"
							"<option></option>" + test_case_menu + "</select> </td> </tr>";
						html_header += s1;
					}
					html_header += "<tr>"
						"<td colspan = \"3\"></td>"
						"<td> <button type = \"submit\" class = \"btn btn-info btn-block\">Run</button>	</td>"
						"</tr> </tbody> </table> </form> </body> </html>";
					payload = httpHeader + html_header;
					socket_ptr->async_write_some(buffer(payload), [this, self](boost::system::error_code ec, std::size_t length) {
						if (!ec)
							socket_ptr->shutdown(ip::tcp::socket::shutdown_both);
					});
				}
				if (filename == "console.cgi") {
					//string error = "HTTP/1.1 404 Not Found\r\n";
					//_socket.send(buffer(error));
					//cout << qs << endl;
					parseQS();
					generateHTML();
					socket_ptr->async_write_some(buffer(payload), [this,self](boost::system::error_code ec, std::size_t length) {
						if (!ec) {
							for (int i = 0; i < hostCount; i++) {
								string session = "s ";
								session[1] = '0' + i;
								make_shared<ConsoleSession>(socket_ptr,session, hosts[i])->start();
								Sleep(100);
							}
							//socket_ptr = nullptr;
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
	//ip::tcp::socket _socket;
	shared_ptr<ip::tcp::socket> socket_ptr;

public:
	HttpServer(unsigned short port)
		: _acceptor(global_io_service, ip::tcp::endpoint(ip::tcp::v4(), port)),
		socket_ptr(make_shared<ip::tcp::socket>(global_io_service)) {
		do_accept();
	}

private:
	void do_accept() {
		_acceptor.async_accept(*socket_ptr, [this](boost::system::error_code ec) {
			if (!ec) make_shared<HttpSession>(socket_ptr)->start();
			socket_ptr = make_shared< ip::tcp::socket >(global_io_service);
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
	}
	catch (exception& e) {
		cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}
