all:
	g++ atm.cpp -m32 -o atm -lcryptopp
	g++ bank.cpp account.cpp -m32 -o bank -lpthread -lcryptopp
	g++ proxy.cpp -m32 -o proxy -lpthread
