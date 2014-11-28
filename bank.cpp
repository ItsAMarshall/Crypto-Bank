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
#include <sstream>

#include <cryptopp/rsa.h>
#include <cryptopp/osrng.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>

#include "account.h"

using namespace CryptoPP;

void* client_thread(void* arg);
void* console_thread(void* arg);
void setup_accounts();

std::map<std::string, Account>* accounts;
RSA::PrivateKey* rsa_private;
lword public_key_size;
byte* public_key_buff;
AutoSeededRandomPool* pool;

int main(int argc, char* argv[])
{
  if(argc != 2)
  {
    printf("Usage: bank listen-port\n");
    return -1;
  }

  // Set up pre-existing accounts
  setup_accounts();

  // Set up encryption
  printf("Generating RSA keys...\n");
  pool = new AutoSeededRandomPool();
  InvertibleRSAFunction rsa_params;
  rsa_params.GenerateRandomWithKeySize(*pool, 4096);
  rsa_private = new RSA::PrivateKey(rsa_params);
  RSA::PublicKey rsa_public(rsa_params);

  ByteQueue queue;
  rsa_public.Save(queue);
  public_key_size = queue.TotalBytesRetrievable();
  public_key_buff = new byte[public_key_size];
  queue.Get(public_key_buff, public_key_size);
  printf("Key generation complete.\n");

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

  //ENCRYPTION SETUP
  //Send client the RSA public key
  //Format: [8 bytes: key length] [~500 bytes: public key]
  byte* prepacket = new byte[1024];
  memcpy(prepacket, &public_key_size, 8);
  memcpy(prepacket + 8, public_key_buff, public_key_size);
  if (send(csock, prepacket, 1024, 0) != 1024) {
    printf("[bank] Failed to send public key\n");
    return 0;
  }

  //Receive the AES private key and IV
  //Format: (RSA encrypted) [32 bytes: AES key] [16 bytes: IV]
  int rsa_ct_size = recv(csock, prepacket, 1024, 0);
  if (rsa_ct_size <= 0) {
    printf("[bank] Failed to receive AES creds\n");
    return 0;
  }
  RSAES_OAEP_SHA_Decryptor rsa_decryptor(*rsa_private);
  int rsa_pt_size = rsa_decryptor.MaxPlaintextLength(rsa_ct_size);
  SecByteBlock rsa_pt(rsa_pt_size);
  DecodingResult rsa_decode_result = 
      rsa_decryptor.Decrypt(*pool, prepacket, rsa_ct_size, rsa_pt);
  if (!rsa_decode_result.isValidCoding) {
    printf("[bank] Failed to decrypt RSA message\n");
    return 0;
  }
  int aes_key_size = 32;
  SecByteBlock aes_key(0x00, aes_key_size);
  memcpy(aes_key.BytePtr(), rsa_pt.BytePtr(), aes_key_size);
  byte aes_iv[AES::BLOCKSIZE];
  memcpy(aes_iv, rsa_pt.BytePtr() + aes_key_size, AES::BLOCKSIZE);

  // Verify AES functionality
  CFB_Mode<AES>::Encryption aes_encryption(aes_key, aes_key_size, aes_iv);
  CFB_Mode<AES>::Decryption aes_decryption(aes_key, aes_key_size, aes_iv);
  byte msg_ping[5] = {'P', 'I', 'N', 'G', '\0'};
  byte msg_pong[5] = {'P', 'O', 'N', 'G', '\0'};
  aes_encryption.ProcessData(msg_ping, msg_ping, 5);
  if (send(csock, msg_ping, 5, 0) != 5) {
    printf("[bank] Failed to send ping; AES failed\n");
    return 0;
  }
  byte aes_data[1024];
  int data_len = recv(csock, aes_data, 1024, 0);
  if (data_len <= 0) {
    printf("[bank] Failed to get pong; AES failed\n");
    return 0;
  }
  aes_decryption.ProcessData(aes_data, aes_data, data_len);
  if (memcmp(aes_data, msg_pong, 5) != 0) {
    printf("[bank] Failed to decrypt pong; AES failed\n");
    return 0;
  }
  printf("[bank] AES set up successfully\n");

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
    //read the packet from the ATM and AES decrypt
    /*if(sizeof(int) != recv(csock, &length, sizeof(int), 0))
      break;
    if(length >= 1024)
    {
      printf("packet too long\n");
      break;
    }*/
    /*if(length != recv(csock, packet, length, 0))
    {
      printf("[bank] fail to read packet\n");
      break;
    }*/
    int data_len;
    if((data_len = recv(csock, packet, 1024, 0)) <= 0)
    {
      printf("[bank] fail to read packet\n");
      break;
    }
    aes_decryption.ProcessData((byte*)packet, (byte*)packet, data_len);

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
      amount_ = str.substr(9, sec_space);

      //printf("%s withdrawn.\n", temp.c_str());
      withdraw_ = true;
    }
    else if(str.find("login ") == 0) {
      //takes in login [username] [pin]
      //Get PIN from Bank
      //prompt for PIN
      int sec_space = str.find(" ", 6);
      user_ = str.substr(6, sec_space);

      printf("User %s is logging in.\n", user_.c_str());

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

    for(unsigned int i = 0; i < 1024; ++i) {
      packet[i] = '\0';
    }
    std::string response;

    // -1 for non-existent account, 0 for continue
    if(login_) {
      active_account = accounts->find(user_);
      if (active_account == accounts->end())
      {
        std::cerr << "No such account\n";
        response = "-1";
      }
      else {
        response = "0";
        mid_login = true;
      }
    }
    // -1 for bad pin, 0 for success
    else if(mid_login) {
      mid_login = false;
      if (active_account->second.validate(pin_)) {
        validated = true;
        response = "0";
        std::cout << active_account->first << " is validated." << std::endl;
      }
      else {
        std::cout << active_account->first << " not validated; invalid pin" << std::endl;
        active_account = accounts->end();
        response = "-1";
      }
    }
    // all other commands require the user to have been authenticated
    else if(validated) {
      if (balance_) {
        // For some reason sprintf isn't working
        double balance = active_account->second.get_balance();
        std::stringstream ss;
        ss << balance;
        response = ss.str();
      }
      else if(withdraw_) {
      	double value = atof(amount_.c_str());
      	bool complete = active_account->second.withdraw(value);
      	if( complete) {
        response = "0";
      	}
      	else {
        response = "-1";
      	}
      }
      else if(transfer_) {
        bool success = false;

        // Find the other user
        std::map<std::string, Account>::iterator other_account;
        other_account = accounts->find(user_);
        if (other_account != accounts->end()) {
          // Parse the amount
          double amount = atof(amount_.c_str());
          if (amount > 0) {
            // Try to withdraw
            if (active_account->second.withdraw(amount)) {
              other_account->second.deposit(amount);
        response = "0";
              success = true;
            }
          }
        }

        if (!success) {
        response = "-1";
        }
      }
    }


    //encrypt the packet and send it back to the client
    memcpy(packet, response.c_str(), response.length());
    packet[response.length()] = '\0';
    length = response.length() + 1;
    aes_encryption.ProcessData((byte*)packet, (byte*)packet, length);
    /*if(sizeof(int) != send(csock, &length, sizeof(int), 0))
    {
      printf("[bank] fail to send packet length\n");
      break;
    }*/
    if(length != send(csock, (void*)packet, length, 0))
    {
      printf("[bank] fail to send packet\n");
      break;
    }

  }

  if (active_account != accounts->end())
  {
    active_account->second.logout();
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
