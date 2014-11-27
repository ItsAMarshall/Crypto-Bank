Crypto-Bank
===========

COMPLETED
ATM can communicate with the bank, send data in form of Packet which bank converts to string and parses
Bank returns a packet of user PIN for login purposes
	May have bank send back either true or false to tell if user is there, then user sends pin to gain access
	and bank verifies.

Withdraw and balance shouldnt be too hard
	Just have to get login working first because a user is not specified on the input for those commands.


TODO
We have to have the current user as a field in each packet so the bank knows who it is.
Once communication between ATM and Bank is working we have to encrypt it.  
	Might have proxy do some shuffling?
