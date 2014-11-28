/**
  @file atm.cpp
  @brief Top level ATM implementation file
  */
#include <string>
#include <iostream>
#include <cassert>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <cryptopp/osrng.h>
#include <cryptopp/rsa.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>

using namespace std;
using namespace CryptoPP;

void withdraw_send(string& input, char packet[]) {

  int sec_space = input.find(" ", 9);
  string temp = input.substr(9, sec_space);

  printf("%s withdrawn.\n", temp.c_str());

  for(unsigned int i = 0; i < input.size(); ++i) {
    packet[i] = input[i];
  }
  return;
}

void login_send(string& input, char packet[]) {
  //Get PIN from Bank
  //prompt for PIN
  int sec_space = input.find(" ", 6);
  string temp = input.substr(6, sec_space);

  printf("User %s is logging in.\n", temp.c_str());

  for(unsigned int i = 0; i < input.size(); ++i) {
    packet[i] = input[i];
  }
  return;
}

void balance_send(string& input, char packet[]) {
  printf("Checking Balance\n");
  for(unsigned int i = 0; i < input.size(); ++i) {
    packet[i] = input[i];
  }
}

void transfer_send(string& input, char packet[]) {
  int space = input.find(" ", 9);
  string amount = input.substr(9, (space-9));
  printf("amount %s\n", amount.c_str());
  space = input.find(" ", 9+amount.size());
  string destination = input.substr(space+1);
  printf("Transfering %s to %s.\n", amount.c_str(), destination.c_str());

  for(unsigned int i = 0; i < input.size(); ++i) {
    packet[i] = input[i];
  }
}

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

  //ENCRYPTION SETUP
  //Generate random AES private key and IV
  AutoSeededRandomPool pool;
  int aes_key_size = 32;
  SecByteBlock aes_key(0x00, aes_key_size);
  pool.GenerateBlock(aes_key, aes_key_size);
  byte aes_iv[AES::BLOCKSIZE];
  pool.GenerateBlock(aes_iv, AES::BLOCKSIZE);

  //Get public RSA key
  unsigned char* prepacket = new unsigned char[1024];
  if (recv(sock, prepacket, 1024, 0) <= 0)
  {
    printf("[atm] Failed to receive public key\n");
    return -1;
  }
  lword public_key_size;
  memcpy(&public_key_size, prepacket, 8);

  //Instantiate RSA public key
  ByteQueue queue;
  queue.Put(prepacket + 8, public_key_size);
  RSA::PublicKey rsa_public;
  rsa_public.Load(queue);
  RSAES_OAEP_SHA_Encryptor rsa_encryptor(rsa_public);

  //Encrypt AES private key and IV with RSA
  int rsa_pt_size = aes_key_size + AES::BLOCKSIZE;
  SecByteBlock rsa_pt(rsa_pt_size);
  memcpy(rsa_pt.BytePtr(), aes_key.BytePtr(), aes_key_size);
  memcpy(rsa_pt.BytePtr() + aes_key_size, aes_iv, AES::BLOCKSIZE);
  size_t rsa_ct_size = rsa_encryptor.CiphertextLength(rsa_pt_size);
  SecByteBlock rsa_ct(rsa_ct_size);
  rsa_encryptor.Encrypt(pool, rsa_pt, rsa_pt_size, rsa_ct);

  // Send AES creds
  memcpy(prepacket, rsa_ct.BytePtr(), rsa_ct_size);
  if (send(sock, prepacket, rsa_ct_size, 0) != rsa_ct_size)
  {
    printf("[atm] Failed to send AES creds\n");
    return -1;
  }

  // Verify AES functionality
  CFB_Mode<AES>::Encryption aes_encryption(aes_key, aes_key_size, aes_iv);
  CFB_Mode<AES>::Decryption aes_decryption(aes_key, aes_key_size, aes_iv);
  byte msg_ping[5] = {'P', 'I', 'N', 'G', '\0'};
  byte msg_pong[5] = {'P', 'O', 'N', 'G', '\0'}; 
  byte aes_data[1024];
  int data_len = recv(sock, aes_data, 1024, 0);
  if (data_len <= 0)
  {
    printf("[atm] Failed to get ping; AES failed\n");
    return -1;
  }
  aes_decryption.ProcessData(aes_data, aes_data, data_len);
  if (memcmp(aes_data, msg_ping, 5) != 0)
  {
    printf("[atm] Failed to decrypt ping; AES failed\n");
    return -1;
  }
  aes_encryption.ProcessData(msg_pong, msg_pong, 5);
  if (send(sock, msg_pong, 5, 0) != 5) {
    printf("[atm] Failed to send pong; AES failed\n");
    return -1;
  }
  printf("[atm] AES set up successfully\n");
  if (recv(sock, prepacket, 1024, 0) <= 0) {
    printf("[atm] Failed to receive ack\n");
    return -1;
  }

  //input loop
  char buf[80];
  string input;
  bool logged_in = false;
  while(1)
  {
    printf("atm> ");
    getline(cin, input);
    char packet[1024];
    int length = input.size();

    //input parsing
    if(input.find("logout") == 0)
      break;
    else if(logged_in && input.find("withdraw ") == 0) {
      //takes in withdraw [amount]
      withdraw_send(input, packet);
    }
    else if(input.find("login ") == 0) {
      //takes in login [username]
      login_send(input, packet);

    }
    else if(logged_in && input.find("balance") == 0) {
      balance_send(input, packet);
    }
    else if(logged_in && input.find("transfer ") == 0) {
      //takes in transfer [amount] [username]
      transfer_send(input, packet);
    }
    else {
      printf("invalid input.  Valid operations are:\n \
          login | withdraw | balance | transfer | logout\n");
      continue;
    }

    // AES encrypt and send
    char buff[1024];
    memcpy(buff, input.c_str(), input.length());
    buff[input.length()] = 0;
    aes_encryption.ProcessData((byte*)buff, (byte*)buff, input.length() + 1);
    if (send(sock, buff, input.length() + 1, 0) <= 0)
    {
      break;
    }

    for(unsigned int i = 0; i < input.size(); ++i) {
      packet[i] = '\0';
    }

    // Receive and AES decrypt
    int data_len;
    if ((data_len = recv(sock, packet, 1024, 0)) <= 0)
    {
      break;
    }
    aes_decryption.ProcessData((byte*)packet, (byte*)packet, data_len);

    string str(packet);

    /////////

    if(input.find("withdraw ") == 0) {
      if( str.find("-1") == 0){
        printf("Could not withdraw funds.\n");
      }
      else if( str.find("0") == 0) {
      	printf("Withdrew funds.\n");
      }
    }
    else if(input.find("login ") == 0) {
      //Is user?
      if( str.find("-1") == 0){
        printf("User not found.\n");
      }
      else if( str.find("0") == 0) {
        printf("Please enter a PIN\n");
        string pin;
        getline(cin, pin); 
        assert(pin.size() == 4);
        for(unsigned int i = 0; i < 4; ++i) {
          packet[i] = pin[i];
        }
        packet[4] = '\0';
        aes_encryption.ProcessData((byte*)packet, (byte*)packet, 5);
        if (send(sock, packet, pin.size() + 1, 0) <= 0)
        {
          break;
        }
        if ((data_len = recv(sock, packet, 1024, 0)) <= 0)
        {
          break;
        }
        aes_decryption.ProcessData((byte*)packet, (byte*)packet, data_len);
        string str(packet);
        if (str[0] == '0') {
          cout << "You are logged in\n";
          logged_in = true;
        }
        else {
          cout << "Failed to log in with given PIN\n";
        }
      }
    }
    else if(input.find("balance") == 0) {
      cout << "Your balance is $" << str << endl;
    }
    else if(input.find("transfer ") == 0) {
      if( str.find("-1") == 0){
        printf("Could not transfer funds.\n");
      }
      else if( str.find("0") == 0) {
      	printf("Funds transfered.\n");
      }
    }

    /////////




  }

  //cleanup
  close(sock);
  return 0;
}
