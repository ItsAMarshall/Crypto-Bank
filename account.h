#ifndef ACCOUNT_H
#define ACCOUNT_h

#include <string>

class Account
{
  private:
  std::string username;
  std::string pin;
  double balance;

  bool validate(std::string a_pin);

  public:
  Account(std::string a_username, std::string a_pin, double a_balance);

  bool get_balance(std::string a_pin, double& a_balance);
  bool sufficient_funds(std::string a_pin, double amount, bool& sufficient);
  bool withdraw(std::string a_pin, double amount);
  void deposit(double amount);
};

#endif
