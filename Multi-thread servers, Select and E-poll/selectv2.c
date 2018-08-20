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
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define SERVER_TCP_PORT    7000
#define BUFLEN  80
#define LISTEN_PORTS		128
#define LOG_FILE 		"Select_Log.txt"
#define PROCESSES		5

static void ErrorMessage (const char* message);
static int EchoProcess (int,struct timeval,char*,int, int);
void print_into_file(char *,char *);

int main (int argc, char* argv[]) {
  int sel_fd,port;
  fd_set active_fd_set, read_fd_set;
  int i,client_no=1,pid=0,arg=1;
  struct sockaddr_in remote_addr, addr;
  socklen_t addr_size=sizeof(struct sockaddr_in);
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

	if((sel_fd = socket (PF_INET,SOCK_STREAM,0))==-1){//initialize socket
		ErrorMessage ("Failed on setting the socket.");
	}

	if(setsockopt (sel_fd, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg))==-1){//Set the socket to reuse address
		ErrorMessage ("Failed on setting the socket to reuse address.");
	}

	bzero(&addr, addr_size);

	printf("Socket has been created");
	//Gets the details of the client Name, IP and port
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	//binds address to the server socket
	if(bind(sel_fd, (struct sockaddr*) &addr, addr_size) == -1){
		ErrorMessage ("Failed on binding the port");
	}
	//set the server socket to listen up to LISTEN_PORTS

  if (listen (sel_fd, LISTEN_PORTS) == -1)
    {
			ErrorMessage ("Failed on setting listening ports");
    }
  for(int i=0;i<PROCESSES;i++){
		if(pid==0){
			pid=fork();
		}
	}
  //Set Select File descriptors
  FD_ZERO (&active_fd_set);
  FD_SET (sel_fd, &active_fd_set);

  while (1)
    {
      read_fd_set = active_fd_set;
	  //sets select to check the read_fd_set filedescriptores
      if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
        {
          ErrorMessage ("Failure on Select");
        }

      for (i = 0; i < FD_SETSIZE; ++i) //Sweep all the possible descriptors
        if (FD_ISSET (i, &read_fd_set)) //Select Server finds socket in the files descriptor
          {
            if (i == sel_fd)//if the server detects new connection
              {
				//if its a new connection it accepts it
                int new;
                new = accept (sel_fd,(struct sockaddr *) &remote_addr,&addr_size);
                if (new < 0)
                  {
                    perror ("Failed to set New Connection");
                  }
                gettimeofday (&tvalStart, NULL);
				//sets new socket connection as active
                FD_SET (new, &active_fd_set);
              }
            else//if socket is sending data
              {
                if (EchoProcess (i,tvalStart,inet_ntoa (remote_addr.sin_addr),client_no++,ntohs (remote_addr.sin_port)) == 0)//start data exchange with the client
                  {
                    close (i);//close socket
                    FD_CLR (i, &active_fd_set);//free active file descriptor
                  }
              }
          }
    }
}

static int EchoProcess (int fd,struct timeval tvalStart,char *addr,int client, int port){
		int	 bytes_to_read,n,i;
		char	*bp, buf[BUFLEN+1],aux[80];
		int iterations=0,cont=1;
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
				for(i=0;i<bytes_to_read-1;i++){//removes the flag
					buf[i]=buf[i+1];
				}
				if(cont==1){//return an echo to the client and add to the iterations counter
					printf ("Echo: %s\n", buf);
					send (fd, buf, n, 0);
          iterations++;
          gettimeofday (&tvalF, NULL);
  				avg+=(double) (tvalF.tv_usec - tvalS.tv_usec) / 1000000 +
  				(double) (tvalF.tv_sec - tvalS.tv_sec);
				}
				else{
          printf("TERMINATING CONNECTION\n");
		  //get data to write into the log
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
          return 0;
				}
			}
			//clear buffer
			fflush(stdout);
		}
		return 1;
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
