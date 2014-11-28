#include "account.h"

Account::Account(std::string a_username, std::string a_pin, double a_balance)
{
  username = a_username;
  pin = a_pin;
  balance = a_balance;
  logged_in = false;
}

// Required for map functions
Account::Account()
{
  username = "";
  pin = "";
  balance = 0;
  logged_in = false;
}

bool Account::validate(std::string a_pin)
{
  // Do not allow double log-in
  if (!logged_in && a_pin == pin)
  {
    logged_in = true;
    return true;
  }
  else
  {
    return false;
  }
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

  if (amount <= 0)
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

void Account::logout()
{
  logged_in = false;
}
