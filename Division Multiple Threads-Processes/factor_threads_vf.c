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
#include <pthread.h>
#include <sys/time.h>
#include <string.h>

#define MAX_FACTORS	1024
#define MAX_QUEUE	100
#define WRITE_CONT	1000
// Globals
mpz_t dest[MAX_FACTORS];
mpz_t q[MAX_QUEUE];
pthread_mutex_t printfLock = PTHREAD_MUTEX_INITIALIZER;
mpz_t counter;
int i = 0;
int count;
long int aux=0;
FILE *f;
void* factor (void*);
char timestr[9];
int write_c=0;

//function to obtain date time and writes into the log
void print_date(int date){
	struct timeval now;
    	struct tm *tm;
   	int rc;
 	rc = gettimeofday(&now, 0);
	tm = localtime(&now.tv_sec);
	rc = strftime(timestr, sizeof(timestr), "%H:%M:%S", tm);
	//Part where dates will be written to the file
	if(rc>0){
		if(date==0){
			fprintf(f, "%s %s%s%06ld", "END:			", timestr, ".",now.tv_usec);
			fprintf(f,"\n");
		} else if (date==1){
			fprintf(f, "%s %s%s%06ld", "START:			", timestr, ".",now.tv_usec);
			fprintf(f,"\n");
		} else if (date==2){
			aux+=WRITE_CONT;
			fprintf(f, "%s%ld%s %s%s%06ld", "OPER( ", aux," ):		", timestr , ".",now.tv_usec);
			fprintf(f,"\n");
		}
	}
}

int main(int argc, char **argv) 
{
	struct timeval now;
    	struct tm *tm;
   	int rc;
	mpz_t n,j,tmp;
  	int l=0;
	const char* msg = "";
	//Checks if the user wrote a number to be divided
  	if(argc != 2) 
	{
    		puts("Usage: ./<program_name> <number to be factored>");
    		return EXIT_SUCCESS;
  	}
	mpz_init(counter);
	mpz_set_ui(counter, 1);
	pthread_t thread1, thread2, thread3, thread4, thread5;
	print_date(-1);
	//File is created with the hour:min:sec when it is created
	f = fopen(strcat(timestr,"-threads.txt"), "w");
	print_date(1);
 
	mpz_init_set_str (n,argv[1],10);
	mpz_init(j);
	mpz_add_ui(j,n,1);
	for(int k=0;k<MAX_QUEUE;k++){
		mpz_init(q[k]);		
	}
	mpz_init(tmp);
	//while loop where I create the threads to call to use factors
	while (mpz_cmp (j, counter)>0) 
	{
		//Creates queue where the threads will get the values
		for(int k=0;k<MAX_QUEUE;k++){
			if(mpz_cmp (j, counter)>0){
				mpz_set(q[k], counter);	
				mpz_add_ui(tmp,counter,1);
				mpz_swap(tmp, counter);
			} else {
				mpz_set_ui(q[k], 0);	
			}
		}	
		count=0;
	  	pthread_create (&thread1, NULL, factor, (void*)n);
	  	pthread_create (&thread2, NULL, factor, (void*)n);
	  	pthread_create (&thread3, NULL, factor, (void*)n);
	  	pthread_create (&thread4, NULL, factor, (void*)n);
	  	pthread_create (&thread5, NULL, factor, (void*)n);
	  	pthread_join (thread1, NULL);
		pthread_join (thread2, NULL);
		pthread_join (thread3, NULL);
		pthread_join (thread4, NULL);
		pthread_join (thread5, NULL);

	}
	//part where I clean the mpz values as to prevent memory leaks
	mpz_clear(n);
	mpz_clear(j);
	mpz_clear(counter);
	for(int k=0;k<MAX_QUEUE;k++){
		mpz_clear(q[k]);		
	}
  	for(l=0;l<i;l++){
  		gmp_printf("%Zd\n", dest[l]);
	}
	for(l=0;l<i;l++){
  		mpz_clear(dest[l]);
	}
	print_date(0);
  	printf("\n");
	fclose(f);
  	return EXIT_SUCCESS;
}

//Function that gets the factors
void* factor (void* n){
	mpz_t tmp,j;
	mpz_init(tmp);
	mpz_init(j);
  	while (count<MAX_QUEUE) 
	{
	    	pthread_mutex_lock (&printfLock);	

		if(mpz_cmp_ui(q[count],0)==0){
			pthread_mutex_unlock (&printfLock);
			sched_yield();
			mpz_clear(tmp);
			mpz_clear(j);
			return NULL;
		}
		mpz_mod(tmp,n,q[count]);
	    	if(mpz_cmp_ui(tmp,0)==0){
		    	mpz_init(dest[i]);
		    	mpz_set(dest[i], q[count]);
		  	i++;
		}
		count++;		
		write_c++;
		//Write in the log the time of the operation
		if(write_c>=WRITE_CONT){
			print_date(2);
			write_c=0;
		}
		pthread_mutex_unlock (&printfLock);
		sched_yield();
	}
	mpz_clear(tmp);
	mpz_clear(j);
	return NULL;
}
