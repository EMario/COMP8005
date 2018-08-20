//////////////////////////////////////////////////////////////////
//
//	Author: Mario Enriquez, Charles Kevin Tan
//
//	Class:	COMP 8005
//
//	Due Date: 2016/02/15
//
//////////////////////////////////////////////////////////////////

#include<stdio.h>
#include<string.h>    
#include<stdlib.h>   
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>  
#include <sys/types.h>
#include <time.h>  
#include<pthread.h> 
#include<math.h>
#include <sys/time.h>

#define SERVER_TCP_PORT 7000
#define LISTEN_PORTS 5
#define BUFLEN 80
#define LOG_FILE "Thread_Log.txt"

void *EchoProcess (void *);
static void ErrorMessage (const char*);
void print_into_file(char *,char *);

int client=1;
char mssg[80];
pthread_mutex_t printfLock = PTHREAD_MUTEX_INITIALIZER;

int main(int argc , char *argv[])
{
    int socket_create, client_socket,*aux_socket,port;
    struct sockaddr_in server,client;
    socklen_t connections = sizeof(struct sockaddr_in);
    FILE *file = fopen(LOG_FILE, "w");
    fclose(file);
    int request_cycle = 0 ;
    switch (argc){
		//Set arguments from the command line
	case 2:
		port=atoi(argv[1]);
		break;

	default:
		port=SERVER_TCP_PORT;
		break;
    }
    if ((socket_create = socket(AF_INET , SOCK_STREAM , 0)) == -1)
    {
	ErrorMessage("Failed to create the socket.");
    }
    printf("Socket has been created");
    server.sin_family = AF_INET;
    server.sin_port = htons( port );
    server.sin_addr.s_addr = INADDR_ANY;
    if( bind(socket_create,(struct sockaddr *)&server ,
        sizeof(server)) < 0)
    {
	ErrorMessage("Cannot bind.");
    }
    puts("\nBind done");

    listen(socket_create , LISTEN_PORTS);

    puts("Wait for client connections");
    connections = sizeof(struct sockaddr_in);
    pthread_t id_thread;
    //Keep accepting connections

    while( (client_socket = accept(socket_create, (struct sockaddr *)&client, (socklen_t*)&connections)) )
    {	

	aux_socket=malloc(1);
	*aux_socket=client_socket;
		//Create the thread for the new connection
        if( pthread_create( &id_thread , NULL ,  EchoProcess , (void*) aux_socket) < 0)
        {
            	ErrorMessage("Failed to create the thread.");
        }
	request_cycle = request_cycle + 1;
        puts("Connection accepted");
        printf("IP address of client connecting is: %s\n", inet_ntoa(client.sin_addr));
        printf("port is: %d\n", (int) ntohs(client.sin_port));
	snprintf(mssg,100,"IP address of client connecting is: %s\nport is: %d\n", inet_ntoa(client.sin_addr),(int) ntohs(client.sin_port));
	printf("Requests received:%d\n", request_cycle);

      //Free the thread resources
	pthread_detach(id_thread);
        puts("Assign the thread");
    }

    if (client_socket < 0)
    {
	ErrorMessage("Cannot accept client socket.");
    }
	printf("HELLO");
    return 0;
}

void* EchoProcess (void *socket_create)
{
	int	 bytes_to_read,n,i;
	char	*bp, buf[BUFLEN+1],aux[100];
	int fd = *(int*)socket_create;
	int cont=1,iterations=0;
	struct timeval tvalStart,tvalS, tvalF;
	double avg=0;
	char logfile[300];
	//clean up aux and logfiles of unwanted data, so that we can store data on them
	memset(&aux[0], 0, sizeof(aux));
	memset(&logfile[0], 0, sizeof(logfile));
	strcpy(aux,mssg);
	strcpy(logfile,aux);
	gettimeofday (&tvalStart, NULL);
	while (cont==1){//while the received flag is 1 continue the cycle
		gettimeofday (&tvalS, NULL);
		bp = buf;
		bytes_to_read = BUFLEN+1;
		n = read (fd, bp, bytes_to_read);
		iterations++;
		if(n>0){//bytes read are bigger than 0 means there is data
			cont = buf[0] - '0';
			for(i=0;i<bytes_to_read-1;i++){
				buf[i]=buf[i+1];
			}
			if(cont==1){//return an echo to the client and add to the iterations counter
				printf ("Echo: %s\n", buf);
				send (fd, buf, n, 0);
				gettimeofday (&tvalF, NULL);
				avg+=(double) (tvalF.tv_usec - tvalS.tv_usec) / 1000000 +(double) (tvalF.tv_sec - tvalS.tv_sec);
			}
			else{
				//get data to write into the log
				avg/=iterations;
				snprintf(aux,100,"Client No: %d\nReceived and Echoed: %s - %ld bytes - %d times\nAverage response time: %f",client++,buf,strlen(bp),iterations,avg);
				strcat(logfile,aux);
				gettimeofday (&tvalF, NULL);
				memset(&aux[0], 0, sizeof(aux));
				snprintf(aux,100,"\nTotal Connection Time: %f\nTerminating connection.\n\n",
				(double) (tvalF.tv_usec - tvalStart.tv_usec) / 1000000 + (double) (tvalF.tv_sec - tvalStart.tv_sec));
				strcat(logfile,aux);
				//write string into the log file
				print_into_file(LOG_FILE,logfile);
				avg/=iterations;
				printf("TERMINATED CONNECTION\n");

				
			}
		}
		fflush(stdout);
	}
	free(socket_create);
	close(fd);

}

// Prints the error stored in errno and aborts the program.
static void ErrorMessage(const char* message){
    	perror (message);
    	exit (EXIT_FAILURE);
}

//Writes logfile into the specified filename
void print_into_file(char *filename,char *logfile){
	FILE *file = fopen(filename, "a");
	fprintf(file,"%s",logfile);
	fclose(file);
}

