/* 
 * tcpclient.c - A simple TCP client
 * usage: tcpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <string.h>
#include <string>
#include <iostream>
using namespace std;

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(string msg) {
    cout << msg << endl;
    exit(0);
}

/*The followinf function calculates the MDF hash of a given file (filename) in md5sum 
 *It returns a string(char*)
 *
 *Method - It uses popen to run md5sum filename and then reads the output whose first 32 digits 
 *are the MD5 hash for the file
 */

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
    int sockfd, portno, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    char filename[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
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

    /* connect: create a connection with the server */
    if (connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) 
      error("ERROR connecting");

    /* get message line from the user */
    printf("Please enter filename: ");
    
    //gets thr filename
    scanf("%[^\n]",filename);

    char md5[33];
    //Calculate the MD5 checksum of the file to be transferred
    getMD5(filename,md5);

    //Open the file
    FILE* fp;
    unsigned long fsize;
    fp = fopen(filename,"r");

    if(fp == NULL)
    {
        error("ERROR opening file");
        exit(0);
    }


    //Find the filesize
    fseek(fp, 0, SEEK_END);
    fsize = (unsigned long)ftell(fp);
    rewind(fp);

    char size[1000];

    sprintf(size,"%lu", fsize);

    bzero(buf, BUFSIZE);

    strcat(buf,filename);
    strcat(buf, " ");
    strcat(buf, size);

    /* send the hello message containing the filename and filesize to the server */
    n = write(sockfd, buf, strlen(buf));
    if (n < 0) 
      error("ERROR writing to socket");

    /* Get ackcowoneledgement from the server */
    bzero(buf, BUFSIZE);
    n = read(sockfd, buf, BUFSIZE);
    if (n < 0) 
      error("ERROR reading from socket");
    else if(strcmp(buf,"ACK")==0)
    {
        printf("Acknowledgement Recieved\n");
        printf("------Sending File Now------\n");
        while(1)
        {
        	/*
        	 *Read Data from the file and send it to the server
        	 */
            bzero(buf,BUFSIZE);
            int bytes_read = fread(buf, sizeof(char), BUFSIZE, fp);
            if(bytes_read == 0)
            {
                break;
            }
            if(bytes_read < 0)
            {
                error("ERROR reading from file");
            }
            while(bytes_read > 0)
            {
                int bytes_written = send(sockfd, buf, bytes_read, 0);
                printf("Transferring Bytes - %d\n", bytes_written );
                if(bytes_written <= 0)
                {
                    error("ERROR writing to socket\n");
                }
                bytes_read = bytes_read - bytes_written;
            }
        }
    printf("****File Sent!****\n");
    }
    else
    {
    	error("ERROR in receiving Acknowledgement");
    }
    fclose(fp);
    
    bzero(buf, BUFSIZE);
    //Recieve the MD5 Checksum as Acknowledgement from the server
    n = recv(sockfd, buf, BUFSIZE, 0);
    printf("Acknowledgement Recieved : MD5 - %s\n", buf );
    printf("-----------Matching MD5 Checksum-----------\n");
    
    //Match the MD5 Checksum calculated and the one recieved from the server
    if(strcmp(buf,md5) == 0)
    {
        printf("MD5 Matched\n");
    }
    else
    {
        printf("MD5 Not Matched\n");
    }
    close(sockfd);
    printf("-----------Connection Closed-----------\n");
    return 0;
}
