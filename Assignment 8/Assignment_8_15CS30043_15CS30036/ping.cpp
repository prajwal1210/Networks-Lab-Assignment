#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <iostream>
#include <string>
#include <cmath>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <bits/stdc++.h>
#include <iostream>
#include <arpa/inet.h>
using namespace std;

#define DATASIZE    56
#define BUFSIZE     256
#define BILLION     1000000000

double* timearr;
int ind = 0;


struct ICMPHDR{
	u_char type;
	u_char code;
	u_short checksum;
	u_short id;
	u_short seq;
};


struct IPHDR
{
	u_char vihl;
	u_char tos;
	short len;
	short id;
	short flagoff;
	u_char ttl;
	u_char proto;
	u_short checksum;
	struct in_addr src;
	struct in_addr dest;
};


struct ECHOREQUEST{
    // struct ip iphd;
    IPHDR iphd;
    ICMPHDR header;
    char data[DATASIZE];
};

struct ECHOREPLY{
	IPHDR iphd;
	ICMPHDR header;
	char data[DATASIZE];	
};

unsigned short checksum(unsigned short *b, int len)
{	unsigned short *buf = b;
	unsigned int sum=0;
	unsigned short result;

	for ( sum = 0; len > 1; len -= 2 )
		sum += *buf++;
	if ( len == 1 )
		sum += *(unsigned char*)buf;
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;
	return result;
}


void add_ip_header(ECHOREQUEST* pckt, struct in_addr dest)
{
    pckt->iphd.vihl = 4; 
    pckt->iphd.vihl = pckt->iphd.vihl << 4;
    pckt->iphd.vihl = pckt->iphd.vihl | 5;
    pckt->iphd.tos = 0;
    pckt->iphd.len = htons(sizeof(ECHOREQUEST));
    pckt->iphd.id = htons(getpid());
    pckt->iphd.flagoff = 0X00;
    pckt->iphd.ttl = 255;
    pckt->iphd.proto = IPPROTO_ICMP;
    pckt->iphd.checksum = 0;
    pckt->iphd.dest = dest;
    pckt->iphd.src.s_addr = htonl(INADDR_ANY);
    pckt->iphd.checksum =  checksum((unsigned short*)pckt,sizeof(IPHDR));

    // pckt->iphd.ip_hl = 5;
    // pckt->iphd.ip_v = 4;
    // pckt->iphd.ip_tos = 0;
    // pckt->iphd.ip_len = 20+8+DATASIZE; // 20 for ip header , 8 icmp header , datalen for the icmp message 
    // pckt->iphd.ip_id = getpid();
    // pckt->iphd.ip_off = 0;
    // pckt->iphd.ip_ttl = 50; 
    // pckt->iphd.ip_p = IPPROTO_ICMP;
    // pckt->iphd.ip_src.s_addr = htonl(INADDR_ANY);
    // // inet_pton (AF_INET, htonl(INADDR_ANY), &(pckt->iphd.ip_src));
    // inet_pton (AF_INET, inet_ntoa(dest), &(pckt->iphd.ip_dst));
    // pckt->iphd.ip_sum = checksum((unsigned short *) pckt, sizeof(struct ip));

}

void create_packet(ECHOREQUEST* pckt, int seq)
{
	pckt->header.type = ICMP_ECHO;
	pckt->header.code = 0;
	pckt->header.id = getpid();
	pckt->header.seq = seq;
    struct timespec* sendtime = (struct timespec*)pckt->data;
	clock_gettime(CLOCK_REALTIME, sendtime);
	pckt->header.checksum = 0;
	pckt->header.checksum = checksum((unsigned short*)&(pckt->header),sizeof(ICMPHDR) + DATASIZE);
	

}

