all : client

client : udpclient.cpp dummy_tcp.cpp dummy_tcp.h
	g++ udpclient.cpp dummy_tcp.cpp -lpthread -std=c++11 -Wno-write-strings -o client

