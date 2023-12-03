#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <math.h>
#include <netdb.h>
#include <regex.h>

#define MAX_CLIENTS 50
#define maxbuffersize 2048
// #define DEBUG

static _Atomic unsigned int cli_count = 0;
static int uid = 10;
int leave_flag = 0;

// Client structure 
typedef struct
{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[12];
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


//We design on how clients are added to the list to chat with others
// SECTION ==Add clients to list
//only one thread can execute the code between the lock and unlock.
void queue_add(client_t *cl)
{
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i)
    {
		if(!clients[i])
        {
			clients[i] = cl;
			break;
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

//We design on how clients are removed after they complete chatting
// SECTION ==Remove clients from the list
void queue_remove(int uid)
{
	pthread_mutex_lock(&clients_mutex);
	for(int i=0; i < MAX_CLIENTS; ++i)
    {
		if(clients[i])
        {
			if(clients[i]->uid == uid)
            {
				clients[i] = NULL;
				break;
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

//design on how to send message to all client, but also not to self, since you the one sending it either way
//amake use of pthreasd
// Send message to all clients except sender
void send_message(char *s, int uid)
{
	pthread_mutex_lock(&clients_mutex);
	for(int i=0; i<MAX_CLIENTS; ++i)
    {
		if(clients[i])
        {
			if(clients[i]->uid != uid)
            {
				if(send(clients[i]->sockfd, s, strlen(s),0) <= 0)
                {
					printf("ERROR: writing to sockedfd  failed");
					fflush(stdout);
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

// Handle all communication with the client 

void *handle_client(void *arg)
{
	char mem1[maxbuffersize];
	int leave_flag = 0;

	cli_count++;
	client_t *cli = (client_t *)arg;

	while(1)
    {
		if (leave_flag) 
        {
			break;
		}
		int receive = recv(cli->sockfd, mem1, maxbuffersize, 0);
		if (receive > 0)
        {
			if(strlen(mem1) > 0)
            {
				//printf("%s %s\n",cli->name,buff_out);
				char message [maxbuffersize]={0};
				char out_message [maxbuffersize]={0};
				char mem1_out [maxbuffersize]={0};
				
				memcpy(message,mem1+3,sizeof(mem1));
				message[strcspn(message, "\n")] = '\0';

				memcpy(mem1_out,mem1+3,sizeof(mem1));
				//message_dispaly[strcspn(message_dispaly, "\n")] = '\0';
				//check rhe ending creterion , either a \n or a space \r
	
					if (mem1_out[receive-2] == 0x0d)
					{
						mem1_out[receive-2] = 0;
					} 
					else if (mem1_out[receive-1] == '\n')
					{
						mem1_out[receive-1] = 0;
					}


				//checking message character length 255 
			  
    		
    			int charcount = strlen(message)-1;
				if(charcount<257)
				{
					sprintf(out_message,"%s %s %s%s","MSG",cli->name,message,"\n");
					printf("%s %s",cli->name,mem1_out);
					fflush(stdout);
					send_message(out_message, cli->uid);
				}
 				else
				{
					sprintf(mem1, "ERROR %s handle , for character \n", cli->name);
					send_message(mem1, cli->name);
					//leave_flag = 1;	
				} 

			}
		} 
        else if (receive == 0)
        {
			//when someone is exit, the server needsto update  person
			sprintf(mem1, "%s left \n", cli->name);
			printf("%s", mem1);
			fflush(stdout);
			
			leave_flag = 1;
		} 
		//check if there is a message on receive
		else if (receive<0)
		{
			sprintf(mem1, "ERROR %s handle , for character, possible no message ---check\n", cli->name);
			//printf("%s", buff_out);
			leave_flag = 1;
		}
        else 
        {
			//check error in the whole communication section
			printf("ERROR: -1\n");
			fflush(stdout);
			leave_flag = 1;
		}
		//clear memory
		bzero(mem1,maxbuffersize);	
	}

  // since its a multiple client chat, when to remove client ffrom ques 
	close(cli->sockfd);
	queue_remove(cli->uid);
	free(cli);
	cli_count--;
	pthread_detach(pthread_self());
	return NULL;
}
// main program starts 

int main(int argc, char **argv)
{
	//check the commandline argurments 
	if(argc != 2)
	{
		printf("ERROR Please enter ip and port in ths order  IP: port\n");
		fflush(stdout);
		exit(0);
	}
	//get the commnadline values
	char delim[]=":";
 	char *Desthost=strtok(argv[1],delim);
  	char *Destport=strtok(NULL,delim);
	int port=atoi(Destport);
	printf("Host %s, and port %d.\n",Desthost,port);
	fflush(stdout);

	// assign IP, PORT
	struct addrinfo hints;
	memset(&hints,0,sizeof(hints));
	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_protocol=0;
	hints.ai_flags=AI_ADDRCONFIG;
	struct addrinfo* res=0;
	int err=getaddrinfo(Desthost,Destport,&hints,&res);
	if (err!=0) 
	{
    		perror("ERROR Failed to resolve remote socket address\n");
			fflush(stderr);
    		exit(0);
	}
	//Initialize variables
	int sockfd;
	int option = 1;
	pthread_t tid;
	int connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	char name[12]={0};
  
	// socket create and verification (server only support IPV4 and TCP )
	sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
	if(sockfd<=0)
	{
		printf("ERROR Server socket creation failed\n");
		fflush(stdout);
		exit(0);
	}
	else
		printf("Socket successfully created\n");
		fflush(stdout);

	if(setsockopt(sockfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0)
	{
		printf("ERROR setsockopt failed\n");
		fflush(stdout);
    	exit(0);
	}
	// Binding socket to server IP
	if ((bind(sockfd, res->ai_addr,res->ai_addrlen)) != 0) 
	{
		printf("ERROR Server socket bind failed\n");
		fflush(stdout);
		exit(0);
	}
	else
		printf("Socket successfully binded..\n");
		fflush(stdout);
	// server listening to a numberof clients ===50
	if ((listen(sockfd, 50)) != 0) 
	{
		printf("50 client allowed, please chceck..\n");
		fflush(stdout);
		exit(0);
	}
	else
		printf("Server listening for incominf conncetion..\n");
		fflush(stdout);

	freeaddrinfo(res);
	printf("<<<<<<<WELCOME TO CHAT SYSTEM >>>>>>>\n");
	fflush(stdout);

	while(1)
	{
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);

		// Check if max clients is reached
		if((cli_count + 1) == MAX_CLIENTS)
		{
			printf("ERROR, Rejected: cleint maxnumber reachead\n ");
			fflush(stdout);
			printf(":%d\n", cli_addr.sin_port);
			fflush(stdout);
			close(connfd);
			continue;
		}
		//sending the version protocol
		char Protocol[maxbuffersize]="HELLO 1\n";
		if(send(connfd, Protocol, strlen(Protocol), 0)<=0)
		{
			printf("ERROR, WRONG PROTOCOL VERSION \n");
			fflush(stdout);
			exit(0);
		}
		
		//NICK Name check
		char NICKmessage[maxbuffersize]={0};
		if(recv(connfd, NICKmessage, 32, 0) <= 0)
	    {
			printf("ERROR Receiving  NICK Name message\n");
			fflush(stdout);
			leave_flag = 1;
		} 
	    else
	    {		
			memcpy(name, NICKmessage+5,sizeof(NICKmessage));
			name[strcspn(name, "\n")] = '\0';
			#ifdef DEBUG
			printf("NICK Name received:%s\n",name);
			fflush(stdout);
			#endif

			//checking the NICK acceptenace 
			char *expression="^[A-Za-z0-9_]+$";
			regex_t regularexpression;
			int reti;
			reti=regcomp(&regularexpression, expression,REG_EXTENDED);
			if(reti)
			{
				printf("ERROR, Could not compile regex.\n");
				fflush(stdout);
				exit(0);
			}
			int matches;
			regmatch_t items;
		
			if(strlen(name)<12)
			{
				reti=regexec(&regularexpression, name,matches,&items,0);
				if(!reti)
				{
					//sending the OK to client 
					char OKmessage[12]="OK\n";
					if(send(connfd, OKmessage, strlen(OKmessage),0)<=0)
					{
						printf("ERROR on sending OK message");
						fflush(stdout);
					}
					else
					{					
						//add new client to the list
						client_t *cli = (client_t *)malloc(sizeof(client_t));
						cli->address = cli_addr;
						cli->sockfd = connfd;
						cli->uid = uid++;
						strcpy(cli->name, name);
						printf("%s JOINED THE CHAT\n",cli->name);
						fflush(stdout);
						queue_add(cli);
						pthread_create(&tid, NULL, &handle_client, (void*)cli);
					}
				} 
				else 
				{
					//sending the ERROR to client if not allowed into the chat 
					char Errormessage[12]="error joining chat\n";
					if(send(connfd, Errormessage, strlen(Errormessage),0)<=0)
					{
						printf("ERROR on sending NICK \n");
						fflush(stdout);
					}	
				}
			} 
			else
			{
				// if character are more than 12
				char Errormessage[12]="ERROR\n";
				if(send(connfd, Errormessage, strlen(Errormessage),0)<=0)
				{
					printf("ERROR on sending NICK");
					fflush(stdout);
				}
			}
		}
	}

	return 0;
}
