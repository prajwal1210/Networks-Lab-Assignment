/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
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
#include "dummy_tcp.h"
using namespace std;
#define BUFSIZE 1024

struct msg{
    int sequence_no;
    int length_of_chunk;
    char chunk[1024];
};

/* 
 * error - wrapper for perror
 */

/*
        Method to compute the size of file  
*/
long int get_file_size(char* filename) // path to file
{
    FILE *f = NULL;
    f = fopen(filename,"r");
    if(f==NULL){
        cout<<"File doesn't exist"<<endl;
        exit(1);
    }
    fseek(f,0,SEEK_END);
    long int size = ftell(f);
    fclose(f);
    return size;                // returns size of file
}

void error(char *msg) {
    perror(msg);
    exit(0);
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
    int sockfd, portno, n, retransmitted=0;
    socklen_t serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 4) {
       fprintf(stderr,"usage: %s <hostname> <port> <filename>\n", argv[0]);
       exit(0);
    }
    cout<<"--------- Client Running ------------"<<endl;
    hostname = argv[1];
    portno = atoi(argv[2]);
    char* filename = new char;
    cout<<"File Name : "<<argv[3]<<endl;                
    strcpy(filename,argv[3]);                           // copying fileName
    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) 
        error("ERROR opening socket");
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
    serveraddr.sin_port = htons(portno);

    dummy_tcp* dt=new dummy_tcp(sockfd,&serveraddr);

    /* get a message from the user */
    bzero(buf, BUFSIZE);



    int total_chunks;
    long int file_size = get_file_size(filename);                                 // Getting fileSize
    string str = string(filename)+" "+to_string(file_size)+" ";                       // Creating Hello message to be transfered
    cout<< "Msg Sent : "<< str << endl;
    serverlen = sizeof(serveraddr);


    /* send the message to the server */
    bzero(buf,BUFSIZE);
    strcpy(buf,str.c_str());
    //cout << buf << endl;
    n = dt->app_send(buf, BUFSIZE/2);
    
    //cout<<n<<"DATA"<<endl;
    bzero(buf,BUFSIZE);

    FILE* f = fopen(filename,"rb");
    if(f == NULL){
        error("File not found");
    }

    char md5[33];
    getMD5(filename,md5);

    int bytes_sent = 0;
    int nr,ns,size_read;
    while(bytes_sent < file_size){
        bzero(buf,1024);
        size_read = fread(buf,sizeof(char), BUFSIZE, f);
        ns = dt->app_send(buf,size_read);
        bytes_sent += ns;
    }

    //cout << dt->excepted_seq_no << " " << dt->reciever_data_buffer.size();
    bzero(buf,BUFSIZE);
    nr = dt->app_recieve(buf,33);
    cout << md5 << endl;
    cout << buf << endl;
    if(strcmp(md5,buf)==0)
    	cout << "MD5 matched" << endl;
    else
    	cout << "MD5 not matched" << endl;
    sleep(5);
    close(sockfd);
    delete(dt);
    return 0;
}
