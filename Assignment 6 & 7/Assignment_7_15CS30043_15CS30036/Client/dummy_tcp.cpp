#include "dummy_tcp.h"
#include <arpa/inet.h>
#include <semaphore.h>


dummy_tcp::dummy_tcp(int sockfd, struct sockaddr_in* serveraddr = NULL)
{
	if(serveraddr != NULL)
		{
			this->addr = *serveraddr;
			this->addrlen = sizeof(this->addr);
			cout << inet_ntoa(this->addr.sin_addr) << endl;
		}
	
	
	this->base_ptr = -1;
	this->cur_ptr = -1;
	this->ssthresh = 10*MSS;
	this->sockfd = sockfd;
	this->excepted_seq_no = 0;

	struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(this->sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
    }

    drop = 0;

	cwnd = 2*MSS;
	rwnd = MAX_BUFFERSIZE;
	swnd = min(cwnd,rwnd);
	wndptr = 0; 

	sem_init(&send,0,1);
	sem_init(&recieve,0,1);
	sem_init(&update,0,1);

	pthread_create(&sender,NULL,&dummy_tcp::flow_control_helper,this);
	pthread_create(&reciever,NULL, &dummy_tcp::udp_recieve_helper, this);

}



void* dummy_tcp::udp_recieve(void* arg)
{
	char data[MSS];
	int n;
	packet_t* packet;
	struct sockaddr_in cli;
	socklen_t clilen;
	clilen = sizeof(cli);
	while(1){
		bzero(data,MSS);
		n = recvfrom(sockfd,data,MSS,0,(struct sockaddr *) &cli, &clilen);
		if(n < 0)
		{
			//Timeout
			//cout << inet_ntoa(this->addr.sin_addr) << endl; 
			//cout << "packet not recieved" << endl;
			sem_wait(&update);
			this->ssthresh = max(MSS, this->ssthresh/2);
			cwnd = MSS;
			swnd = min(cwnd, rwnd);
			sem_post(&update);
			continue;
		}
		//cout << "packet recieved" << endl;
		this->addr = cli;
		this->addrlen = clilen;
		packet = (packet_t*)data;
		// for(int i = 0; i < packet->header.length; i++)
		// 	cout << packet->data[i];

		// srand(time(NULL));
		// int r = rand()/RAND_MAX;
		// if(drop == 0 && r < 0.5 && packet->header.seq != 0)
		// {
		// 	cout << "Dropped" << endl;
		// 	drop++;
		// 	continue;
		// }
		// cout << "Sequence number: " << packet->header.seq;
		// cout << " Packet length: " << packet->header.length << endl;
		parse_packets(packet);
	}
}

void* dummy_tcp::parse_packets(packet_t *packet)
{
	//cout << packet->header.seq << endl;
	if(packet->header.type == true)
	{
		//cout << "Ack" << endl;
		update_window(packet);
	}
	else
	{
		reciever_buffer_handler(packet);		
	}
}




void* dummy_tcp::reciever_buffer_handler(packet_t *packet)
{
	if(packet->header.seq == excepted_seq_no)
		{
			while(packet->header.length > (MAX_BUFFERSIZE-size));
			//acquire lock
			sem_wait(&recieve);
			for(int i=0; i<packet->header.length; i++)
			{	
				reciever_data_buffer.push_back(packet->data[i]);
				//cout << reciever_data_buffer.back();

			}
			excepted_seq_no+=packet->header.length;
			//cout << excepted_seq_no << endl;
			//release lock
			send_ack(excepted_seq_no-1);
			sem_post(&recieve);
			packet_t tmp;
			while(reciever_packet_queue.find(excepted_seq_no)!=reciever_packet_queue.end())
			{
				//cout << "Here" << endl;
				tmp = reciever_packet_queue[excepted_seq_no];
				while(tmp.header.length > (MAX_BUFFERSIZE-size));
				//acquire lock
				sem_wait(&recieve);
				for(int i=0; i<tmp.header.length; i++)
					reciever_data_buffer.push_back(tmp.data[i]);
				reciever_packet_queue.erase(excepted_seq_no);
				excepted_seq_no+=tmp.header.length;
				size+=tmp.header.length;
				//release lock
				sem_post(&recieve);
				send_ack(excepted_seq_no-1);
			}
		}
	else if(packet->header.seq < excepted_seq_no)
	{
		send_ack(excepted_seq_no-1);
	}
	else
	{
		reciever_packet_queue[packet->header.seq] = *packet;
		send_ack(excepted_seq_no-1);
	}
}

