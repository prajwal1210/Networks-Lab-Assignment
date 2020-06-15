/* 
 * tcpserver.c - A simple TCP echo server 
 * usage: tcpserver <port>
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
#include <string>
#include <iostream>
#include <ctype.h>


using namespace std;

#define BUFSIZE 1024

#if 0
/* 
 * Structs exported from in.h
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

/*
 * error - wrapper for perror
 */
void error(string msg) {
  cout << msg << endl;
  exit(1);
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
  int parentfd; /* parent socket */
  int childfd; /* child socket */
  int portno; /* port to listen on */
  socklen_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buffer */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  parentfd = socket(AF_INET, SOCK_STREAM, 0);
  if (parentfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));

  /* this is an Internet address */
  serveraddr.sin_family = AF_INET;

  /* let the system figure out our IP address */
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

  /* this is the port we will listen on */
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(parentfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * listen: make this socket ready to accept connection requests 
   */
  if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */ 
    error("ERROR on listen");
  printf("Server Running ....\n");
  /* 
   * main loop: wait for a connection request, echo input line, 
   * then close connection.
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /* 
     * accept: wait for a connection request 
     */
    childfd = accept(parentfd, (struct sockaddr *) &clientaddr, &clientlen);
    if (childfd < 0) 
      error("ERROR on accept");
    
    /* 
     * gethostbyaddr: determine who sent the message 
     */
    /*hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server established connection with %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    */
    printf("Connection Established\n");
    /* 
     * read: read hello string from the client
     */

    bzero(buf, BUFSIZE);
    n = read(childfd, buf, BUFSIZE);
    if (n < 0) 
      error("ERROR reading from socket");
    
    printf("server received %d bytes: %s\n", n, buf);

    /* 
     * Now we split the hello message to retrieve the filename and filesize
     */
    char filename[BUFSIZE]; //stores the filename
    char filesize[BUFSIZE]; //stores the filesize
    int l = 0;
    int k = 0;
    bzero(filename, BUFSIZE);
    bzero(filesize, BUFSIZE);
    int i = 0;
    int cnt_space = 0;
    while(buf[i]!= '\0')
    {
      if(buf[i] == ' ')
        cnt_space++;
    	else if(cnt_space == 0)
    	{
    		filename[k] = buf[i];
    		k++;
    	}
    	else if(cnt_space == 1)
    	{
    		filesize[l] = buf[i];
    		l++;
    	}

    	i++;
    	
    }
    filename[k] = '\0';
    filesize[l] = '\0';

    //Convert filesize to int 
    int files = atoi(filesize);
    printf("Name of file- %s\nSize of file - %s\n",filename, filesize);

    /*
     * We open the file with the same filename on the server side 
     */
    FILE* fp;
    fp = fopen(filename,"w");
    if(fp == NULL)
    {
      error("ERROR opening file");
    }

    /*
     * Server sends an acknowledgement to the client
     */
    n = write(childfd, "ACK" , strlen(buf));
    if (n < 0) 
      error("ERROR writing to socket");


    /*
     *Now we read the file transfered via a bytestream and write it to our copy file created
     */
  	int read_size = 0;
  	while(1)
  	{
  		bzero(buf, BUFSIZE);
  		n = recv(childfd, buf, BUFSIZE,0); //Recieve the data from the client
  		if(n < 0)
  			error("ERROR reading from socket");
  		read_size += n;                   //Keep track of the no. of bytes read till now
      printf("Receiving %d Bytes\n", n);
  		n = fwrite(buf, sizeof(char), n, fp); //Write to the new file
  		if(n<0)
  			error("ERROR writing to the file");
      if(read_size == files)          //Stop when the full file has been read
        {
          break;
        }
  	}
  	fclose(fp);
    printf("------Succesfully transferred %s------\n", filename);
    char md5[33];
    getMD5(filename,md5);   //Generate the MD5 Checksum of the file recieved
    bzero(buf, BUFSIZE);
    printf("------Sending MD5 Acknowledgement------\n");
    n = send(childfd, md5 , strlen(md5), 0);  //Senf thr MD5 Checksum as acknowledgement to the client
    printf("------Succes!------\n");
    printf("------Connection Closed------\n");
    close(childfd);
  }
}
