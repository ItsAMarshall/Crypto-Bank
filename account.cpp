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

double Account::get_balance()
{
  return balance;
}

bool Account::withdraw(double amount)
{
  if (amount > balance)
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
