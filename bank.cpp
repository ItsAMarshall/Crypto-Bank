/**
	@file bank.cpp
	@brief Top level bank implementation file
 */
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include <string>
#include <iostream>
#include <map>

#include "account.h"

void* client_thread(void* arg);
void* console_thread(void* arg);
void setup_accounts();

std::map<std::string, Account>* accounts;

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		printf("Usage: bank listen-port\n");
		return -1;
	}

  setup_accounts();
	
	unsigned short ourport = atoi(argv[1]);
	
	//socket setup
	int lsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(!lsock)
	{
		printf("fail to create socket\n");
		return -1;
	}
	
	//listening address
	sockaddr_in addr_l;
	addr_l.sin_family = AF_INET;
	addr_l.sin_port = htons(ourport);
	unsigned char* ipaddr = reinterpret_cast<unsigned char*>(&addr_l.sin_addr);
	ipaddr[0] = 127;
	ipaddr[1] = 0;
	ipaddr[2] = 0;
	ipaddr[3] = 1;
	if(0 != bind(lsock, reinterpret_cast<sockaddr*>(&addr_l), sizeof(addr_l)))
	{
		printf("failed to bind socket\n");
		return -1;
	}
	if(0 != listen(lsock, SOMAXCONN))
	{
		printf("failed to listen on socket\n");
		return -1;
	}
	
	pthread_t cthread;
	pthread_create(&cthread, NULL, console_thread, NULL);
	
	//loop forever accepting new connections
	while(1)
	{
		sockaddr_in unused;
		socklen_t size = sizeof(unused);
		int csock = accept(lsock, reinterpret_cast<sockaddr*>(&unused), &size);
		if(csock < 0)	//bad client, skip it
			continue;
			
		pthread_t thread;
		pthread_create(&thread, NULL, client_thread, (void*)csock);
	}
}

void* client_thread(void* arg)
{
	int csock = (int)arg;
	
	printf("[bank] client ID #%d connected\n", csock);
	bool withdraw_, balance_, login_, transfer_;
  bool mid_login = false, validated = false;
	std::string user_, amount_, pin_;
  std::map<std::string, Account>::iterator active_account = accounts->end();
	//input loop
	int length;
	char packet[1024];
	while(1)
	{
		withdraw_ = false;
		balance_ = false;
		login_ = false;
		transfer_ = false;
		//read the packet from the ATM
		if(sizeof(int) != recv(csock, &length, sizeof(int), 0))
			break;
		if(length >= 1024)
		{
			printf("packet too long\n");
			break;
		}
		if(length != recv(csock, packet, length, 0))
		{
			printf("[bank] fail to read packet\n");
			break;
		}
		
		//convert packet to string to be parsed
		std::string str(packet);

		//determine the command type

    //if we're mid-login, this is the pin
    if (mid_login) {
      pin_ = str;
    }
		else if(str.find("withdraw ") == 0) {
			//takes in withdraw [amount]
			int sec_space = str.find(" ", 9);
			std::string temp = str.substr(9, sec_space);

			//printf("%s withdrawn.\n", temp.c_str());
			withdraw_ = true;
			// user_ = somewhere in packet
		}
		else if(str.find("login ") == 0) {
			//takes in login [username] [pin]
			//Get PIN from Bank
			//prompt for PIN
			int sec_space = str.find(" ", 6);
			user_ = str.substr(6, sec_space);

			printf("User %s is logging in.\n", user_.c_str());
      printf("Pin %s\n", pin_.c_str());

			login_ = true;
		}
		else if(str.find("balance") == 0) {
			printf("Checking Balance\n");
			balance_ = true;

		}
		else if(str.find("transfer ") == 0) {
			//takes in transfer [amount] [username]
			int space = str.find(" ", 9);
			std::string amount = str.substr(9, (space-9));
			printf("amount %s\n", amount.c_str());
			space = str.find(" ", 9+amount.size());
			std::string destination = str.substr(space+1);
			//printf("Transfering %s to %s.\n", amount.c_str(), destination.c_str());
			
			transfer_ = true;
			user_ = destination;
			amount_ = amount;
		}

		for(unsigned int i = 0; i < length; ++i) {
			packet[i] = '\0';
		}

    // -1 for non-existent account, 0 for continue
		if(login_) {
      active_account = accounts->find(user_);
      if (active_account == accounts->end())
      {
        std::cerr << "No such account\n";
        packet[0] = '-';
        packet[1] = '1';
      }
      else {
        packet[0] = '0';
        mid_login = true;
      }
		}
    // -1 for bad pin, 0 for success
    else if(mid_login) {
      mid_login = false;
      if (active_account->second.validate(pin_)) {
        validated = true;
        packet[0] = '0';
      }
      else {
        active_account = accounts->end();
        packet[0] = '-';
        packet[1] = '1';
      }
    }


		//send the new packet back to the client
		if(sizeof(int) != send(csock, &length, sizeof(int), 0))
		{
			printf("[bank] fail to send packet length\n");
			break;
		}
		if(length != send(csock, (void*)packet, length, 0))
		{
			printf("[bank] fail to send packet\n");
			break;
		}
		
	}

	printf("[bank] client ID #%d disconnected\n", csock);

	close(csock);
	return NULL;
}

void* console_thread(void* arg)
{
	char buf[80];
	while(1)
	{
		printf("bank> ");
		fgets(buf, 79, stdin);
		buf[strlen(buf)-1] = '\0';	//trim off trailing newline
		
    std::string input(buf);
    int first_space = input.find_first_of(' ');
    if (first_space == input.npos || first_space == input.length() - 1)
    {
      std::cerr << "Malformed command\n";
      continue;
    }
    std::string command = input.substr(0, first_space);

    //DEPOSIT
    if (command == "deposit")
    {
      int second_space = input.find_first_of(' ', first_space + 1);
      if (second_space == input.npos || second_space == input.length() - 1)
      {
        std::cerr << "Malformed command\n";
        continue;
      }
      std::string username = input.substr(first_space + 1, second_space - first_space - 1);

      std::string amount_string = input.substr(second_space + 1,
        input.length() - second_space);
      double amount = atof(amount_string.c_str());
      if (amount <= 0)
      {
        std::cerr << "Invlaid deposit amount\n";
        continue;
      }

      std::map<std::string, Account>::iterator it;
      it = accounts->find(username);
      if (it == accounts->end())
      {
        std::cerr << "No such account\n";
        continue;
      }
      it->second.deposit(amount);
    }

    //BALANCE
    else if (command == "balance")
    {
      std::string username = input.substr(first_space + 1, input.length() - first_space);
      
      std::map<std::string, Account>::iterator it;
      it = accounts->find(username);
      if (it == accounts->end())
      {
        std::cerr << "No such account\n";
        continue;
      }

      std::cout << it->second.get_balance() << std::endl;
    }
    else
    {
      std::cerr << "Unknown command\n";
    }
	}
}

void setup_accounts()
{
  accounts = new std::map<std::string, Account>();
  accounts->insert(std::make_pair("Alice", Account("Alice", "1234", 100)));
  accounts->insert(std::make_pair("Bob", Account("Bob", "7373", 50)));
  accounts->insert(std::make_pair("Eve", Account("Eve", "1337", 0)));
}
