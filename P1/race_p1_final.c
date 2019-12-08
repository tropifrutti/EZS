/*
Quellcode zum Praktikum "Echtzeitsysteme" im Fachbereich Elektrotechnik
und Informatik der Hochschule Niederrhein.

Fuer die Generierung wird die Realzeitbibliothek "rt" benoetigt.
Wenn das folgende Makefile verwendet wird, muss zur Generierung nur
noch "make" auf der Kommandozeile eingegeben werden:
==================================
CFLAGS=-g -Wall -m32
LDLIBS=-lrt

all: race

clean:
	rm -f race *.o
==================================

Sa 27. Sep 17:57:59 CEST 2014
*/
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <time.h>
#include <sys/time.h>
#include <asm/types.h>
#include <pthread.h>

#define make_seconds( a ) (a/1000000)
#define make_mseconds( a ) ((a-((a/1000000)*1000000))/1000)

static void set_speed( int fd, int Speed );

static unsigned char basis_speed_in=0x24, basis_speed_out=0x24;
static unsigned long last_time;
static int fdc=0;

typedef struct _liste {
	int type;
	int length;
	struct _liste *next;
} streckenliste;

static streckenliste *root = NULL; 

static struct _liste *add_to_liste( int type, int length )
{
	struct _liste *ptr, *lptr;

	lptr = malloc( sizeof(struct _liste) );
	if( lptr==NULL ) {
		printf("out of memory\n");
		return NULL;
	}
	lptr->type = type;
	lptr->length = length;
	lptr->next = root;
	if( root == NULL ) {
		root = lptr;
		lptr->next = lptr;
		return root;
	}
	for( ptr=root; ptr->next!=root; ptr=ptr->next )
		;
	ptr->next = lptr;
	return lptr->next;
}

static void print_liste(void)
{
	struct _liste *lptr = root;
	int length_sum=0;

	do {
		printf("0x%04x - %d [mm]\n", lptr->type, lptr->length );
		length_sum += lptr->length;
		lptr = lptr->next;
	} while( lptr!=root );
	printf("sum: %d\n", length_sum );
}

static void exithandler( int signr )
{
	if( fdc )
		set_speed( fdc, 0x0 );
	exit( 0 );
}

static void set_speed( int fd, int speed )
{
	printf("new speed: 0x%x\n", speed );
	if( fd>0 )
		write( fd, &speed, sizeof(speed) );
}

static __u16 read_with_time( int fd, unsigned long *time1 )
{
	struct timespec timestamp;
	__u16 state;
	ssize_t ret;

	ret=read( fd, &state, sizeof(state) );
	if (ret<0) {
		perror( "read" );
		return -1;
	}
	clock_gettime(CLOCK_MONOTONIC,&timestamp);
	*time1 = (timestamp.tv_sec * 1000000)+(timestamp.tv_nsec/1000);
	return state;
}

static inline int is_sling( __u16 state )
{
	return (state&0xf000)==0x0000;
}

static char *decode( __u16 status )
{
	if( (status&0xf000)==0x0000 )
		return "Auslenkungssensor";
	if( (status&0xf000)==0x1000 )
		return "Start/Ziel";
	if( (status&0xf000)==0x2000 )
		return "Gefahrenstelle";
	if( (status&0xf000)==0x3000 )
		return "NICHT KODIERT";
	if( (status&0xf000)==0x4000 )
		return "NICHT KODIERT";
	if( (status&0xf000)==0x5000 )
		return "NICHT KODIERT";
	if( (status&0xf000)==0x6000 ) {
		if( (status&0x0400)==0x0400 )
			return "Brueckenende";
		else
			return "Brueckenanfang";
	}
	if( (status&0xf000)==0x7000 )
		return "NICHT KODIERT";
	if( (status&0xf000)==0x8000 )
		return "Kurve";
	if( (status&0xf000)==0x9000 )
		return "NICHT KODIERT";
	if( (status&0xf000)==0xa000 )
		return "NICHT KODIERT";
	if( (status&0xf000)==0xb000 )
		return "NICHT KODIERT";
	if( (status&0xf000)==0xc000 )
		return "NICHT KODIERT";
	if( (status&0xf000)==0xd000 )
		return "NICHT KODIERT";
	if( (status&0xf000)==0xe000 )
		return "NICHT KODIERT";
	if( (status&0xf000)==0xf000 )
		return "NICHT KODIERT";
	return "TYP UNBEKANNT";
}

