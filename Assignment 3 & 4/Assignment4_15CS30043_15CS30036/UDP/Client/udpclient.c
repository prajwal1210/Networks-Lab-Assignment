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
#include <netinet/in.h>
#include <netdb.h> 
#include <string.h>
#include <math.h>       /* ceil */
#include <ctype.h>

#define BUFSIZE 1024
#define DATASIZE 1032

/* 
 * error - wrapper for perror
 */
void error(char* msg) {
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
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    char packet[DATASIZE];
    char filename[BUFSIZE];
    char Ack[DATASIZE];
    int seq = 0;
    int retranscount = 0;

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

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

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
    }

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

    bzero(size, 1000);

    unsigned long packets = ceil(fsize/BUFSIZE);
    sprintf(size,"%lu", packets);

    strcat(buf, " ");
    strcat(buf, size);
    strcat(buf, " ");

    int j = strlen(buf);
    bzero(packet, DATASIZE);
    packet[0] = 0;
    packet[1] = 0;
    packet[2] = 0;
    packet[3] = 0;
    packet[7] = (j >> 24) & 0xFF;
    packet[6] = (j >> 16) & 0xFF;
    packet[5] = (j >> 8) & 0xFF;
    packet[4] = j & 0xFF;


    strcpy(&packet[8],buf);

    bzero(buf, BUFSIZE);
    strcpy(buf, &packet[8]);

    /* send the message to the server */
    while(1)
    {
    	serverlen = sizeof(serveraddr);
	    n = sendto(sockfd, packet, DATASIZE, 0, (struct sockaddr *)&serveraddr, serverlen);
	    if (n < 0) 
	      error("ERROR in sendto");
	    
    /* print the server's reply */
        bzero(Ack, DATASIZE);
	    n = recvfrom(sockfd, Ack, DATASIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
	    if (n < 0) 
        {
            retranscount++;
	      continue;
        }
	    if(strcmp(Ack, "0") == 0)
        {
            printf("%s, Acknowledged\n",Ack);
            seq++;
            break;
        }
	}
    char seq0[4];
    while(1)
        {
            bzero(packet,DATASIZE);

            packet[3] = (seq >> 24) & 0xFF;
            packet[2] = (seq >> 16) & 0xFF;
            packet[1] = (seq >> 8) & 0xFF;
            packet[0] = seq & 0xFF;

            int bytes_read = fread(&packet[8], sizeof(char), BUFSIZE, fp);
            if(bytes_read == 0)
            {
                break;
            }
            if(bytes_read < 0)
            {
                error("ERROR reading from file");
            }

            packet[7] = (bytes_read >> 24) & 0xFF;
            packet[6] = (bytes_read >> 16) & 0xFF;
            packet[5] = (bytes_read >> 8) & 0xFF;
            packet[4] = bytes_read & 0xFF;

            while(1)
            {
                n = sendto(sockfd, packet, bytes_read+8, 0, (struct sockaddr *)&serveraddr, serverlen);
                if(n <= 0)
                {
                    error("ERROR writing to socket\n");
                }
                bzero(Ack, DATASIZE);
                n = recvfrom(sockfd, Ack, DATASIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
                if (n < 0) 
                  {
                    retranscount ++;
                    continue;
                }
                if(atoi(Ack) == seq)
                {
                    printf("Transferring Bytes - %d\n", bytes_read );                    
                    printf("%s, Acknowledged\n",Ack);
                    seq++;
                    break;
                }
                else 
                    continue;

            }
        }
    fclose(fp);
    
    bzero(buf, BUFSIZE);
    //Recieve the MD5 Checksum as Acknowledgement from the server
    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
    printf("MD5 recieved - %s\n", &buf[1]);
    if (n < 0) 
          error("ERROR in recieve");

    bzero(Ack, DATASIZE);
    strcpy(Ack, "MD");
    n = sendto(sockfd, Ack, DATASIZE, 0, (struct sockaddr *)&serveraddr, serverlen);
    if (n < 0) 
        error("ERROR in sendto");
    printf("-----------Matching MD5 Checksum-----------\n");    
    //Match the MD5 Checksum calculated and the one recieved from the server
    if(strcmp(&buf[1],md5) == 0)
    {
        printf("MD5 Matched\n");
    }
    else
    {
        printf("MD5 Not Matched\n");
    }
    close(sockfd);
    printf("-----------Connection Closed-----------\n");
    printf("Retransmissions - %d\n", retranscount);
    return 0;
}
