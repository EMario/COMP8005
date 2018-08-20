//////////////////////////////////////////////////////////////////
//
//	Author: Mario Enriquez, Charles Kevin Tan
//
//	Class:	COMP 8005
//
//	Due Date: 2016/02/15
//
//////////////////////////////////////////////////////////////////

#include <stdio.h>     
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <netinet/tcp.h>
#include <stdlib.h>     
#include <string.h>     
#include <unistd.h>    
#include <sys/time.h>
#include <sys/types.h>
#include <netdb.h>
#include <fcntl.h>

#define SERVER_TCP_PORT		8005
#define BUFLEN			80
#define MESSAGE			"HELLO!"
#define MAX_RETRIES		5
#define ITERATIONS		1
#define CLIENTS			1
#define TYPE				1
#define LOG_FILE 		"Client_Log.txt"
#define EXCEL_FILE 		"Excel_Data_Client.txt"

static void ErrorMessage (const char* message);
static void EchoMssg(int,int,char*,char*,FILE *,long int);
void print_into_file(char *,char *); 

int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	int port,c_socket,iterations,clients,pid,i,arg=1;
	long int client_no=0;
	struct hostent	*hp;
	struct timeval tvalStart, tvalFinish;
	struct linger linger;
	char  *host, **pptr,str[16],tmp[BUFLEN],logfile[300],aux[50];
	FILE	*file,*excel;
	file = fopen(LOG_FILE, "w"); //Creating/Rewriting log file 
	excel = fopen(EXCEL_FILE, "w"); //Creating/Rewriting Excel file
	fprintf(excel,"%s","Client\tTime\n");
	fclose(file);
	fclose(excel);
	pid=0;
	switch (argc){
		//Set arguments from the command line
		case 2:
			host=argv[1];
			port=SERVER_TCP_PORT;
			clients=CLIENTS;
			iterations=ITERATIONS;
			strcpy(tmp,MESSAGE);
			break;
		case 3:
		  host=argv[1];
	  	port=atoi(argv[2]);
			clients=CLIENTS;
			iterations=ITERATIONS;
			strcpy(tmp,MESSAGE);
			break;
		case 4:
		  host=argv[1];
	  	port=atoi(argv[2]);
	  	clients=atoi(argv[3]);
			iterations=ITERATIONS;
			strcpy(tmp,MESSAGE);
			break;
		case 5:
		  host=argv[1];
	  	port=atoi(argv[2]);
	  	clients=atoi(argv[3]);
	  	iterations=atoi(argv[4]);
			strcpy(tmp,MESSAGE);
			break;
		case 6:
		  host=argv[1];
	  	port=atoi(argv[2]);
	  	clients=atoi(argv[3]);
	  	iterations=atoi(argv[4]);
			strcpy(tmp,argv[5]);
			break;
		break;
		default:
			fprintf(stderr, "Usage: %s host [port] [client no] [iterations] [message]\n",argv[0]);
			exit(1);
	}
	while (i<clients-1){ //Multiprocessing start, the program will fork into a defined number of processes to send requests to the server
		if(pid==0){
			client_no++;
			pid=fork();
		}
		i++;
	}
	if(pid==0){//Sets the client number of the last forked process
		client_no++;
	}
	gettimeofday (&tvalStart, NULL);

	if ((c_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1){//initialize socket
		ErrorMessage("Socket failure");
	}

	if (setsockopt (c_socket, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1) {//Set the socket to reuse address
		ErrorMessage("Failed to set to reuse address");
	}
	
	//Avoid unneccessary long connections by setting the linger option of the socket off
	memset(&linger,0,sizeof(linger));
	linger.l_onoff=1;
	linger.l_linger=0;
	if(setsockopt(c_socket,SOL_SOCKET,SO_LINGER,(char *) &linger,sizeof(linger))==-1){
		ErrorMessage("Linger set failure");
	}

	//Gets the details of the client Name, IP and port
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	hp=gethostbyname(host);
	if (hp== NULL)
	{
		fprintf(stderr, "Unknown server address\n");
		exit(1);
	}
	bcopy(hp->h_addr, (char *)&addr.sin_addr, hp->h_length);
	//Connects to the server
	if (connect(c_socket, (struct sockaddr *) &addr, sizeof(addr)) == -1){
			ErrorMessage("Connection to the Server Failed");
	}
	pptr = hp->h_addr_list;
	//clean the arrays of aux and logfile to remove unwanted characters
	memset(&aux[0], 0, sizeof(aux));
	memset(&logfile[0], 0, sizeof(logfile));
	snprintf(aux,50,"Connected Client No: %ld \nServer Name: %s\n",client_no,hp->h_name);
	strcat(logfile,aux);
	memset(&aux[0], 0, sizeof(aux));
	snprintf(aux,50,"IP Address: %s", inet_ntop(hp->h_addrtype, *pptr, str, sizeof(str)));
	strcat(logfile,aux);
	printf("Connected to Server: %s IP Address: %s\n",hp->h_name,aux);
	//Starts the function EchoMssg
	EchoMssg(c_socket,iterations,tmp,logfile,excel,client_no);
	gettimeofday (&tvalFinish, NULL);
	//closes connection
	close(c_socket);
	memset(&aux[0], 0, sizeof(aux));
	snprintf(aux,50,"\nTotal time = %f\nConnection Closed!\n\n",(double) (tvalFinish.tv_usec - tvalStart.tv_usec) / 1000000 +
	(double) (tvalFinish.tv_sec - tvalStart.tv_sec));
	strcat(logfile,aux);
	printf("Closing Connection, Total Connection time = %f\n",(double) (tvalFinish.tv_usec - tvalStart.tv_usec) / 1000000 +
	(double) (tvalFinish.tv_sec - tvalStart.tv_sec));
	//print log data into archive
	print_into_file(LOG_FILE,logfile);

	exit(0);
}

// Prints the error stored in errno and aborts the program.
static void ErrorMessage(const char* message)
{
    perror (message);
    exit (EXIT_FAILURE);
}

//Echo process with the server, adds a flag at the start to indicate if its EOT
static void EchoMssg(int c_socket, int iterations,char *message,char *logfile,FILE *excel,long int client_no)
{
	char *bp, rbuf[BUFLEN+1], sbuf[BUFLEN+1], aux[85], data[20];
	int bytes_to_read,i;
	struct timeval tvalS, tvalF;
	double avg=0;
	strcpy(sbuf,"1");
	strcat(sbuf,message);
	memset(&aux[0], 0, sizeof(aux));
	snprintf(aux,85,"\nSending message: %s size: %ld bytes - %d times.",message,strlen(message),iterations);
	strcat(logfile,aux);
	printf("Sending Message: %s, of size %ld\n",message,strlen(message));

	for(i=0;i<iterations;i++){//send message depending number of iterations, attach flag 1
		gettimeofday (&tvalS, NULL);
		send (c_socket, sbuf, BUFLEN, 0);
		bp = rbuf;
		bytes_to_read = BUFLEN;
		recv (c_socket, bp, bytes_to_read, 0);
		fflush(stdout);
		gettimeofday (&tvalF, NULL);
		avg+=(double) (tvalF.tv_usec - tvalS.tv_usec) / 1000000 +
		(double) (tvalF.tv_sec - tvalS.tv_sec);
	}
	avg/=iterations;
	//set flag to 0 to indicate EOT
	memset(&data[0], 0, sizeof(data));
	snprintf(data,20,"%ld\t%f\n",client_no,avg);
	print_into_file(EXCEL_FILE,data);
	memset(&aux[0], 0, sizeof(aux));
	snprintf(aux,85,"\nReceived: %s - %d times.\nAverage Response Time: %f\nTotal Size: %ld bytes",bp,iterations,avg,
	(strlen(message)*iterations));
	printf("Received Message: %s, of size %ld\n",bp,strlen(message));
	strcat(logfile,aux);
	sbuf[0]='0';
	write (c_socket, "", 0);
}

//Writes logfile into the specified filename
void print_into_file(char *filename,char *logfile){
	FILE *file = fopen(filename, "a");
	fprintf(file,"%s",logfile);
	fclose(file);
}
