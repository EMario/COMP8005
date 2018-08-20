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
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>

#define SERVER_TCP_PORT		7001
#define LISTEN_PORTS		128
#define AX_EPOLL_EVENTS_PER_RUN	256
#define BUFLEN			80
#define RUN_TIMEOUT		-1
#define LOG_FILE 		"Epoll_Log.txt"

int fd_server;

static void ErrorMessage (const char*);
static int EchoProcess (int,struct timeval,char*,int, int);
void close_connection (int);
void set_non_blocking(int);
void print_into_file(char *,char *);

int main (int argc, char* argv[])
{
	static struct epoll_event event,events[AX_EPOLL_EVENTS_PER_RUN];
	struct sigaction interrupt;
	struct sockaddr_in addr, remote_addr,port_addr;
	int fd_new, epoll_fd,port,client_no=1;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	struct timeval tvalStart;
	FILE	*file;
	file = fopen(LOG_FILE, "w");
	fclose(file);

	switch (argc){
		//Set arguments from the command line
		case 2:
			port=atoi(argv[1]);
			break;

		default:
			port=SERVER_TCP_PORT;
			break;
	}

	interrupt.sa_handler = close_connection;
  interrupt.sa_flags = 0;
	if ((sigemptyset (&interrupt.sa_mask) == -1 || sigaction (SIGINT, &interrupt, NULL) == -1))//set interruption signal to use when epoll runs
	{
		ErrorMessage("Failed to set Interruption Signal.");
	}

	if ((fd_server = socket (AF_INET, SOCK_STREAM, 0))== -1) {//initialize socket
		ErrorMessage("Failed to create the socket");
	}


    	int arg = 1;
    	if (setsockopt (fd_server, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1) {//Set the socket to reuse address
		ErrorMessage("Cannot set port to be reusable");
    	}

		set_non_blocking(fd_server);
	//Gets the details of the client Name, IP and port

	memset (&addr, 0, sizeof (addr));
   	addr.sin_family = AF_INET;
   	addr.sin_addr.s_addr = htonl(INADDR_ANY);
   	addr.sin_port = htons(port);
	//binds address to the server socket
    	if (bind (fd_server, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
		ErrorMessage("Cannot bind");
	}
  if (listen (fd_server, LISTEN_PORTS) == -1) {
		ErrorMessage("Cannot set Listening ports.");
  }

  epoll_fd = epoll_create(AX_EPOLL_EVENTS_PER_RUN);
  if (epoll_fd == -1) {
		ErrorMessage("Cannot create epoll queue");
  }

	event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
	event.data.fd = fd_server;
  if ((epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_server, &event)) == -1) {
		ErrorMessage("epoll_ctl");
	}

	while (1){

		int nfds = epoll_wait(epoll_fd, events, AX_EPOLL_EVENTS_PER_RUN, RUN_TIMEOUT);
		if(nfds == -1){
			ErrorMessage("Error in the epoll wait!");
		}

		for (int i = 0; i < nfds; i++)
		{
		    	if (events[i].events & (EPOLLHUP | EPOLLERR)) {
					fputs("epoll: EPOLLERR", stderr);
					close(events[i].data.fd);
					continue;
		    	}
		    	assert (events[i].events & EPOLLIN);

		    if (events[i].data.fd == fd_server) {
					if ((fd_new = accept (fd_server, (struct sockaddr*) &remote_addr, &addr_size)) == -1){
			    			if (errno != EAGAIN && errno != EWOULDBLOCK) {
										perror("accept");
			    			}
			    			continue;
					}
					set_non_blocking(fd_new);
					event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
					event.data.fd = fd_new;
					if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, fd_new, &event) == -1) {
						ErrorMessage ("epoll_ctl");
					}
					if(getsockname(fd_new, (struct sockaddr *)&port_addr, &addr_size) == -1){
						ErrorMessage ("Failed to get port");
					}
					gettimeofday (&tvalStart, NULL);
					continue;
		    }

		    if (!EchoProcess(events[i].data.fd,tvalStart,inet_ntoa(remote_addr.sin_addr),client_no++,ntohs(remote_addr.sin_port))) {
					close (events[i].data.fd);
		    }

		}
	}
	close(fd_server);
	exit (EXIT_SUCCESS);
}

void set_non_blocking(int fd)
{
	if (fcntl (fd, F_SETFL, O_NONBLOCK | fcntl(fd, F_GETFL, 0)) == -1){
		ErrorMessage("fcntl");
	}
}

static int EchoProcess (int fd,struct timeval tvalStart,char *addr,int client, int port)
{
	int	 bytes_to_read,n,i;
	char	*bp, buf[BUFLEN+1],aux[80];
	int cont=1,iterations=0;
	struct timeval tvalS, tvalF;
	double avg=0;
	char logfile[300];
	//clean up aux and logfiles of unwanted data, so that we can store data on them
	memset(&aux[0], 0, sizeof(aux));
	memset(&logfile[0], 0, sizeof(logfile));
	snprintf(aux,80,"Remote Address:  %s Port number: %d Client: %d\n",addr,port,client);
	strcpy(logfile,aux);
	while (cont==1){//while the received flag is 1 continue the cycle
		gettimeofday (&tvalS, NULL);
		bp = buf;
		bytes_to_read = BUFLEN+1;
		n = read (fd, bp, bytes_to_read);
		if(n>0){//bytes read are bigger than 0 means there is data
			cont = buf[0] - '0';
			for(i=0;i<bytes_to_read-1;i++){
				buf[i]=buf[i+1];
			}
			if(cont==1){//return an echo to the client and add to the iterations counter
				iterations++;
				send (fd, buf, n, 0);
				gettimeofday (&tvalF, NULL);
				avg+=(double) (tvalF.tv_usec - tvalS.tv_usec) / 1000000 +
				(double) (tvalF.tv_sec - tvalS.tv_sec);
			}
			else{
				//get data to write into the log
				printf("TERMINATING CONNECTION\n");
				avg/=iterations;
				memset(&aux[0], 0, sizeof(aux));
				snprintf(aux,80,"Received and Echoed: %s - %ld bytes - %d times\nAverage response time: %f \n",buf,strlen(bp),iterations,avg);
				strcat(logfile,aux);
				gettimeofday (&tvalF, NULL);
				memset(&aux[0], 0, sizeof(aux));
				snprintf(aux,80,"\nTotal Connection Time: %f\nTerminating connection.\n\n",
				(double) (tvalF.tv_usec - tvalStart.tv_usec) / 1000000 + (double) (tvalF.tv_sec - tvalStart.tv_sec));
				strcat(logfile,aux);
				//write string into the log file
				print_into_file(LOG_FILE,logfile);
				close(fd);
				return 1;
			}
		}
		fflush(stdout);
	}
	close(fd);
	return 0;
}

// Prints the error stored in errno and aborts the program.
static void ErrorMessage(const char* message){
    	perror (message);
    	exit (EXIT_FAILURE);
}

// close server connection in case of an error
void close_connection (int signo){
        close(fd_server);
	exit (EXIT_SUCCESS);
}

//Writes logfile into the specified filename
void print_into_file(char *filename,char *logfile){
	FILE *file = fopen(filename, "a");
	fprintf(file,"%s",logfile);
	fclose(file);
}
