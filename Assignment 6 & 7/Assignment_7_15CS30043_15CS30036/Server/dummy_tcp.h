#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <string.h>
#include <math.h>       
#include <ctype.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <semaphore.h>
using namespace std;

#define MSS 1024
#define MAX_BUFFERSIZE 250*1024

typedef struct H
{
	int seq;
	bool type;
	int length;
	int rw;

}header_t;

const int header_size = sizeof(header_t);

typedef struct P
{	
	header_t header;
	char data[MSS-header_size];
}packet_t;

class dummy_tcp
{
public:
	map<int,packet_t> reciever_packet_queue;
	deque<char> reciever_data_buffer;
	deque<char> sender_data_buffer;
	int size;
	int sockfd;
	int excepted_seq_no;
	int dupack;
	int rwnd;
	int cwnd;
	int swnd;
	//int wndptr;
	int ssthresh;
	int cur_ptr;
	int base_ptr;

	int wndptr;

	int drop;
		
 	socklen_t addrlen;
 	socklen_t serverlen; 
 	
 	struct sockaddr_in serveraddr; 
 	struct sockaddr_in addr;
	
	pthread_t sender;
	pthread_t reciever;

	sem_t send;
	sem_t recieve;
	sem_t update;

	char data[MSS];

	dummy_tcp(int sockfd, struct sockaddr_in* serveraddr);
	void* parse_packets(packet_t *packet);
	void* reciever_buffer_handler(packet_t *packet);
	int app_send(char* data,int len);
	int app_recieve(char* buf, int len);
	int sender_buffer_handler(char* data, int len);
	void* create_packet(char* buf, int len, int seq);
	int udp_send(char* data,int len);
	void update_window(packet_t* packet);
	void* send_ack(int ackno);
	void* udp_recieve(void* arg);
	void* flow_control();
	dummy_tcp();

	static void *udp_recieve_helper(void *context)
    {
        return ((dummy_tcp *)context)->udp_recieve(NULL);
    }
    static void *flow_control_helper(void *context)
    {
        return ((dummy_tcp *)context)->flow_control();
    }
};
