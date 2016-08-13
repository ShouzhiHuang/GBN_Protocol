/* 
 * File:   server.c
 * Author: Frank
 *
 * Created on 2015年11月18日, 下午8:03
 */

#include <ctype.h>                  /*for tourpper*/
#include <stdio.h>                  /*for standard I/O functions*/
#include <stdlib.h>                  /*for exit*/
#include <string.h>                  /*for memset,memcpy,and strlen*/
#include <netdb.h>                 /*for struct hostent and gethostbyname*/
#include <sys/socket.h>             /*for socket,sendto,and recvfrom*/
#include <netinet/in.h>            /*for sockaddr_in*/
#include <unistd.h>              /*for close*/
#include <time.h>                  /*for timeval*/

#define STRING_SIZE 1024
#define SERV_UDP_PORT 65100
#define DATASIZE 81
#define MAXSEQ 16
/*
 * 
 */
struct packet    //packet format
{
    short Count;
    short Seq;
    char data[DATASIZE];
};
short ack_seq=-1;                          /*ack# for correct received packet(in sequance) in host bytes*/
/*--------------------------------------------------------------------------------------------------*/
float P_Loss_rate;/* packet loss rate 0-1*/
int P_Delay;/*1 is delay 0 is not*/
float A_Loss_rate;/* ACK loss rate 0-1*/
struct timeval current_t; /*get current time*/
/*--------------------------------------------------------------------------------------------------*/
int Simulate();             /*simulate packet delay,loss or error*/
int ACKsimulate();        /*simulate ACK loss */
/*--------------------------------------------------------------------------------------------------*/
int main(void) {
    int sock_server;    /*Socket on which server listens to clients*/
    struct sockaddr_in server_addr; /*Internet address structure that stores server address*/
    unsigned short server_port; /*Port number used by server (local port)*/
    struct sockaddr_in client_addr;   /*stores client address */
    unsigned int client_addr_len; /*Length of client address structure */
    struct packet recd_p;  /*recieved packet*/
    unsigned int msg_len;  /*length of message*/
    int bytes_sent, bytes_recd; /*# of bytes sent or received*/
    unsigned char recd_buffer[sizeof(recd_p)]; /*convert structure to network bytes*/
    long i; /*temporary loop variable*/
    float j; /*store random number*/
    int simulate;/*store result of simulate function*/
    int acksimulate;/*store result of ACKsimulate function*/
    int total_packet_n=0;/*Number of data packets received successfully (without loss and not including duplicates)*/
    long total_bytes_n=0;/*Total number of data bytes received which are delivered to user (this should be the sum of the count
                                   fields of all packets received successfully without loss and without duplicates)*/
    int total_packet_d=0; /*Total number of duplicate data packets received (without loss)*/
    int total_packet_l=0;/*Number of data packets received but dropped due to loss*/
    int total_packet=0;/*inluding every packets*/
    int total_ack_n=0;/*Number of ACKs transmitted without loss*/
    int total_ack_d=0;/*Number of ACKs generated but dropped due to loss*/
    int total_ack=0;/*Total number of ACKs generated (with and without loss)*/
    /*----------------------------------------------------------------------------------*/
    /*open a socket*/
    if ((sock_server=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP))<0){
        perror("server:can't open datagram socket\n");
        exit(1);
    }
    /*initialize server address information*/
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);/*this allows choice of any host interface, if more than one are present*/
    server_port=SERV_UDP_PORT;  /*Server will listen on this port*/
    server_addr.sin_port=htons(server_port);
    /*bind socket to then local server port*/
    if (bind(sock_server,(struct sockaddr*)&server_addr, sizeof(server_addr))<0){
        perror("Server:can't bind to local address\n");
        close(sock_server);
        exit(1);
    }
    /*------------------------------------------------------------------------------------------------------*/
    /*input(configuration)*/
    printf("please enter configuration:\n");
    printf("Packet Loss Rate:");scanf("%f",&P_Loss_rate);
    printf("Packet Delay:");scanf("%d",&P_Delay);
    printf("ACK Loss Rate:");scanf("%f",&A_Loss_rate);
    /*wait for incoming messages in an indefinte loop*/
    printf("Waiting for incoming messages on port %hu\n\n",server_port);
    client_addr_len=sizeof(client_addr);
    /*------------------------------------------------------------------------------------------------------*/

    FILE * fp;
    fp=fopen("output.txt","wt");
    if(fp==NULL){
        printf("fail to open file...\n");
        exit(1);
    }
    for(;;){
        memset(recd_buffer,0,sizeof(recd_buffer));
        bytes_recd=recvfrom(sock_server,recd_buffer,STRING_SIZE,0,(struct sockaddr *)&client_addr, &client_addr_len);
        if(bytes_recd>0){
            memset(&recd_p,0,sizeof(recd_p));                        /* clean the structrue */
            memcpy(&recd_p,recd_buffer,sizeof(recd_buffer));
            if(ntohs(recd_p.Count)==0){
                printf("End of Transmission Packet with sequence number %d received with %d data bytes\n",ntohs(recd_p.Seq),ntohs(recd_p.Count));
                break;
            }
            simulate=Simulate();
            if(simulate==0){
                printf("Packet %d lost\n",ntohs(recd_p.Seq));
                total_packet_l++;
                total_packet++;
                continue;
            }
            else if(simulate==-1){
                srand((unsigned int)time(NULL));
                j=rand();
                j=j/32767.0;
                j=j*1000;
                i=0;
                while(i<j){
                    i++;
                }
                if((ack_seq+1)%MAXSEQ!=ntohs(recd_p.Seq)){
                    
                    printf("Duplicate packet %d received with %d data bytes\n",ntohs(recd_p.Seq),ntohs(recd_p.Count));
                    total_packet_d++;
                    total_packet++;
                }
                else{
                    printf("Packet %d received with %d data bytes\n",ntohs(recd_p.Seq),ntohs(recd_p.Count));
                    printf("%s\n",recd_p.data);
                    fputs(recd_p.data,fp);
                    ack_seq=ntohs(recd_p.Seq);
                    printf("Packet %d delivered to user\n",ack_seq);
                    total_packet_n++;
                    total_bytes_n=total_bytes_n+ntohs(recd_p.Count);
                    total_packet++;
                }
            }
            else{
                if((ack_seq+1)%MAXSEQ!=ntohs(recd_p.Seq)){
                    
                    printf("Duplicate packet %d received with %d data bytes\n",ntohs(recd_p.Seq),ntohs(recd_p.Count));
                    total_packet_d++;
                    total_packet++;
                }
                else{
                    printf("Packet %d received with %d data bytes\n",ntohs(recd_p.Seq),ntohs(recd_p.Count));
                    printf("%s\n",recd_p.data);
                    fputs(recd_p.data,fp);
                    ack_seq=ntohs(recd_p.Seq);
                    printf("Packet %d delivered to user\n",ack_seq);
                    total_packet_n++;
                    total_bytes_n=total_bytes_n+ntohs(recd_p.Count);
                    total_packet++;
                }
            }
        }
    /*send message*/
        if(bytes_recd>0){
            acksimulate=ACKsimulate();
            if(simulate!=0){
                if(ACKsimulate()){
                    short ack;
                    ack=htons(ack_seq);
                    bytes_sent=sendto(sock_server,&ack,sizeof(ack),0,(struct sockaddr*)&client_addr,client_addr_len);
                    if(bytes_sent>0){
                        printf("ACK %d transmitted\n",ack_seq);
                        total_ack_n++;
                        total_ack++;
                    }
                }
                else{
                    printf("ACK %d lost\n",ack_seq);
                    total_ack_d++;
                    total_ack++;
                }
            }
        }
    }
    /*----------------------------------------------------------------------------------------------------------*/
    /*output at the end of transmission*/
    printf("Number of data packets received successfully:%d\n",total_packet_n);         /*without loss and not including duplicates*/
    printf("Total number of data bytes received which are delivered to user:%d\n",total_bytes_n);/* without loss and without duplicates*/
    printf("Total number of duplicate data packets received:%d\n",total_packet_d);  /*without loss*/
    printf("Number of data packets received but dropped due to loss:%d\n",total_packet_l);
    printf("Total number of data packets received:%d\n",total_packet);/*including those that were successful, those lost, and duplicates*/
    printf("Number of ACKs transmitted without loss:%d\n",total_ack_n);
    printf("Number of ACKs generated but dropped due to loss:%d\n",total_ack_d);
    printf("Total number of ACKs generated:%d\n",total_ack);/*with and without loss*/
    fclose(fp);
    close(sock_server);
    return (EXIT_SUCCESS);
}
/*--------------------------------------------------------------------------------------------------*/
int Simulate(){                                  /*simulate packet delay and loss or error*/
    float r;  
    gettimeofday(&current_t,NULL);   
    srand(current_t.tv_usec);
    r=rand()/(RAND_MAX+1.0); 
    printf("%f\n",r);
    if(r<P_Loss_rate){
        return 0;                                   /*packet loss or error*/
    }
    else if(P_Delay){
        return -1;                                    /*packet delay*/
    }
    else{
        return 1;                                   /*received correct*/
    }
}
/*--------------------------------------------------------------------------------------------------*/
int ACKsimulate(){                                /*simulate packet delay and loss or error*/ 
    float r;  
    gettimeofday(&current_t,NULL);   
    srand(current_t.tv_usec);
    r=rand()/(RAND_MAX+1.0); 
    printf("%f\n",r);
    if(r<A_Loss_rate){
        return 0;                                   /*packet loss or error*/
    }
    else{
        return 1;                                   /*received correct*/
    }
}