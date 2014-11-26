#include "account.h"

bool Account::validate(std::string a_pin)
{
  return (a_pin == pin);
}

Account::Account(std::string a_username, std::string a_pin, double a_balance)
{
  username = a_username;
  pin = a_pin;
  balance = a_balance;
}

bool Account::get_balance(std::string a_pin, double& a_balance)
{
  if (!validate(a_pin))
  {
    return false;
  }

  a_balance = balance;

  return true;
}

bool Account::sufficient_funds(std::string a_pin, double amount, bool& sufficient)
{
  if (!validate(a_pin))
  {
    return false;
  }

  sufficient = (amount <= balance);

  return true;
}

bool Account::withdraw(std::string a_pin, double amount)
{
  bool sufficient;
  if (!sufficient_funds(a_pin, amount, sufficient) || !sufficient)
  {
    return false;
  }

  balance -= amount;

  return true;
}

void Account::deposit(double amount)
{
  balance += amount;
}
