/* 
 * udpserver.c - A UDP echo server 
 * usage: udpserver <port_for_server>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <fcntl.h>
#include <fstream>
#include <sys/wait.h>
#include "dummy_tcp.h"
using namespace std;
#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

char* getMD5(char* filename, char* md5sum) {
    char mdigest[BUFSIZE] = "md5sum ";
    strcat(mdigest,filename);

    FILE* p = popen(mdigest,"r");
    if(p == NULL)
    {
      error("ERROR opening file when Generating md5sum");
    }

    int i = 0;
    char c;
    while(i < 32 && isxdigit(c = fgetc(p)))
    {
      md5sum[i] = c;
      i++;
    }
    md5sum[32] = '\0';
    pclose(p);
    return md5sum;
}

int main(int argc, char **argv) {
  int sockfd; /* socket file descriptor - an ID to uniquely identify a socket by the application program */
  int portno; /* port to listen on */
  socklen_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port_for_server>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  cout<<"------------ Server Running ------------"<<endl;
  clientlen = sizeof(clientaddr);
  
  dummy_tcp* dt;
  
  char data[1024];
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    dt = new dummy_tcp(sockfd,NULL);
    bzero(buf,BUFSIZE);
  	
  	cout<<"Trying to receive file details"<<endl;    

  	dt->app_recieve(buf,BUFSIZE/2);
    

    cout<<"File details received"<<endl;

    string str(buf);
    cout << str << endl;


    int i,c=1;
    for(i=0;str[i]!=' ';i++){   // Finding the start fileName
      c++;
    }
    for(i=c;str[i]!=' ';i++){  // Finding the end of fileName
    }
    char* fileName=new char;
    
    strcpy(fileName,str.substr(0,c-1).c_str());  // Getting fileName from message
    bzero(buf,BUFSIZE);
    
    strcpy(buf,(str.substr(c,i-c+1).c_str()));
    long int fileSize = atoi(buf);             // Getting fileSize in int
    cout<<"FileSize : "<<fileSize<<endl;
    
    long int bytes_received = 0;
    int sequence_no = 0;
    int ns,nr;

    FILE *f = fopen(fileName,"w");
    bytes_received = 0;

    cout<<"File Receiving "<<endl;
    int rem_size = fileSize;
    while(rem_size>0){
      bzero(buf,BUFSIZE);
      int nr = dt->app_recieve(buf,min(BUFSIZE,rem_size));
      cout << "Bytes received-" << nr << endl;
      fwrite(buf,sizeof(char),nr,f);
      rem_size-=nr;
      bytes_received+=nr;
    }
    cout<<"File bytes transfered : "<<bytes_received<<endl;
    cout<<"FILE TRANSFER COMPLETE :) "<<endl;
    fclose(f);
    //cout << dt->base_ptr << " " << dt->sender_data_buffer.size();
    char md5[33];
    bzero(md5,33);
    getMD5(fileName,md5);
    int n = dt->app_send(md5,33);
    sleep(10);
    delete(dt);
    exit(0);
  }
  }
