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
#include <bits/stdc++.h>


using namespace std;

#define BUFSIZE 512

void error(string msg) {
  cout << msg << endl;
  exit(1);
}


typedef struct inf{
    string ip;
    int port;
}info;

typedef struct act{
    info caddr;
    time_t lastact;

}actconn;

int main(int argc, char const **argv) {

    int portno;						//To store the listening port no.
    int newfd;	
    int serverfd;					//To store the listener socket fd
    int fdmax;						//Max fd in the collection of file descriptors
    fd_set fdcollect;				//Collection of file descriptors to watch
    fd_set fd_temp;					//Temporary copy to pass to select 
    struct sockaddr_in myaddr;		//My own address
    struct sockaddr_in serveraddr;	//The peer to which I will connect as a client while sending a message
    struct sockaddr_in clientaddr;	//client who connects to me
    struct hostent *server;			
    socklen_t clientlen;
    char message[BUFSIZE];			//Message buffer to send on the TCP connection			
    char msg[BUFSIZE];				//Message read from STDIN
    map<int, actconn> active;		//A map from active sockets to the IP, Port and last activity time info
    map<string,info> userinf;		//A map to store the details of the user from the user_info table
    vector<int> actsocks;			//List of active sockets
    int yes = 1;				
    char* filename = (char*)"user_info.txt";
    int n;				
    timeval tv;						//Timeout for select

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    //Load details from the file
    ifstream in;
    in.open(filename);
    string ne;
    string ip;
    int portnumber;
    info temp;
    while(in >> ne >> ip >> portnumber)
    {
         temp.ip = ip;
         temp.port = portnumber;
         userinf[ne] = temp;
         //cout <<  ne << " " << ip << " " << portnumber << endl;
    }


    //Port number read from command line
    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(1);
	  }
    portno = atoi(argv[1]);

    //Clear fdcollect and fd_temp
    FD_ZERO(&fdcollect);
    FD_ZERO(&fd_temp);

    //Create the listening socket
    if((serverfd = socket(AF_INET, SOCK_STREAM, 0))== -1)
    {
        error("Error in socket call");
    }

    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        error("setsockopt");
    }

    //Bind the listening socket to the given port
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(portno);
    memset(&(myaddr.sin_zero),'\0',8);
    if(bind(serverfd,(struct sockaddr *)&myaddr, sizeof(myaddr))==-1)
    {
        error("Error in Bind");
    }

    //Listen
    if(listen(serverfd,5)==-1)
    {
        error("Error in listen call");
    }

    //Add the listening socket and STDIN to the collection of file descriptors
    FD_SET(serverfd, &fdcollect);
    FD_SET(0,&fdcollect);

    //cout << serverfd << endl;

    //Find the max file descriptor among the ones currently in the collection
    fdmax = max(serverfd,STDIN_FILENO);

    cout << "Chat Server Running......." << endl;
    while(true)
    {
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        fd_temp = fdcollect;
        if(select(fdmax+1,&fd_temp, NULL, NULL, &tv) >= 0)   //Call to select 
        {
            for(int i = 0; i<= fdmax; i++)
            {	
            	//Check if the file descriptor is in the list of active sockets returned by select
                if(FD_ISSET(i,&fd_temp))
                {
                	//Check if the listening socket was chosen by select - This means there is a connnection request to it
                    if(i == serverfd)
                    {
                        clientlen = sizeof(clientaddr);
                        //Accept the incoming connection
                        newfd = accept(serverfd,(struct sockaddr *)&clientaddr, &clientlen);
                        if(newfd < 0)
                        {
                            error("Error on accept");
                        }

                        //Add the new socket to the collection of file descriptors
                        FD_SET(newfd,&fdcollect);

                        //Calculate new fdmax
                        fdmax = max(fdmax, newfd);

                        printf("%s Connected\n", inet_ntoa(clientaddr.sin_addr));
                    }
                    else if(i == 0)//The user wants to send a message(STDIN has data)
                    {
                        bzero(msg, BUFSIZE);
                        //Get the message from STDIN
                        fgets(msg,BUFSIZE,stdin);
                        int length = strlen(msg);
                        int flag = 0;
                        int space = 0;
                        char name[BUFSIZE];
                        int count = 0;

                        //Split the message and the name
                        for(int k = 0; k< length; k++)
                        {
                            if(msg[k] == '/')
                            {
                                space = k+1;
                                flag = 1;
                                break;
                            }
                            else
                            {
                                name[count] = msg[k];
                                count++;
                            }
                        }
                        if(flag == 0)
                        {
                        	//The message format is wrong
                            printf("Wrong message format\n" );
                            continue;
                        }

                        bzero(message, BUFSIZE);
                        
                        //Copy the actual message to a buffer
                        strcpy(message, &msg[space]);
                        name[count] = '\0';
                        string user(name);
                        info sendinfo;

                        //Find the ip and port no. of the user with the given name
                        if(userinf.find(user) != userinf.end())
                        {
                            sendinfo = userinf[user];
                        }
                        else
                        {
                            printf("ERROR: User not registered \n");
                            continue;
                        }
                        
                        map<int,actconn>::iterator iter;
                        flag = 0;
                        actconn temp;
                        int sockfd = -1;
                        //find if there is a connection to the server with an already active socket 
                        for(iter = active.begin(); iter != active.end();iter++)
                        {
                            temp = iter->second;
                            if(temp.caddr.ip == sendinfo.ip && temp.caddr.port == sendinfo.port)
                            {
                                flag = 1;
                                sockfd = iter->first;
                                break;
                            }
                        }

                        //If yes the reuse the active socket to send the message
                        if(flag == 1)
                        {
                            n = send(sockfd,message, BUFSIZE, 0);
                            if(n < 0)
                            {
                                error("Error in send");
                            }
                            //Update the last activity of the socket
                            time(&(active[sockfd].lastact));
                        }
                        else   //Else make a new connection to the peer and then send the message
                        {
                            sockfd = socket(AF_INET, SOCK_STREAM, 0);
                            if(sockfd < 0)
                                error("Error opening socket");

                            //Get hostent by ip
                            const char* hostname = (sendinfo.ip).c_str();
                            server = gethostbyname(hostname);
                            if (server == NULL){
                                printf("Host not found - %s\n",hostname);
                                continue;
                            }
                            bzero((char *) &serveraddr, sizeof(serveraddr));
                            serveraddr.sin_family = AF_INET;
                            bcopy((char *)server->h_addr,(char *)&serveraddr.sin_addr.s_addr, server->h_length);
                            serveraddr.sin_port = htons(sendinfo.port);

                            //Connect to the peer
                            if (connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
                            {
                            	cout << "ERROR connecting to " << user << endl;
                            	fflush(stdout);
                            	continue;
                            }
                            //Add to currently active sockets
                            actsocks.push_back(sockfd);
                            actconn newtemp;
                            newtemp.caddr = sendinfo;
                            time(&newtemp.lastact);
                            active[sockfd] = newtemp;
                            FD_SET(sockfd,&fdcollect);
                            fdmax = max(fdmax, sockfd);

                            //Send message using the newly opened socket
                            n = send(sockfd,message, BUFSIZE, 0);
                            if(n < 0)
                            {
                                error("Error in send");
                            }
                        }
                    }
                    else //Some message has been recieved or connection closed
                    {
                        bzero(message,BUFSIZE);

                        //recieve message
                        n = recv(i,message, BUFSIZE,0); 
                        if(n==0)
                        {
                            //Connection closed from other side
                            close(i);
                            FD_CLR(i,&fdcollect);
                        }
                        else if(n < 0) 
                        {
                            error("Error in recieve");
                        }

                        //Finding the peer's name who sent the message
                        clientlen = sizeof(clientaddr);
                        if((n = getpeername(i,(struct sockaddr *)&clientaddr, &clientlen)) >= 0)
                        {
                        	map<string,info>:: iterator it;
                        	for(it = userinf.begin(); it!= userinf.end(); it++)
                        	{
                        		if((it->second).ip == inet_ntoa(clientaddr.sin_addr))
                        		{
                        			cout << it->first << "- ";
                        			break;
                        		}
                        	}
                        	
                        }
                        printf("%s", message);

                    }
                }
            }
        }

        /*Check if any active socket has timed out and close it accordingly */
        time_t curr;
        int len = actsocks.size();
        map<int, actconn>::iterator it;
        for(int k = 0; k < len; k++)
        {
            time(&curr);
            if(curr - active[actsocks[k]].lastact >= 20)
            {
                //printf("Closing- %d\n", k);
                cout << "Closing active socket to " << active[actsocks[k]].caddr.ip << endl;
                close(actsocks[k]);
                FD_CLR(actsocks[k], &fdcollect);
                it = active.find(actsocks[k]);
                active.erase(it);
                actsocks.erase(actsocks.begin() + k);
            }
        }
    }
    return 0;
}
