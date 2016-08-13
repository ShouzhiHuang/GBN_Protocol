/*
 * File:   client.c
 * Author: Frank
 *
 * Created on 2015年11月18日, 下午8:03
 */

#include <stdio.h>                  /*for standard I/O functions*/
#include <stdlib.h>                  /*for exit*/
#include <string.h>                  /*for memset,memcpy,and strlen*/
#include <netdb.h>                 /*for struct hostent and gethostbyname*/
#include <sys/socket.h>             /*for socket,sendto,and recvfrom*/
#include <netinet/in.h>            /*for sockaddr_in*/
#include <unistd.h>              /*for close*/
#include <fcntl.h>                /*for fcntl*/
#include <sys/time.h>             /*for gettimeofday*/
#include <unistd.h>

#define STRING_SIZE 1024
#define DATASIZE 81
#define MAXSEQ 16
#define SERV_UDP_PORT 65100
/*
 *
 */
struct packet    //packet format
{
    short Count;
    short Seq;
    char data[DATASIZE];
};
struct window            /*sliding window*/
{
    short seq;
    char data[DATASIZE];
    struct window * next;
};
/*-------------------------------------------------------------------------------------------------*/
int sock_client;   /*Socket used by client*/
struct sockaddr_in client_addr;   /*Internet address structure that stores client address*/
unsigned short client_port; /*Port number used by client (local port)*/
struct sockaddr_in server_addr;/*Internet address structure that stores server address*/
struct hostent *server_hp;/* structure to stores server's IP address*/
char server_hostname[STRING_SIZE];/*Server's hostname*/
unsigned short server_port; /*Port number used by server (remote port)*/
//struct packet send_p; /*sent packet*/
unsigned int msg_len; /*length of message */
struct packet end;     /*end of transmission*/
int bytes_sent,bytes_recd; /*# of bytes sent or received*/
int fcntl_flags;/*flag used by the fcntl function to set socket for non-blocking operation*/
/*-------------------------------------------------------------------------------------------------*/
/*------------------------configuration--------------------------------*/
int Size;/*window size*/
long Timeout;/* microseconds*/
short ack;/*store net bytes ack*/
short ACK;/*store host bytes and valid ACK*/
/*-------------------------------------------------------------------------------------------------*/
FILE * position;             /*store the position of file pointer*/
struct window *wind_nextseqnum;      /*keep the position of nextseqnum when reach rear it lose fuction*/
struct window *wind_base;              /*keep the position of baseseq */
struct window *rear;            /*store the position of rear of the window*/
short nextseqnum=0;            /*record next seqnum*/
struct timeval send_t;    /*current system time*/
struct timeval check_t;    /*estimated expired time*/
int wind_counter=0;             /*count for the size of the window*/
unsigned char send_buffer[sizeof(end)]; /*convert structure to network bytes*/
int total_retrans=0;         /*Total number of retransmissions*/
long total_bytes_n=0;        /*Total number of data bytes transmitted (this should be the sum of the count fields of all transmitted
                              packets when transmitted for the first time only)*/
