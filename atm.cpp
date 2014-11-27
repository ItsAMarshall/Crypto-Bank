/**
	@file atm.cpp
	@brief Top level ATM implementation file
 */
<<<<<<< HEAD
#include <string>
#include <iostream>
#include <cassert>
=======
>>>>>>> FETCH_HEAD
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
<<<<<<< HEAD
#include <ctype.h>

using namespace std;

void withdraw_function() {
}

=======
>>>>>>> FETCH_HEAD

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		printf("Usage: atm proxy-port\n");
		return -1;
	}
	
	//socket setup
	unsigned short proxport = atoi(argv[1]);
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(!sock)
	{
		printf("fail to create socket\n");
		return -1;
	}
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(proxport);
	unsigned char* ipaddr = reinterpret_cast<unsigned char*>(&addr.sin_addr);
	ipaddr[0] = 127;
	ipaddr[1] = 0;
	ipaddr[2] = 0;
	ipaddr[3] = 1;
	if(0 != connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)))
	{
		printf("fail to connect to proxy\n");
		return -1;
	}
	
	//input loop
	char buf[80];
<<<<<<< HEAD
	string input;
	while(1)
	{
		printf("atm> ");
		//fgets(buf, 79, stdin);
		//buf[strlen(buf)-1] = '\0';	//trim off trailing newline
		getline(cin, input);
		//TODO: your input parsing code has to put data here
		char packet[1024];
		int length = 1;
		//char command[8];
		//strncpy(command, buf, 8);

		//input parsing
		printf("%s\n", input.c_str());
		if(input.find("logout") == 0)
			break;
		else if(input.find("withdraw ") == 0) {
			//takes in withdraw [amount]
			int sec_space = input.find(" ", 9);
			string temp = input.substr(9, sec_space);

			printf("%s withdrawn.\n", temp.c_str());
		}
		else if(input.find("login ") == 0) {
			//takes in login [username]
			//prompt for PIN
			int sec_space = input.find(" ", 6);
			string temp = input.substr(6, sec_space);

			printf("User %s is logging in.\n", temp.c_str());
		}
		else if(input.find("balance") == 0) {
			printf("Checking Balance\n");
		}
		else if(input.find("transfer ") == 0) {
			//takes in transfer [amount] [username]
			int space = input.find(" ", 9);
			string amount = input.substr(9, (space-9));
			printf("amount %s\n", amount.c_str());
			space = input.find(" ", 9+amount.size());
			string destination = input.substr(space+1);
			printf("Transfering %s to %s.\n", amount.c_str(), destination.c_str());
		}
		else {
			printf("invalid input.  Valid operations are:\n \
login | withdraw | balance | transfer | logout\n");
		}


=======
	while(1)
	{
		printf("atm> ");
		fgets(buf, 79, stdin);
		buf[strlen(buf)-1] = '\0';	//trim off trailing newline
		
		//TODO: your input parsing code has to put data here
		char packet[1024];
		int length = 1;
		
		//input parsing
		if(!strcmp(buf, "logout"))
			break;
>>>>>>> FETCH_HEAD
		//TODO: other commands
		
		//send the packet through the proxy to the bank
		if(sizeof(int) != send(sock, &length, sizeof(int), 0))
		{
			printf("fail to send packet length\n");
			break;
		}
		if(length != send(sock, (void*)packet, length, 0))
		{
			printf("fail to send packet\n");
			break;
		}
		
		//TODO: do something with response packet
		if(sizeof(int) != recv(sock, &length, sizeof(int), 0))
		{
			printf("fail to read packet length\n");
			break;
		}
		if(length >= 1024)
		{
			printf("packet too long\n");
			break;
		}
		if(length != recv(sock, packet, length, 0))
		{
			printf("fail to read packet\n");
			break;
		}
	}
	
	//cleanup
	close(sock);
	return 0;
}
