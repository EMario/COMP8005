//////////////////////////////////////////////////////////////////
//
//	Author: Mario Enriquez
//
//	Class:	COMP 8005
//
//	Due Date: 2016/04/04
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
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>

#define IPLIST "ip_list.txt"
#define LOGFILE "log_file.txt"
#define FAILURE -1
#define SUCCESS 0
#define LISTEN_PORTS 128
#define BUFLEN 512
#define AX_EPOLL_EVENTS_PER_RUN	2048
#define RUN_TIMEOUT		-1
#define PROCESSES 5

static void ErrorMessage (const char*);
void close_connection (int);
void set_non_blocking(int);
int forwarding(struct fd_sock);
int getForwardTable(std::vector<struct ip_port> &,std::string);
void print_into_file(std::string);

int fw_server;
int assignedPort;
std::string logfile;

struct ip_port{
	int srcport;
	std::string ip;
	int dstport;
};

struct fd_sock{
	int src;
	int dst;
	std::string src_ip;
	std::string dst_ip;
};

int main (int argc, char* argv[])
{
	int i,pid=0,portNo=-1,nfds,fw_new,fw_socket,arg=1,conn,fw_server,epoll_counter=0;
	struct fd_sock fd_sockets[AX_EPOLL_EVENTS_PER_RUN];
	std::vector<ip_port> forwardList;
	std::string iplist,ip_remote,message;
	struct sigaction interrupt;
	static struct epoll_event events[AX_EPOLL_EVENTS_PER_RUN], event;
	int epoll_fd,src,dst;
	struct sockaddr_in remote_addr,port_addr,server_addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	struct hostent *host;
	char *hostip;

	switch (argc){
		case 2:
			iplist=argv[1];
			logfile=LOGFILE;
			break;
		case 3:
			iplist=argv[1];
			logfile=argv[2];
			break;
		default:
			iplist=IPLIST;
			logfile=LOGFILE;
			break;
	}

	interrupt.sa_handler = close_connection;
	interrupt.sa_flags = 0;
	if ((sigemptyset (&interrupt.sa_mask) == -1 || sigaction (SIGINT, &interrupt, NULL) == -1))//set interruption signal to use when epoll runs
	{
		ErrorMessage("Failed to set Interruption Signal.");
	}

	if(getForwardTable(forwardList,iplist)==FAILURE){
		ErrorMessage("Forwarding Table is empty.");
	}

	for(i=1;i<(int) forwardList.size();i++){
		if(pid==0){
			pid=fork();
			portNo=(i-1);
		}
	}
	if(pid==0){
		portNo++;
	}
	assignedPort=forwardList[portNo].srcport;

 	if ((fw_server = socket (AF_INET, SOCK_STREAM, 0))== -1) {//initialize socket
 		ErrorMessage("Failure to set the socket server");
 	}

 	if (setsockopt (fw_server, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1) {//Set the socket to reuse address
 		ErrorMessage("Cannot set port to be reusable");
 	}

 	set_non_blocking(fw_server);

 	memset (&server_addr, 0, sizeof (server_addr));
 	server_addr.sin_family = AF_INET;
 	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
 	server_addr.sin_port = htons(forwardList[portNo].srcport);
 	// //binds address to the server socket
 	if (bind (fw_server, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1) {
 		ErrorMessage("Cannot bind");
 	}

 	if (listen (fw_server, LISTEN_PORTS) == -1) {
 		ErrorMessage("Cannot set Listening ports.");
 	}
	/*pid=0;
	for (int i=0;i<PROCESSES;i++){
		if(pid==0){
			pid=fork();
		}
	}*/

	 epoll_fd = epoll_create(AX_EPOLL_EVENTS_PER_RUN);
	 if (epoll_fd == -1) {
	 	ErrorMessage("Cannot create epoll queue");
	 }
	 	event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
	 	event.data.fd = fw_server;
	 	if ((epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fw_server, &event)) == -1) {
	 	 ErrorMessage("epoll_ctl");
	 	}
	 while (1){

		 if((nfds = epoll_wait(epoll_fd, events, AX_EPOLL_EVENTS_PER_RUN, RUN_TIMEOUT))==-1){
			 ErrorMessage("Error in the epoll wait!");
		 }
		 for (i=0;i<nfds;i++){
			 if (events[i].events & (EPOLLHUP | EPOLLERR)){
				 //fputs("epoll:EPOLLERR\n",stderr);
				 print_into_file("epoll:EPOLLERR\n");
				 src=fd_sockets[events[i].data.fd].src;
				 dst=fd_sockets[events[i].data.fd].dst;
				 close (src);
				 close (dst);
				 continue;
			 }
			 assert (events[i].events & EPOLLIN);
			 if (events[i].data.fd == fw_server) {
			 	if ((fw_new = accept (fw_server, (struct sockaddr*) &remote_addr, &addr_size)) == -1){
			 		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			 			perror("accept");
						printf("Could not Accept");
			 		}
			 		continue;
			 	}
				char* remote=(inet_ntoa(remote_addr.sin_addr));
			 	set_non_blocking(fw_new);
				event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
			 	event.data.fd = fw_new;
			 	if ((epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fw_new, &event)) == -1) {
			 		ErrorMessage("epoll_ctl");
			 	}
				epoll_counter++;
				if(getsockname(fw_new, (struct sockaddr *)&port_addr, &addr_size) == -1){
					ErrorMessage ("Failed to get port");
				}
				hostip=new char[forwardList[portNo].ip.length()+1];

				strcpy(hostip, forwardList[portNo].ip.c_str());
				host=gethostbyname(hostip);
				delete [] hostip;
				if ((fw_socket = socket (AF_INET, SOCK_STREAM, 0))== -1) {//initialize socket
					ErrorMessage("Failure to set the socket server");
				}

				if (setsockopt (fw_socket, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1) {//Set the socket to reuse address
					ErrorMessage("Cannot set port to be reusable");
				}

				memset (&remote_addr, 0, sizeof (remote_addr));
			 	remote_addr.sin_family = AF_INET;
				memcpy(&remote_addr.sin_addr, host->h_addr, host->h_length);
			  	remote_addr.sin_port = htons(forwardList[portNo].dstport);
				if((conn=connect(fw_socket,(struct sockaddr *) &remote_addr, sizeof (remote_addr)))>=0){
					set_non_blocking(fw_socket);
				} else {
					close(fw_socket);
				}
				event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
				event.data.fd = fw_socket;
				if ((epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fw_socket, &event)) == -1) {
				 ErrorMessage("epoll_ctl");
				}
				epoll_counter++;
				if(getsockname(fw_new, (struct sockaddr *)&port_addr, &addr_size) == -1){
					ErrorMessage ("Failed to get port");
				}
				fd_sockets[fw_new].src=fw_new;
				fd_sockets[fw_socket].src=fw_socket;
				fd_sockets[fw_new].dst=fw_socket;
				fd_sockets[fw_socket].dst=fw_new;
				fd_sockets[fw_new].src_ip=remote;
				fd_sockets[fw_new].dst_ip=forwardList[portNo].ip;
				fd_sockets[fw_socket].src_ip=forwardList[portNo].ip;
				fd_sockets[fw_socket].dst_ip=remote;
				//std::cout << epoll_counter << std::endl;

				message = ("Established Connection from " + fd_sockets[fw_new].src_ip + " to " + forwardList[portNo].ip);
				print_into_file(message);
				continue;
			}
			if (!forwarding(fd_sockets[events[i].data.fd])) {
				src=fd_sockets[events[i].data.fd].src;
				dst=fd_sockets[events[i].data.fd].dst;
				close (src);
				close (dst);
				epoll_counter--;
				//std::cout << epoll_counter << std::endl;
				message = ("Closing Connection from " + fd_sockets[fw_new].src_ip + " to " + forwardList[portNo].ip);
				print_into_file(message);
			}
	 }
 }
	close(fw_server);
	exit (EXIT_SUCCESS);
}