int total_packet_n=0;       /*Number of data packets transmitted (initial transmission only)*/
int total_packet_d=0;       /*Total number of data packets transmitted (initial transmissions plus retransmissions)*/
int total_ack=0;             /*Number of ACKs received*/
int total_timeout=0;         /*Count of how many times timeout expired*/
struct packet Send;
/*-------------------------------------------------------------------------------------------------*/
struct packet make_pkt(struct window * buffer); /*get packet from window*/
int rdt_rcv();                               /*receive ACK*/
int packet_send(struct packet send);         /*send packet*/
struct window* create_window(int size);     /*create window*/
int rtd_send();                               /*read file and put message into window*/
void packet_resend();                   /*resend packet*/
void move_base();                       /*slide window*/
int isTimeout(struct timeval a,struct timeval b); /*check if there has timeout*/
void convertT();                       /*convert Timeout to microsecond*/
void packet_resend_l(struct packet resend); /*resend packet when the window size is 1*/
int rdt_rcv_l();/*receive ACK when the window size is 1*/
/*-------------------------------------------------------------------------------------------------*/
int main(void) {
    /*open a socket*/
    if ((sock_client = socket (PF_INET,SOCK_DGRAM,IPPROTO_UDP))<0){
        perror("Client:can't open datagram socket\n");
        exit(1);
    }
       /* Note: there is no need to initialize local client address information
            unless you want to specify a specific local port.
            The local address initialization and binding is done automatically
            when the sendto function is called later, if the socket has not
            already been bound. 
            The code below illustrates how to initialize and bind to a
            specific local port, if that is desired. */

   /* initialize client address information */

   client_port = 0;   /* This allows choice of any available local port */

   /* Uncomment the lines below if you want to specify a particular 
             local port: */
   /*
   printf("Enter port number for client: ");
   scanf("%hu", &client_port);
   */

   /* clear client address structure and initialize with client address */
   memset(&client_addr, 0, sizeof(client_addr));
   client_addr.sin_family = AF_INET;
   client_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* This allows choice of
                                        any host interface, if more than one 
                                        are present */
   client_addr.sin_port = htons(client_port);

   /* bind the socket to the local client port */
   if (bind(sock_client, (struct sockaddr *) &client_addr,sizeof (client_addr)) < 0) {
      perror("Client: can't bind to local address\n");
      close(sock_client);
      exit(1);
   }
    /*make socket non-blocking */
    fcntl_flags= fcntl(sock_client,F_GETFL,0);
    fcntl(sock_client,F_SETFL,fcntl_flags | O_NONBLOCK);
    /*initialize server address  information*/
    //printf("Enter hostname of server:");
    //scanf("%s", server_hostname);
    memcpy(server_hostname,"localhost",STRING_SIZE);
    if((server_hp=gethostbyname(server_hostname))==NULL){
        perror("Client:invlid server hostname \n");
        close(sock_client);
        exit(1);
    }
    // printf("Enter port number of server:");
    // scanf("%hu",&server_port);
    server_port=SERV_UDP_PORT;
    /*clear server address structure amd initialize with server address*/
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    memcpy((char*)&server_addr.sin_addr,server_hp->h_addr,server_hp->h_length);
    server_addr.sin_port=htons(server_port);
    /*----------------------------------------------------------------------------------------------*/
    position=fopen("test1.txt","r"); /*open a file and keep the position of the file pointer*/
    if(position==NULL){
        printf("fail to open file...\n");
    }
    /*----------------------------------------------------------------------------------------------*/
    /*user interface*/
    /*input(configuration)*/
    printf("please enter configuration:\n");
    printf("Window Size:");scanf("%d",&Size);                  /*enter the sliding window size*/
    printf("Timeout:");scanf("%d",&Timeout);                     /*enter the timeout interval*/
    convertT();   /*convert to microsecond*/
    /*----------------------------------------------------------------------------------------------*/
    /*send message*/
    
    
    if(Size==1){
        memset(&Send,0,sizeof(Send));
        fgets(Send.data,DATASIZE,position);
        Send.Count=strlen(Send.data);
        Send.Seq=nextseqnum;
        packet_send(Send);
        printf("Packet %d transmitted with %lu data bytes\n",Send.Seq,Send.Count);
        total_bytes_n=total_bytes_n+Send.Count;
        total_packet_n++;
        total_packet_d++;
        nextseqnum=(nextseqnum+1)%MAXSEQ;
         gettimeofday(&send_t,NULL);
        while(!feof(position)||!rdt_rcv_l()){
            if(rdt_rcv_l()&&!feof(position)){
                memset(&Send,0,sizeof(Send));
                fgets(Send.data,DATASIZE,position);
                Send.Count=strlen(Send.data);
                Send.Seq=nextseqnum;
                packet_send(Send);
                printf("Packet %d transmitted with %lu data bytes\n",Send.Seq,Send.Count);
                total_bytes_n=total_bytes_n+Send.Count;
                total_packet_n++;
                total_packet_d++;
                nextseqnum=(nextseqnum+1)%MAXSEQ;
                gettimeofday(&send_t,NULL);
            }
            gettimeofday(&check_t,NULL);
            if(isTimeout(check_t,send_t)){
                printf("Timeout expired for packet numbered %d\n",Send.Seq);
                total_timeout++;
                packet_send(Send);
                printf("Packet %d retransmitted with %d data bytes\n",Send.Seq,Send.Count);
                total_retrans++;
                total_packet_d++;
            }
        } 
    }
    else{
    wind_nextseqnum=wind_base=create_window(Size);/*initialize window*/
    nextseqnum=wind_nextseqnum->seq;
    while(wind_counter!=0){
        if((nextseqnum!=(rear->seq+1)%MAXSEQ)){
            if((wind_nextseqnum->next!=NULL)&&(nextseqnum!=wind_nextseqnum->seq)){
                wind_nextseqnum=wind_nextseqnum->next;
            }
            if((wind_nextseqnum->seq==nextseqnum)&&packet_send(make_pkt(wind_nextseqnum))){
                printf("Packet %d transmitted with %lu data bytes\n",wind_nextseqnum->seq,strlen(wind_nextseqnum->data));
                total_packet_n++;
                total_packet_d++;
                total_bytes_n=total_bytes_n+strlen(wind_nextseqnum->data);
                if(wind_nextseqnum->next!=NULL){
                    wind_nextseqnum=wind_nextseqnum->next;
                    nextseqnum=wind_nextseqnum->seq;
                }
                else{
                    nextseqnum=(nextseqnum+1)%MAXSEQ;
                }
            }
        }
        if(rdt_rcv()==1){
            move_base();
            if(wind_counter==0){;break;}
            gettimeofday(&send_t,NULL);
        }
        gettimeofday(&check_t,NULL);
        if(isTimeout(check_t,send_t)&&(wind_counter!=0))
        {
            printf("Timeout expired for packet numbered %d\n",wind_base->seq);
            total_timeout++;
            packet_resend();
        }
            
        rtd_send();
    }
    }
    end.Count=0;
    end.Seq=nextseqnum;
    memcpy(end.data,"EOT",DATASIZE);
    packet_send(end);
    printf("End of Transmission Packet with sequence number %d transmitted with %d data bytes\n",end.Seq,ntohs(end.Count));
    /*-----------------------------------------------------------------------------------------------*/
    /*output at the end of transmission*/
    printf("Number of data packets transmitted:%d\n",total_packet_n);                       /*total # of packets not include duplicate*/
    printf("Total number of data bytes transmitted:%ld\n",total_bytes_n);                    /*not include duplicate*/
    printf("Total number of retransmissions:%d\n",total_retrans);
    printf("Total number of data packets transmitted:%d\n",total_packet_d);                   /*include duplicate*/
    printf("Number of ACKs received:%d\n",total_ack);
    printf("Count of how many times timeout expired:%d\n",total_timeout);

    fclose(position);
    close(sock_client);
    return (EXIT_SUCCESS);
}
/*-------------------------------------------------------------------------------------------------*/
struct packet make_pkt(struct window * buffer){  /*form packet from window*/
    struct packet temp;
    memcpy(temp.data,buffer->data,DATASIZE);
    temp.Seq=buffer->seq;
    temp.Count=strlen(temp.data);
    return temp;
}
/*-------------------------------------------------------------------------------------------------*/
int rdt_rcv(){              /*receive ack and decide if the packet has been correctly received*/
    short a;
    struct window * p;
    p=wind_base;
    bytes_recd=recvfrom(sock_client,&ack,sizeof(a),0,(struct sockaddr *)0,(int *)0);
    if(bytes_recd>0){
        a=ntohs(ack);
        printf("ACK %d received\n",a);
        total_ack++;
            while(p!=NULL){
                if(a==p->seq){
                    ACK=a;
                    return 1;
                }
                p=p->next;
            }
        return 0;
    }
    else
    {
        return 0;
    }
}
int rdt_rcv_l(){
    short a;
    bytes_recd=recvfrom(sock_client,&ack,sizeof(a),0,(struct sockaddr *)0,(int *)0);
    if(bytes_recd>0){
        a=ntohs(ack);
        printf("ACK %d received\n",a);
        total_ack++;
        if(a==Send.Seq){
            return 1;
        }
        else{
            return 0;
        }
    }
    else
    {
        return 0;
    }
}
/*-------------------------------------------------------------------------------------------------*/
int packet_send(struct packet send){      /*send packet*/
    struct packet p;
    p.Count=htons(send.Count);
    p.Seq=htons(send.Seq);
    memcpy(p.data,send.data,strlen(send.data)+1);
    memset(send_buffer,0,sizeof(send));         /* clean send buffer */
    memcpy(send_buffer,&p,sizeof(send));  /* convert structrue to network bytes */
    bytes_sent=sendto(sock_client,send_buffer,sizeof(send_buffer),0,(struct sockaddr*)&server_addr,sizeof(server_addr));
    
    if((bytes_sent>0)&&(wind_counter!=0)){
        if(send.Seq==wind_base->seq){
            gettimeofday(&send_t,NULL);
        }
        return 1;
    }
    else{
        return 0;
    }
}
/*-------------------------------------------------------------------------------------------------*/
void packet_resend_l(struct packet resend){
     packet_send(resend);
     printf("Packet %d retransmitted with %ld data bytes\n",resend.Seq,resend.Count);
     total_packet_d++;
     total_retrans++;
     if(rdt_rcv_l()&&!feof(position)){
         memset(&Send,0,sizeof(Send));
                fgets(Send.data,DATASIZE,position);
                Send.Count=strlen(Send.data);
                Send.Seq=nextseqnum;
                packet_send(Send);
                total_packet_n++;
                total_packet_d++;
                nextseqnum=(nextseqnum+1)%MAXSEQ;
        gettimeofday(&send_t,NULL);
    }
    gettimeofday(&check_t,NULL);
    if(isTimeout(check_t,send_t)){
        total_timeout++;
        printf("Timeout expired for packet numbered %d\n",Send.Seq);
        packet_resend_l(Send);
        }
}
/*-------------------------------------------------------------------------------------------------*/
struct window* create_window(int size){            /*create window*/
    struct window *head, *p1,*p2;
    int n=0;
    p1=p2=(struct window *)malloc(sizeof(struct window));
    while(n<size)
    {
        if(n==0) {
            head=p1;    /*recording the head of the window*/
            fgets(head->data,DATASIZE,position);/*read a line into data array*/
            head->seq=n;
        }
        else {
            p1=(struct window *)malloc(sizeof(struct window)); /*creating new node*/
            fgets(p1->data,DATASIZE,position);/*read a line into data array*/
            p1->seq=n;
            p2->next=p1;   /*connecting nodes*/
            p2=p1;    /*recording the current rear of the window*/
        }
        
        n++;
    }
    wind_counter=n;
    p2->next=NULL;
    rear=p2;
    return (head);
}
/*-------------------------------------------------------------------------------------------------*/
int rtd_send(){           /*add new packet to window*/
    if(feof(position)){
        return 0;
    }
    else{
        while((wind_counter<Size)&&(rear!=NULL)){
            struct window *p0;
            p0=(struct window *)malloc(sizeof(struct window));        /*get message*/
            p0->next=NULL;
            fgets(p0->data,DATASIZE,position);
            p0->seq=(rear->seq+1)%MAXSEQ;
            rear->next=p0;
            rear=p0;
            wind_counter++;
            printf("window counter :%d\n",wind_counter);
        }
        return 1;
    }
}
/*-------------------------------------------------------------------------------------------------*/
void packet_resend(){
    struct window *p;
    p=wind_base;
    while(p!=NULL&&p->seq!=16){
        packet_send(make_pkt(p));
        printf("Packet %d retransmitted with %ld data bytes\n",p->seq,strlen(p->data));
        total_packet_d++;
        total_retrans++;
        if(rdt_rcv()){
            move_base();
            gettimeofday(&send_t,NULL);
        }
        gettimeofday(&check_t,NULL);
        if(isTimeout(check_t,send_t))
        {
            total_timeout++;
            printf("Timeout expired for packet numbered %d\n",wind_base->seq);
            packet_resend();
        }
        p=p->next;
    }
    
}
/*-------------------------------------------------------------------------------------------------*/
void move_base(){
    if(wind_counter>0){
       while((wind_counter!=0)&&(wind_base->seq!=(ACK+1)%MAXSEQ)){
            struct window *p0;
            p0=wind_base;
            if(wind_base->next!=NULL){
                wind_base=wind_base->next;
            }
            free(p0);
            wind_counter--;
            printf("window counter :%d\n",wind_counter);
        }
    }
}
/*-------------------------------------------------------------------------------------------------*/
int isTimeout(struct timeval a,struct timeval b){   /*a is checking time, b is sent time*/
    long c;
    c=a.tv_sec-b.tv_sec;
    c=c*1000000;
    if(a.tv_usec<b.tv_usec){
        c=1000000-b.tv_usec+a.tv_usec;
    }
    else{
        c=c+b.tv_usec-a.tv_usec;
    }
    if(c>=Timeout){
        return 1;
    }
    else{
        return 0;
    }
}
/*-------------------------------------------------------------------------------------------------*/
void convertT(){
    int i=0;
    int temp;
    temp=Timeout;
    while(i<temp){
        Timeout=Timeout*10;
        i++;
    }
}