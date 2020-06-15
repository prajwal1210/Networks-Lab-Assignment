all : server

server : udpserver.cpp dummy_tcp.cpp dummy_tcp.h
	 g++ -g udpserver.cpp dummy_tcp.cpp -lpthread -Wno-write-strings -Wno-format -o server