void set_non_blocking(int fd)
{
	if (fcntl (fd, F_SETFL, O_NONBLOCK | fcntl(fd, F_GETFL, 0)) == -1){
		ErrorMessage("fcntl");
	}
}

//Prints the error stored in errno and aborts the program.
static void ErrorMessage(const char* message){
    	perror (message);
    	exit (EXIT_FAILURE);
}

// close server connection in case of an error
void close_connection (int signo){
  close(fw_server);
	print_into_file("Closing Connection...");
 	exit (EXIT_SUCCESS);
}

//Reads all the files from the Forward Table
int getForwardTable(std::vector<ip_port> &forwardList,std::string iptable){
	int result=0;
	ip_port newIpPort;
	std::ifstream infile(iptable.c_str());
	std::string ip;
	int srcport,dstport;
	while(infile >> srcport >> ip >> dstport){
		newIpPort.srcport=srcport;
		newIpPort.ip=ip;
		newIpPort.dstport=dstport;
		forwardList.push_back(newIpPort);
		result++;
	}
	if(result>0){
		return SUCCESS;
	}
	return FAILURE;
}

int forwarding(struct fd_sock sock){
	int n, bytes_to_read,total_sent;
	char    *bp, buf[BUFLEN];

	bp = buf;
	bytes_to_read = BUFLEN;
	total_sent=0;
	while ((n = recv (sock.src, bp, bytes_to_read, 0)) > 0) {
		bp += n;
		bytes_to_read -= n;
		total_sent+=n;
		send (sock.dst, buf, n, 0);
		//std::cout << "Source: " << sock.src << " Bytes: " << n << " Dest: " << sock.dst << std::endl;
	}
	//std::cout << "Source: " << sock.src << " Bytes: " << n << " Dest: " << sock.dst << std::endl;
	//std::cout << total_sent << std::endl;
	if(total_sent==0){
		
		return 0;
	} else {
		return 1;
	}
}

void print_into_file(std::string log_info){
		time_t timev = time(0);
		struct tm * cur = localtime( & timev );

    std::ofstream log(logfile.c_str(), std::ios_base::app | std::ios_base::out);

    log << (cur->tm_year + 1900) << "/" << (cur->tm_mon+1) << "/" << (cur->tm_mday) << " ";
		log << (cur->tm_hour) << ":" << (cur->tm_min) << ":" << (cur->tm_sec) << " Port " << assignedPort << " : " << log_info << std::endl;


}
