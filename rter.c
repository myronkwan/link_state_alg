#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <math.h>
#include "packet.h"
#include "machine.h"

#define MIN(a,b) ((a) < (b) ? a : b)
typedef struct{//struct for costs table and starting node for djkstras
	int *costscol;//address of 2d costs array collumns
	int *costsrow;//address of 2d costs array rows
	int start;//starting node
	int MACHINE_SIZE;//number of machines
	int portnum;//portnumber for thread 2
	int* lc;//least cost array
}costsupdate;

pthread_mutex_t lock;//mutex lock

int checksum(PACKET *pkt){
	char buff=0;
	int i;
	unsigned char * pktptr=pkt;
	pktptr++;//skip checksum bit, which is the first bit of the packet
	for(i=1;i<sizeof(*pkt);++i){
		buff=buff^ *pktptr++;
	}
	return buff;
}
//THREAD 3
void *thread3(void* temp){//receives costsupdate struct
	

	costsupdate *update=(costsupdate *) temp;
	int *costsbegin;//address to beginning of costs array
	int sptset[update->MACHINE_SIZE];
	int i,j,mindist,count;
	int *lcbegin=update->lc;
	int *minindex=update->lc;
	int check;
	costsbegin=update->costscol;
	while(1){
		sleep(7);
		update->lc=lcbegin;

		update->costscol=costsbegin;
		update->costsrow=costsbegin;
		for(i=0;i<update->MACHINE_SIZE;++i){//initialize dist array
			sptset[i]=0;
			if(i==update->start){
				*update->lc=0;
			}
			else
				*update->lc=999;
			update->lc++;	
		}
		
		count=0;
		pthread_mutex_lock(&lock);
		while(count<4){
			update->lc=lcbegin;
			check=0;
			mindist=INFINITY;
			for(i=0;i<update->MACHINE_SIZE;++i){//get min dist vertex not in sptset
				if((*update->lc<mindist)&&(sptset[i]==0)){
					minindex=update->lc;
					mindist=*update->lc;
					check=1;
				}
				update->lc++;
			}
			update->lc=lcbegin;
			if(check==1){//if min dist vertex was selected
				sptset[minindex-lcbegin]=1;//included in sptset
				count++;
				update->costscol=costsbegin+(minindex-lcbegin);
				update->costsrow=update->costscol;
				for(i=0;i< update->MACHINE_SIZE;++i){//update lc to adj vertices
					
					*update->lc=MIN(*update->lc,mindist+*update->costsrow);
					update->costsrow+=update->MACHINE_SIZE;
				
					update->lc++;
				}
	

			}
			
		}
		pthread_mutex_unlock(&lock);		

		
		update->lc=lcbegin;
		printf("lc after update\n");
		for(;update->lc < lcbegin+update->MACHINE_SIZE;++update->lc){
			printf("%d ",*update->lc);
		}
		printf("\n");
		update->lc=lcbegin;
	
	}
}
//THREAD 2
void *thread2(void* temp){
	//SEND IN PORT NUMBER
	//init server socket
	struct sockaddr_in serverAddr, clientAddr;
	struct sockaddr_storage serverStorage;
	socklen_t addr_size, client_addr_size;
	PACKET pkt,ack;
	PACKET *pktptr=&pkt;
	PACKET *ackptr=&ack;
	int sock;
	costsupdate *update=(costsupdate *)temp;
	serverAddr.sin_port = htons(update->portnum);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl (INADDR_ANY);
	memset ((char *)serverAddr.sin_zero, '\0', sizeof (serverAddr.sin_zero));
	addr_size=sizeof(serverStorage);
	int *costsbegin;//address to beginning of costs array
	int i,j;
	//create socket
	if ((sock = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf ("socket error\n");
		return 1;
	}
	// bind
	if (bind (sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0)
	{
		printf ("bind error\n");
		return 1;
	}
	costsbegin=update->costscol;
		

	while(1){//receive update packets
		recvfrom (sock, &pkt, sizeof(pkt),0, (struct sockaddr *)&serverStorage, &addr_size);
		printf("recieveddata: %d %d %d \n",pkt.data[0],pkt.data[1],pkt.data[2]);
		
		//update costs table
		pthread_mutex_lock(&lock);

		update->costscol=costsbegin;
		update->costsrow=costsbegin;
		//update a,b and b,a	
		*(update->costscol+(pkt.data[0]*update->MACHINE_SIZE)+pkt.data[1])=pkt.data[2];
		*(update->costscol+(pkt.data[1]*update->MACHINE_SIZE)+pkt.data[0])=pkt.data[2];
		
		update->costscol=costsbegin;
		update->costsrow=costsbegin;		
			printf("table after received update: \n");		
		for(i=0;i<update->MACHINE_SIZE;++i){
			update->costsrow=update->costscol;
			for(j=0;j<update->MACHINE_SIZE;++j){
				printf("%d ",*update->costsrow);
				update->costsrow+=update->MACHINE_SIZE;
			}
			printf("\n");
			update->costscol++;
		}
		printf("\n");

		pthread_mutex_unlock(&lock);
	}

	return NULL;
}

int main (int argc, char *argv[])
{

	if(argc != 5){
		printf("need arguments: machineid #ofmachines costsfile machinesfile\n");
	}
	FILE *c,*m;
	int i,j,ID=atoi(argv[1]);
	char *mfile,*cfile;
	mfile=argv[4];
	cfile=argv[3];
	
 	int m1=0,m2=0,nc=0;
	int MACHINE_SIZE=atoi(argv[2]);
	machine machines[MACHINE_SIZE];
	int costs[MACHINE_SIZE][MACHINE_SIZE];
	costsupdate update;
	pthread_t uinput;
	
	struct sockaddr_in serverAddr[MACHINE_SIZE];
	socklen_t addr_size[MACHINE_SIZE];
	PACKET pkt,ack;
	PACKET *pktptr=&pkt;
	PACKET *ackptr=&ack;
	struct timeval tv;
	int rv;
	int sock[MACHINE_SIZE];
	int lc[MACHINE_SIZE];
	


	c=fopen(cfile,"r");
	if(c==NULL){
		printf("cant open input file\n");
		exit(0);
	}
	m=fopen(mfile,"r");
	if(m==NULL){
		printf("cant open input file\n");
		exit(0);
	}
	for(i=0;i<MACHINE_SIZE;++i){	
		fscanf(m,"%s %s %d", machines[i].name, machines[i].ip, &machines[i].port);
		//read in costs
		for(j=0;j<MACHINE_SIZE;++j){
			fscanf(c,"%d",&costs[i][j]);
			printf("%d ",costs[i][j]);
		}
		printf("\n");
		
	}
	
	//configure socket addresses for all machines
	for(i=0;i<MACHINE_SIZE;++i){
		serverAddr[i].sin_family=AF_INET;
		serverAddr[i].sin_port=htons(machines[i].port);
		inet_pton(AF_INET,machines[i].ip,&serverAddr[i].sin_addr.s_addr);
		memset(serverAddr[i].sin_zero,'\0',sizeof(serverAddr[i].sin_zero));	
		addr_size[i]=sizeof(serverAddr[i]);
		sock[i]=socket(PF_INET,SOCK_DGRAM,0);
	}


	printf("machineid: %d\n", ID);

	//init costsupdate
	update.costsrow=&costs;
	update.costscol=&costs;
	update.start=ID;
	update.MACHINE_SIZE=MACHINE_SIZE;
	update.portnum=machines[ID].port;
	update.lc=&lc;
	if(pthread_mutex_init(&lock,NULL)!=0){
		printf("mutex init failed\n");
		return 1;
	}
	//START THREAD 2

	if(pthread_create(&uinput,NULL,thread2,&update)){//send costs table for update, will need to mutex lock so no simulatneous access to costs table
		printf("error creating thread\n");
		exit(0);
	}

	//START THREAD 3
	if(pthread_create(&uinput,NULL,thread3,&update)){//dkjistras, send in machine id for starting node
		printf("error creating thread\n");
		exit(0);
	}

	//THREAD 1
	while(1){//keyboard input and update costs

	printf("enter changes: machine2 newcost\n");
		scanf("%d %d",&m2,&nc);//user input
		m1=ID;
		pthread_mutex_lock(&lock);

			
		
		costs[m1][m2]=nc;//update costs table
		costs[m2][m1]=nc;//symmetrical
	
		pthread_mutex_unlock(&lock);
		//create packet
		pkt.data[0]=m1;
		pkt.data[1]=m2;
		pkt.data[2]=nc;
		

		for(i=0;i<MACHINE_SIZE;++i){
			if(i!=ID){
				sendto(sock[i],&pkt,sizeof(pkt),0,(struct sockaddr *)&serverAddr[i],addr_size[i]);
				printf("sent packet to MACHINE: %d\n",i);
			}
		}
		printf("\n updated costs table:\n");
		for(i=0;i<MACHINE_SIZE;++i){	
		//read in costs
			for(j=0;j<MACHINE_SIZE;++j){
				printf("%d ",costs[i][j]);
				}
			printf("\n");
		
		}		

		
		sleep(10);//sleep 10 seconds
		
	}
	
	if(pthread_join(thread2,NULL)){
		printf("error joining threads\n");
		exit(0);
	}

	if(pthread_join(thread3,NULL)){
		printf("error joining threads\n");
		exit(0);
	}


		
	pthread_mutex_destroy(&lock);
	fclose(c);
	fclose(m);
	
	
	return 0;
}
