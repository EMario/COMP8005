//////////////////////////////////////////////////////////////////
//
//	Author: Mario Enriquez
//	ID:		A00909441
//	Class:	COMP 8005
//
//////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <gmp.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

#define MAX_FACTORS	1024
#define MAX_QUEUE	200
#define MSGSIZE 	50
#define WRITE_CONT	1000

// Globals
mpz_t dest[MAX_FACTORS];
char timestr[9];
long int aux=0;
int q[2];
FILE *f;

//function to obtain date time and writes into the log
void print_date(int date, pid_t pid){
	struct timeval now;
    	struct tm *tm;
   	int rc;
 	rc = gettimeofday(&now, 0);
	tm = localtime(&now.tv_sec);
	rc = strftime(timestr, sizeof(timestr), "%H:%M:%S", tm);
	//Part where dates will be written to the file
	if(rc>0){
		if(date==0){
			fseek(f,0,SEEK_END);
			fprintf(f, "%s	%ld	%s%s%s%06ld\n", "END",(long) pid, ":	",timestr, ".",now.tv_usec);
		} else if(date==1){
			fseek(f,0,SEEK_END);
			fprintf(f, "%s	%ld	%s%s%s%06ld\n", "START",(long) pid,":	", timestr, ".",now.tv_usec);
		} else if(date==2){
			fseek(f,0,SEEK_END);
			fprintf(f, "%ld	%ld	%s%s%s%06ld\n",(long) pid,aux, ":	",timestr, ".",now.tv_usec);
		} else if(date==3){
			fseek(f,0,SEEK_END);
			fprintf(f, "%s	%ld	%s%s%s%06ld\n", "OPAUX1",(long) pid,":	", timestr, ".",now.tv_usec);
		} else if(date==4){
			fseek(f,0,SEEK_END);
			fprintf(f, "%s	%ld	%s%s%s%06ld\n", "OPAUX2",(long) pid,":	", timestr, ".",now.tv_usec);
		}
	}
}

int main(int argc, char **argv) 
{
	mpz_t n,tmp, d,j;
  	int i,count;
 	pid_t pid;
	int p[2],r[2];
	char inbuf[MSGSIZE];
	pid_t parent=getpid();

	//Checks if the user wrote a number to be divided
  	if(argc != 2) 
	{
    		puts("Usage: ./<program_name> <number to be factored>");
    		return 1;
  	}	

	print_date(-1,getpid());	
	//File is created with the hour:min:sec when it is created
	f = fopen(strcat(timestr,"-Processes.txt"), "w");
	int write_c=0;
	//pipe declaration
	if (pipe(p) < 0)
	{
		perror("pipe call");
	 	return 1;
	}

	if (pipe(q) < 0)
	{
		perror("pipe call");
	 	return 1;
	}

	if (pipe(r) < 0)
	{
		perror("pipe call");
	 	return 1;
	}
	//fork creates an specific number of childs
	for(int j=0;j<4;j++){
		if(parent==getpid()){
			if ((pid = fork()) < 0){
				return 1;
			} 
			if(pid==0){
				print_date(1,getpid());
			}
		}
	}
 	mpz_init(n);
	mpz_init_set_str (n,argv[1],10);
	mpz_init(tmp);
	mpz_init(d);
	mpz_init(j);
	//Parent will create queue with values for the children to use
	if(getpid()==parent){
		print_date(1,getpid());
		mpz_set_ui(d, 1);
		mpz_add_ui(j,n,1);
		print_date(3,getpid());
		while (mpz_cmp (j, d)) {
			mpz_get_str(inbuf,10,d);
			write(p[1], inbuf, MSGSIZE);
			mpz_add_ui(tmp,d,1);
			mpz_swap(tmp, d);
			
	  	}
		for(int queue=0;queue<4;queue++){
			mpz_set_ui(d, 0);
			mpz_get_str(inbuf,10,d);
			write(p[1], inbuf, MSGSIZE);		
		}
		print_date(3,getpid());
	
		mpz_set_ui(d,0);
		mpz_get_str(inbuf,10,d);
		write(r[1], inbuf, MSGSIZE);
		count=0;
		print_date(4,getpid());
		//Gets the values from the children to set the  array
		do{
			read (r[0], inbuf, MSGSIZE);
			mpz_init_set_str (d,inbuf,10);
			
			if(mpz_cmp_ui(d,0)!=0){
				mpz_mod(tmp,n,d);
				if(mpz_cmp_ui(tmp,0)==0){
					mpz_init(dest[i]);
					mpz_set(dest[i],d);
					i++;
				}
			} else{
				count++;
			}
		} while(count<5);
		//Display the results
		for(int l=0;l<i;l++){
		  	gmp_printf("%Zd\n", dest[l]);
		}
		//clean mpz_t values
		for(int l=0;l<i;l++){
	 		mpz_clear(dest[l]);
		}
		print_date(4,getpid());
		print_date(0,getpid());
		fclose(f);
   		printf("\n");
	} else	{
		//Children use the values of the parent to obtain the factors
		do{
			read (p[0], inbuf, MSGSIZE);
			mpz_init_set_str (d,inbuf,10);
			
			if(mpz_cmp_ui(d,0)!=0){

				mpz_mod(tmp,n,d);
				if(mpz_cmp_ui(tmp,0)==0){
					write(r[1], inbuf, MSGSIZE);
				}

			}
			write_c++;			
			//Writes into the pipe the values which can divide perfectly to the parent
			if(write_c>=WRITE_CONT){
				aux+=WRITE_CONT;
				print_date(2,getpid());
				write_c=0;
			}
			
		} while(mpz_cmp_ui(d,0)!=0);
		mpz_set_ui(d,0);
		mpz_get_str(inbuf,10,d);
		write(r[1], inbuf, MSGSIZE);
		print_date(0,getpid());
	}
	//clean mpz_t values
	mpz_clear(tmp);
	mpz_clear(d);
	mpz_clear(n);
	mpz_clear(j);
  	return EXIT_SUCCESS;
}