static void exploration( int fdc )
{
	__u16 state_old, state_act;
	unsigned long time_act, time_old;
	int rounds=0, length, length_sum=0, v;

	do {
		state_old = read_with_time( fdc, &time_old );
	} while( (state_old&0xf000)!=0x1000 ); // fahre bis Start
	while( rounds<2 ) {
		do {
			state_act=read_with_time( fdc, &time_act );
		} while( is_sling(state_act) );
		printf("old/act: 0x%x/0x%x\t%6.6ld [ms] - ",
			state_old, state_act,
			(time_act-time_old)/1000 );
		if( state_act == state_old ) {
			v = 22000000/(time_act-time_old);
			if( (state_act&0xf000)==0x1000 ) {
				rounds++;
				last_time = time_act;
				printf("Round: %d - %d [mm]\n",
					rounds, length_sum );
				length_sum = 0;
			}
			printf("v=0,%d\n", v );
		} else {
			length = v * (time_act - time_old)/1000000;
			length_sum += length;
			printf("%s: length=%d\n", decode(state_act),length);
			add_to_liste( state_act, length );
		}
		state_old = state_act;
		time_old = time_act;
	}
}

static void tracking( int rounds_to_go )
{
	struct _liste *position = root;
	int rounds=0;
	__u16 state_act;
	unsigned long time_act, besttime=(-1), meantime=0;

	do {
		do {
			state_act=read_with_time( fdc, &time_act );
		} while( is_sling(state_act) );
		state_act=read_with_time( fdc, &time_act );
		printf("0x%04x (expected 0x%04x)\n",state_act,position->type);
		if( state_act != position->type){
			printf("wrong position 0x%04x (0x%04x)\n",
				state_act, position->type);
			while ( state_act != position->type) {
				position = position->next;
			}
		}
		if( (state_act&0xf000)==0x1000 ) { // Start/Ziel
			rounds++;
			rounds_to_go--;
			if( last_time ) {
				if( (time_act-last_time)<besttime )
					besttime = time_act-last_time;
				meantime += time_act-last_time;
				printf("\n---> Runde: %d - Zeit: %ld.%03lds "
					"(Beste: %ld.%03lds, "
					"Mean: %ld.%03lds)\n",
					rounds,
				make_seconds((time_act-last_time)),
				make_mseconds((time_act-last_time)),
				make_seconds(besttime),
				make_mseconds(besttime),
				make_seconds(meantime/rounds),
				make_mseconds(meantime/rounds));
			}
			last_time = time_act;
		}
		position = position->next;
	} while( rounds_to_go );
}

void *gegner( void *args ){
	
	int ret;
	int fd;
	int status;
	int error;

	struct timespec sleeptime;
	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = 250000000;
	
	fd = open ("/dev/Carrera.other", O_RDONLY);
	if ( fd < 0){
		printf("Fehler beim Öffnen /dev/Carrera.other");
		return NULL;
	}
	
	while(1){
		ret = read(fd, &status, sizeof(status));
		printf("Gengner Status: %4.4x\n", status);
		if ((error=clock_nanosleep(CLOCK_MONOTONIC,0,&sleeptime,NULL))!=0){
			printf("clock_nanosleep reporting error %d\n",error);
		}	
	}
	
	close(fd);
	return NULL;
}

int main( int argc, char **argv )
{
	int rounds_to_go=10;
	struct sigaction new_action;

	fdc = open( "/dev/Carrera", O_RDWR );
	if( fdc<0 ) {
		perror( "/dev/Carrera" );
		return -1;
	}
	if( argc > 1 ) {
		basis_speed_in=basis_speed_out=
			(unsigned char)strtoul(argv[1],NULL,0);
		if( argc > 2 ) {
			rounds_to_go = (unsigned int)strtoul(argv[2],NULL,0);
		}
	}
	new_action.sa_handler = exithandler;
	sigemptyset( &new_action.sa_mask );
	new_action.sa_flags = 0;
	sigaction( SIGINT, &new_action, NULL );

	set_speed( fdc, basis_speed_in );
	exploration( fdc );
	print_liste();
	
	//Gegnerthread
	pthread_t gegner_thread;
	if( pthread_create (&gegner_thread, NULL, gegner, NULL)!=0){
		fprintf(stderr, "Creation of gegner_thread failed");
		return -1;
	}

	tracking( rounds_to_go );
	set_speed( fdc, 0x0 );
	return 0;
}