int parse_packet(char* buf, int n, int seq, struct timespec recvtime)
{
	ECHOREPLY* pckt = (ECHOREPLY*)buf;
	if(pckt->header.id == getpid() && pckt->header.seq == seq)
	{
		cout << n << " bytes from ";
		cout << inet_ntoa(pckt->iphd.src) << ": ";
		cout << "icmp_seq=" << seq << " ";
		struct timespec sendtime = *((struct timespec *)pckt->data);
		cout << "ttl=";
		printf("%u",pckt->iphd.ttl);
		cout << " ";
		double time = (recvtime.tv_sec - sendtime.tv_sec)+(recvtime.tv_nsec - sendtime.tv_nsec)/(double)BILLION;
        timearr[ind++] = time*1000;
        cout << "time=" << time*1000 << "ms" <<endl;
		return 1;
	}
	else 
		return 0;


}

int main(int argc, char **argv) {
    int sockfd, n;
    socklen_t serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char dgram[BUFSIZE];
    struct timespec recvtime;
    struct timespec start;
    struct timespec stop;

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <ping number>\n", argv[0]);
       exit(0);
    }
    
    hostname = argv[1];
    int pingnum = atoi(argv[2]);
    timearr = new double(pingnum);

    /* socket: create the socket */
	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) 
        perror("ERROR opening socket");

    int on = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
            perror("setsockopt");
            exit(1);
    }
    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
      (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = 0;

    cout << "Pinging - " << inet_ntoa(serveraddr.sin_addr) << endl;
    /* set the TTL option */
    const int val=255;
    if ( setsockopt(sockfd, SOL_IP, IP_TTL, &val, sizeof(val)) != 0)
		perror("Set TTL option");

	/* set socket timer */
	struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
    }

	ECHOREQUEST* pckt;
	int seq = pingnum;
	int lostpackets = 0;
    int packetsrcv = 0;
    double accum = 0;
    for(int i = 0; i < pingnum; i++)
    {
        clock_gettime(CLOCK_REALTIME, &start);
    	pckt = new ECHOREQUEST();
    	bzero(pckt, sizeof(ECHOREQUEST));
        add_ip_header(pckt,serveraddr.sin_addr);
    	create_packet(pckt,seq);
    	serverlen = sizeof(serveraddr);
    	n = sendto(sockfd,pckt,sizeof(ECHOREQUEST),0,(struct sockaddr*)&serveraddr,serverlen);
    	if(n < 0)
    	{
    		perror("sendto");
    	}

    	/* Recieve echo reply */
    	bzero(dgram,BUFSIZE);
    	n = recvfrom(sockfd,dgram,BUFSIZE,0,(struct sockaddr*)&serveraddr,&serverlen);
    	if(n < 0)
    	{
    		lostpackets++;
    		seq --;

    	} 
    	else
    	{

            clock_gettime(CLOCK_REALTIME, &recvtime);
    		if(parse_packet(dgram,n,seq,recvtime) == 0)
    		{
    			lostpackets++;
    		}
            else
            {
                packetsrcv++;
            }
    		seq --;

    	}
        clock_gettime(CLOCK_REALTIME, &stop);
        accum  += (stop.tv_sec - start.tv_sec)+(stop.tv_nsec - start.tv_nsec)/(double)BILLION;

    	sleep(1);
    }
    cout << "---Statistics---" << endl;
    cout << pingnum << " packets transmitted, " << packetsrcv << " received, ";
    cout << (lostpackets/(double)pingnum)*100 << "\% packet loss, time " << accum*1000 << "ms" << endl;
    double minrtt = *min_element(timearr,timearr+packetsrcv);
    double maxrtt = *max_element(timearr,timearr+packetsrcv);
    double avgrtt = 0;
    for(int i = 0; i < packetsrcv; i++)
       {
            avgrtt+=timearr[i];
       }
    avgrtt/=packetsrcv;
    double mdevrtt = 0;
    for(int i = 0; i < packetsrcv; i++)
       {
            mdevrtt+=abs(timearr[i]-avgrtt);
       }
    mdevrtt/=packetsrcv;
    cout << "rtt min/avg/max/mdev = " << minrtt << "/" << avgrtt << "/" << maxrtt << "/" << mdevrtt << " ms" << endl;

}