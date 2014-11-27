#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <string>

class Account
{
  private:
  std::string username;
  std::string pin;
  double balance;

  public:
  Account();
  Account(std::string a_username, std::string a_pin, double a_balance);

  bool validate(std::string a_pin);
  double get_balance();
  bool withdraw(double amount);
  void deposit(double amount);
};

#endif
