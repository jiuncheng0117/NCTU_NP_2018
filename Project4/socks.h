#ifndef SOCKS_H
#define SOCKS_H

#include <netinet/in.h>
#include <stdlib.h>
#include <string>
using namespace std;



struct Request{
	uint8_t vn;
	uint8_t cd;
	uint16_t dest_port;
	in_addr	dest_ip;
	char user[100];
};

struct Reply{
	uint8_t vn;
	uint8_t cd;
	uint16_t dest_port;
	in_addr	dest_ip;
};

int socks(sockaddr_in);
void printSocks();
void createConnect();
void createBind();
bool firewall();

#endif