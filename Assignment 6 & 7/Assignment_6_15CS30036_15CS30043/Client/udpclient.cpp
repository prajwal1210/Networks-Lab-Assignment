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
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <bits/stdc++.h>
using namespace std;


#define BUFSIZE 1024
#define DATASIZE 1032
#define SLEEP_VAL 1


static int alarm_fired = 0;

//Signal Handler for Alarm
void mysig(int sig)
{
	pid_t pid;
	printf("PARENT : Received signal %d \n", sig);
	if (sig == SIGALRM)
	{
		alarm_fired = 1;
	}
	(void) signal(SIGALRM, mysig);
	printf("Alarm - %d\n",alarm_fired);
} 


/* 
 * error - wrapper for perror
 */
void error(string msg) {
    perror(msg.c_str());
    exit(0);
}

//Get MD5 Hash of the given file
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


typedef struct elem{
	char packet[DATASIZE];
	int size;
	int seqno;
}element;


int main(int argc, char **argv) {
    int sockfd, portno, n; 				//socket, port number and no of bytes 
    socklen_t serverlen;	
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    char packet[DATASIZE];
    char filename[BUFSIZE];
    char Ack[DATASIZE];
    int baseseq = -1;
    int currseq = -1;
    int WINDOWSIZE = 3;
    deque<element> window; 
    deque<element> remaining;
    char seq0[4];
    element temp;
    element t;

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
    printf("%s\n",md5);

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
	

    //convert size to string
	char size[1000];
    sprintf(size,"%lu", fsize);

    bzero(buf, BUFSIZE);

    //Put filename and size in the buffer
    strcat(buf,filename);
    strcat(buf, " ");
    strcat(buf, size);

    bzero(size, 1000);

    //Find the no. of packets that will be transferred
    unsigned long packets = ceil((float)fsize/BUFSIZE);
    sprintf(size,"%lu", packets);
    printf("No of packets - %lu\n", packets);

    //Add the no. of packets to our message
    strcat(buf, " ");
    strcat(buf, size);
    strcat(buf, " ");

   	//Add the header with 0 seq number and length of the message to the message
    int j = strlen(buf);
    bzero(packet, DATASIZE);
    int neg = -1;
    packet[0] = (neg >> 24) & 0xFF;
    packet[1] = (neg >> 16) & 0xFF;
    packet[2] = (neg >> 8) & 0xFF;
    packet[3] =  neg & 0xFF;
    packet[7] = (j >> 24) & 0xFF;
    packet[6] = (j >> 16) & 0xFF;
    packet[5] = (j >> 8) & 0xFF;
    packet[4] = j & 0xFF;


    strcpy(&packet[8],buf);

    bzero(buf, BUFSIZE);
    //strcpy(buf, &packet[8]);

    /* send the initial message to the server */
    while(1)
    {
    	serverlen = sizeof(serveraddr);
	    n = sendto(sockfd, packet, DATASIZE, 0, (struct sockaddr *)&serveraddr, serverlen);
	    if (n < 0) 
	      error("ERROR in sendto");
	   	
	   	//Recieve ACK from the server 
        bzero(Ack, DATASIZE);
	    n = recvfrom(sockfd, Ack, DATASIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
	    if (n < 0) 
        {
          	//If timeout happens 
	      	continue;
        }
	    if(strcmp(Ack, "Ack") == 0)
        {
            printf("%s, Acknowledged\n",Ack);
            break;
        }
	}


    
    while(1)
        {

            while(baseseq - currseq < WINDOWSIZE ){

	        	if(!remaining.empty()) //If the remaining queue is not empty first send data from this
	        	{
	        		temp = remaining.front();
	        		remaining.pop_front();
	        		window.push_back(temp);
	            	baseseq = temp.seqno;
	            	//Send to server 
		            n = sendto(sockfd, temp.packet, temp.size, 0, (struct sockaddr *)&serveraddr, serverlen);
		                if(n <= 0)
		                {
		                    error("ERROR writing to socket\n");
		                }
		            printf("Transmitting Bytes - %d Seq-no:%d\n", temp.size-8,baseseq);
	        	}
	        	else if(!feof(fp)){  //If we have not reached end of file
		            bzero(packet,DATASIZE);
		            baseseq++;

		            //Store the sequence no.
		            packet[3] = (baseseq >> 24) & 0xFF;
		            packet[2] = (baseseq >> 16) & 0xFF;
		            packet[1] = (baseseq >> 8) & 0xFF;
		            packet[0] = baseseq & 0xFF;

		            //Read data from the file
		            int bytes_read = fread(&packet[8], sizeof(char), BUFSIZE, fp);
		            if(bytes_read == 0)
		            {	
		                	break;
		            }
		            if(bytes_read < 0)
		            {
		                error("ERROR reading from file");
		            }

		            //Add packet length to the header
		            packet[7] = (bytes_read >> 24) & 0xFF;
		            packet[6] = (bytes_read >> 16) & 0xFF;
		            packet[5] = (bytes_read >> 8) & 0xFF;
		            packet[4] = bytes_read & 0xFF;

		            //Push into the window
		            bzero(temp.packet,DATASIZE);
		            bcopy(packet,temp.packet,DATASIZE);
		            temp.size = bytes_read+8;
		            temp.seqno = baseseq;

		        
			        window.push_back(temp);
		            //Send to server 
		            n = sendto(sockfd, temp.packet, temp.size, 0, (struct sockaddr *)&serveraddr, serverlen);
		                if(n <= 0)
		                {
		                    error("ERROR writing to socket\n");
		                }

	            	printf("Transmitting Bytes - %d Seq-no:%d\n", temp.size-8,baseseq);
	        		}
	        		else
	        			break;
	     		}

            	while(baseseq - currseq == WINDOWSIZE || (baseseq == packets-1 && currseq!=baseseq))
            	{
					// alarm_fired = 0;
     				//alarm(SLEEP_VAL);
     				//(void) signal(SIGALRM, mysig);
     				//int flag = 0;
     				//do
					// {
        			int flag=0;
					bzero(Ack, DATASIZE);
					/*Wait for acknowledgments because the whole window has been sent*/
            		n = recvfrom(sockfd, Ack, DATASIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
					if (n < 0) 
	                {
	                    //Time out;
	                    printf("Timeout\n");
	                    flag = 0;
	                }
                    else if(n==0)
                    {
                        printf("Connection closed\n");
                        exit(0);
                    }
	                else if(atoi(Ack) > currseq) //If a higher packet is acknowledged
	                {                  
	                    printf("%s, Acknowledged ",Ack);
	                    for(int l = currseq+1; l <= atoi(Ack); l++)
	                    {
	                    	if(!window.empty())
	                    	{
		                    	window.pop_front();
		                    }

		                    else 
		                    	break;
	                    }
	                    //Update the window size according to the congestion control algo
	                    WINDOWSIZE += atoi(Ack) - currseq;

	                    //Update the currseq 
	                    currseq = atoi(Ack);
	                    
	                    flag = 1;
	                    break;
	                }
					//}while(!alarm_fired);
					//Timeout - Packet loss
					
	                /*Retransmission timeout */
					if(flag == 0)
					{
						printf("Retransmitting\n");
						
						//Decrease the window size 
						WINDOWSIZE=max(3,WINDOWSIZE/2);
						deque<element>::iterator it;
						int i = 0; 
						while(window.size() > WINDOWSIZE)
						{
							//Add the remaining packets after decreasing the window size to another queue 
							printf("Add to remaining seq no - %d\n", window.back().seqno);
							remaining.push_front(window.back());
							window.pop_back();
						}

						//Retransmit the packets
						for(it = window.begin() ; it!=window.end(); ++it)
						{
								baseseq = it->seqno;
								t = *it;
								n = sendto(sockfd, it->packet, it->size, 0, (struct sockaddr *)&serveraddr, serverlen);
					                if(n <= 0)
					                {
					                    error("ERROR writing to socket\n");
					                }
					            printf("Retransmitting Bytes - %d Seq-no:%d\n", t.size-8,currseq+i+1); 
				            	i++;
						}
					
            		}		
            	}
            	printf("Base - %d Curr - %d Window size- %d\n", baseseq, currseq, WINDOWSIZE);
            	
            	//If aLL packets have been sent and acknowledged then break 
            	if(baseseq == packets-1 && currseq == baseseq)
            		break;
        	}

    //Close the file
    fclose(fp);
    printf("\n");
    bzero(buf, BUFSIZE);
    //Recieve the MD5 Checksum as Acknowledgement from the server
    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
    printf("MD5 recieved - %s\n", buf);
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
    return 0;
}
