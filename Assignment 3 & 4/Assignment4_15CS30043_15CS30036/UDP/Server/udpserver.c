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
#include <ctype.h>


#define DATASIZE 1032
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
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char packet[DATASIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  char buf[BUFSIZE];
  char fileinfo[DATASIZE];
  char Ackseq[32];

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

  struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
    }
  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  int seq = 0;
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero((char *) &fileinfo, DATASIZE);
    // bzero(buf, BUFSIZE);
    // n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
    // if (n < 0)
    //   error("ERROR in recvfrom");
    n = recvfrom(sockfd, fileinfo, DATASIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
    if(n<0)
      continue;

    printf("%s\n", buf);
    int count = 8;
    char seq0[4],len0[4];
    seq0[0] = fileinfo[0];
    seq0[1] = fileinfo[1];
    seq0[2] = fileinfo[2];
    seq0[3] = fileinfo[3];

    len0[0] = fileinfo[4];
    len0[1] = fileinfo[5];
    len0[2] = fileinfo[6];
    len0[3] = fileinfo[7];

    int seqrec = *(int *)seq0;
    int len = *(int *)len0;
    char filename[256];
    int j=0;

    while(fileinfo[count] != ' ')
    {
      filename[j] = fileinfo[count];
      count++;
      j++;
    }
    filename[j] = '\0';
    j=0;
    count++;
    char filesize[128];
    while(fileinfo[count] != ' ')
    {
      //printf("%c".fileinfo[count]);
      filesize[j] = fileinfo[count];
      count++;
      j++;
    }
    filesize[j] = '\0';
    int files = atoi(filesize);
    j=0;
    count++;
    char packets[128];
    while(fileinfo[count] != ' ')
    {
      packets[j] = fileinfo[count];
      count++;
      j++;
    }
    packets[j] = '\0';
    int packs = atoi(packets);


    FILE* fp;
    fp = fopen(filename,"w");
    if(fp == NULL)
    {
      error("ERROR opening file");
    }

    bzero(packet, BUFSIZE);
    sprintf(packet,"%d",seqrec);

    printf("Filename- %s\n",filename);
    printf("Filesize- %d\n",files);
    printf("Packets- %d\n",packs);
    /*
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr, 
			  sizeof(clientaddr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server received datagram from %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);
    
    /* 
     * sendto: echo the input back to the client 
     */
    n = sendto(sockfd, packet, strlen(packet), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("ERROR in sendto");
    seq++;
    int bytesrec = 0;
    int data_bytes_read = 0;
    int ack_re = 0;
    while(bytesrec < files)
    {

      bzero((char *) &packet, DATASIZE);
      n = recvfrom(sockfd, packet, DATASIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
      data_bytes_read = n-8;
      if(n<0)
        error("ERROR in receiving filename");
      seq0[0] = packet[0];
      seq0[1] = packet[1];
      seq0[2] = packet[2];
      seq0[3] = packet[3];

      len0[0] = packet[4];
      len0[1] = packet[5];
      len0[2] = packet[6];
      len0[3] = packet[7];

      seqrec = *(int *)seq0;
      len = *(int *)len0;

      printf("%d \n", seqrec);

      if(seqrec < seq)
      {
        //Send Deplicate ack
        bzero(packet, DATASIZE);
        sprintf(packet,"%d",seqrec);
        n = sendto(sockfd, packet, strlen(packet), 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) 
          error("ERROR in sendto");
        ack_re++;
      }
      else if(seqrec == seq)
      {
        printf("Receiving %d Bytes\n", data_bytes_read);
        bzero(buf, BUFSIZE);
        //strcpy(buf,&packet[8]);
        n = fwrite(&packet[8], sizeof(char), data_bytes_read, fp); //Write to the new file
        if(n<0)
          error("ERROR writing to the file");
        bytesrec+= n;
        bzero(packet, DATASIZE);
        sprintf(packet,"%d",seqrec);
        n = sendto(sockfd, packet, strlen(packet), 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) 
          error("ERROR in sendto");
        seq++;

      }
      else
        error("Some error ocurred");
    }
      printf("Transfer Complete\n");
      fclose(fp);
      char md5[34];
      getMD5(filename,&md5[1]);   //Generate the MD5 Checksum of the file recieved
      md5[0] = 'M';
      bzero(buf, BUFSIZE);
      printf("---------- Sending MD5-------\n");
      for(int i = 0; i<5;i++)
      {
        n = sendto(sockfd, md5, strlen(md5), 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) 
        error("ERROR in sendto");

        bzero(packet, DATASIZE);
        n = recvfrom(sockfd, packet, DATASIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0) 
          continue;
        if(strcmp(packet, "MD") == 0)
          {
              printf("%s, Acknowledged for MD5\n",packet);
            break;   
                }
      }
      printf("Retransmitted Acks - %d\n",ack_re);
      printf("------------ Succes!---------\n");
      printf("------Connection Closed------\n");
      seq=0;
  }

}