void dummy_tcp::update_window(packet_t* packet)
{
	//cout << "Here" << endl;
	sem_wait(&update);
	//cout << "Here" << endl;
	this->rwnd = packet->header.rw;
	int tempack = packet->header.seq;
	//cout << tempack << endl;
	if(tempack == this->cur_ptr){
		//acquire lock
		this->dupack++;
		if(this->dupack >=3){
			this->ssthresh = max(this->ssthresh/2,MSS);
			this->cwnd = this->ssthresh;
			this->dupack = 0;
			// this->swnd = min(this->rwnd,this->cwnd);
			// this->base_ptr = this->cur_ptr+this->swnd;
		}
		//release lock
		sem_post(&update);
	}
	else if(tempack > cur_ptr){
		//acquire lock
		sem_wait(&send);
		this->dupack = 0;
		//Pops the data from sender buffer which has been ackd by the reciever.
		int temp = tempack-this->cur_ptr;
		for(int i=0;i<temp;i++)
		{
			sender_data_buffer.pop_front();
			//cout << "popped" << endl;
			this->wndptr -- ;
		}
		this->cur_ptr = tempack;
		if(this->cur_ptr > this->base_ptr)
			this->base_ptr = this->cur_ptr;

		if(this->cwnd < this->ssthresh){
			this->cwnd += MSS;
		}
		else
		{
			this->cwnd += (MSS*MSS/this->cwnd);
		}
		//release lock
		sem_post(&send);
	}		
	sem_post(&update);

}

void* dummy_tcp::send_ack(int ackno)
{	
	bzero(data,MSS);
	header_t* packet = (header_t*)data;
	packet->seq = ackno;
	packet->type = true;
	packet->rw = MAX_BUFFERSIZE - reciever_data_buffer.size();
	int n = sendto(sockfd,data,MSS,0,(struct sockaddr *)&addr, addrlen);
	if(n<0)
	{
		cout << "Ack not sent" << endl;
	}
}	


/*Sender modules*/

int dummy_tcp::app_send(char* data,int len)
{
	while(!sender_buffer_handler(data,len));
	return len;
}

int dummy_tcp::app_recieve(char* buf, int len){
	int read = 0;
	while(1)
	{
		sem_wait(&recieve);
		while(!reciever_data_buffer.empty() && read < len)
		{
			buf[read] = reciever_data_buffer.front();
			//cout << buf[read];
			read++;
			reciever_data_buffer.pop_front();
		}
		sem_post(&recieve);
		if(read == len)
			break;

	}
	return read;
}

int dummy_tcp::sender_buffer_handler(char* data, int len)
{
	//cout<<"In handler_control"<<endl;
	if(this->sender_data_buffer.size()+len < MAX_BUFFERSIZE)
	{
		//acquire lock
		sem_wait(&send);
		for(int i = 0 ; i < len ; i ++)
		{
			this->sender_data_buffer.push_back(data[i]);
		}
		//release lock
		//cout<<"Pushed - "<<len<<endl;
		sem_post(&send);
		return 1;
	}
	else 
		return 0;
}

void* dummy_tcp::flow_control()
{
	while(1){
	while(sender_data_buffer.size() == 0);
	//cout<<"In flow_control"<<endl;
	sem_wait(&update);
	sem_wait(&send);
	swnd = min(cwnd,rwnd);
	//cout<<"swnd "<<swnd<<" "<<this->base_ptr<<" "<<this->cur_ptr<<" "<<wndptr<<endl;
	if((this->base_ptr - this->cur_ptr) <  swnd)
	{
		
		while(swnd != (this->base_ptr - this->cur_ptr) && wndptr!= (sender_data_buffer.size()))
		{
			//cout << this->base_ptr << "::" << this->cur_ptr << "::" << wndptr << endl;
			//cout << "CWND-" << cwnd << " RWND-" << rwnd << " SWND-" << swnd << endl;
 			bzero(data, MSS);
			int totake = swnd - (this->base_ptr - this->cur_ptr);
			totake = min(totake,(int)sender_data_buffer.size() - wndptr);
			totake = min(totake, MSS-header_size);
			//cout << totake << endl;
			int seq = base_ptr+1;
			char *data = new char[totake];
			for(int i=0; i < totake; i++)
			{
					data[i] = sender_data_buffer[wndptr];
					wndptr++;
					this->base_ptr++;
			}
			create_packet(data,totake,seq);
			
		}
	}
	else if((this->base_ptr - this->cur_ptr) > swnd){
		// cout << "Here" << endl;
		wndptr -= (this->base_ptr - this->cur_ptr);
		this->base_ptr = this->cur_ptr;
	}

	sem_post(&update);
	sem_post(&send);
}


}

void* dummy_tcp::create_packet(char* buf, int len, int seq)
{
	char* segment = new char[MSS];
	bzero(segment,MSS);
	header_t* header = (header_t*)segment;
	header->seq = seq;
	// cout << "Seq no: " << seq << " ";
	header->type = false;
	header->length = len;
	// cout << "Length of packet: "<< header->length << endl;
	header->rw = this->rwnd;
	packet_t* packet = (packet_t*)segment;
	bcopy(buf,packet->data,len);
	udp_send(segment,len);
}

int dummy_tcp::udp_send(char* packet,int len)
{
	int n=0;
	packet_t* seg = (packet_t*)packet;
	// cout << "-----------------------" << endl;
	// for(int i = 0; i < seg->header.length; i++)
	// 	cout << seg->data[i];
	// cout << "-----------------------" << endl;
	n = sendto(this->sockfd,packet,MSS,0,(struct sockaddr*)&addr,addrlen);
	if(n<0)
	{
		cout << inet_ntoa(this->addr.sin_addr) << endl; 
		cout << "Packet not sent" << endl;
		perror((char *)"Hello");
	}
	return 1;
}


