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
static int gegner_fdc=0;
struct _liste *gegner_position = NULL;
static unsigned long gegner_time = 0;
//static unsigned char brake = 0x0b, clear_brake = 0x0c;

int auslenkung_innen = -1;
int auslenkung_aussen = -1;

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
	int auslenkung = 0;
        if ( (state&0xf000)==0x0000 ){
                // Es liegt eine Auslenkung vor...
		auslenkung = (int)(state&0x000f);

                // Die Auslenkung wird abhaengig von der Spur
                // in auslenkung_innen bzw. auslenkung_aussen abgespeichert
                if ( (state&0x0800)==0x0000){
                        // auslenkung innen
                        auslenkung_innen = auslenkung;
                }
                if ( (state&0x0800)==0x0800){
                        // auslenkung aussen
                        auslenkung_aussen = auslenkung;
                }
                printf("Aussen: %d, Innen: %d\n", auslenkung_aussen, auslenkung_innen);
                return 1;
        }
        printf("Aussen: %d, Innen: %d\n", auslenkung_aussen, auslenkung_innen);
        return 0; // keine Auslenkung

}

int change_speed()
{
        switch(auslenkung_innen)
        {
                case -1: basis_speed_in += 6;
                        break;
                case 0: basis_speed_in += 0;
                        break;
                case 1: basis_speed_in += 5;
                        break;
                case 2: basis_speed_in += 3;
                        break;
                case 3: basis_speed_in += 3;
                        break;
                case 4: basis_speed_in -= 2;
                        break;
                case 5: basis_speed_in -= 1;
                        break;
                case 6: basis_speed_in -= 4;
                        break;
                case 7: basis_speed_in -= 3;
                        break;
                default: break;
        }
        switch(auslenkung_aussen)
        {
                case -1: basis_speed_out += 7;
                        break;
                case 0: basis_speed_out += 4;
                        break;
                case 1: basis_speed_out += 4;
                        break;
                case 2: basis_speed_out += 1;
                        break;
                case 3: basis_speed_out += 3;
                        break;
                case 4: basis_speed_out -= 5;
                        break;
                case 5: basis_speed_out -= 1;
                        break;
                case 6: basis_speed_out -= 0;
                        break;
                case 7: basis_speed_out -= 3;
                        break;
                default: break;
        }
        return 1;
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
	struct timespec sleeptime;
        unsigned long  diff_zeit;
	struct timespec timestamp;

	do {
		do {
			state_act=read_with_time( fdc, &time_act );
		} while( is_sling(state_act) );
		
		state_act=read_with_time( fdc, &time_act );
		printf("0x%04x (expected 0x%04x)\n",state_act,position->type);
		if( state_act != position->type){
			printf("wrong position 0x%04x (0x%04x)\n",
				state_act, position->type);
			while ( state_act != position->type)
				position = position->next;
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

		else if ((state_act & 0xf000) == 0x2000){  // Kreuzungsstelle
			sleeptime.tv_sec = 0;
                	sleeptime.tv_nsec = 100000000;	//100 ms
                	
			diff_zeit = time_act - gegner_time;
			
			printf("Eigenfahrzeug %4.4x - Gegnerfahrzeug  %4.4x, diff_zeit %lu\n", position->type, gegner_position->type,diff_zeit);
			
			while (( position == gegner_position ) && ( diff_zeit < 2500000 )) {
				//Gegner im gleichen Segment und diff kleiner als 2500ms
				set_speed(fdc, 0x0b);
				//write(fdc, &brake, sizeof(brake));
				printf("Gegner auch in Gefahrenstelle seit : %ld\n ms", diff_zeit);
				//Schlafe
				clock_nanosleep(CLOCK_MONOTONIC, 0, &sleeptime, NULL);
				//Messe die Zeitunterschied neu
				clock_gettime(CLOCK_MONOTONIC,&timestamp);
				time_act = (timestamp.tv_sec * 1000000)+(timestamp.tv_nsec/1000);
				diff_zeit = time_act - gegner_time;
			}
			//Clear brake
			set_speed(fdc, 0x0c);
			//write(fdc, &clear_brake, sizeof(clear_brake));
		}
		position = position->next;
		change_speed();
		if ( (state_act&0x0800)==0x0000){
                        // innen
                        set_speed(fdc, basis_speed_in);
                }
                if ( (state_act&0x0800)==0x0800){
                        // aussen
                        set_speed(fdc, basis_speed_out);
                }
	} while( rounds_to_go );
}

//void *gegner( void *args ){
//	
//	int ret;
//	int fd;
//	int status;
//	int error;
//
//	struct timespec sleeptime;
//	sleeptime.tv_sec = 0;
//	sleeptime.tv_nsec = 250000000;
//	
//	fd = open ("/dev/Carrera.other", O_RDONLY);
//	if ( fd < 0){
//		printf("Fehler beim Öffnen /dev/Carrera.other");
//		return NULL;
//	}
//	
//	while(1){
//	if (open ("/dev/Carrera.other", O_RDONLY) < 0){
//		ret = read(fd, &status, sizeof(status));
//		printf("Gengner Status: %4.4x\n", status);
//		if ((error=clock_nanosleep(CLOCK_MONOTONIC,0,&sleeptime,NULL))!=0){
//			printf("clock_nanosleep reporting error %d\n",error);
//		}
//
//		}
//	}
//	
//	close(fd);
//	return NULL;
//}

void *gegner_tracking( void *args ){
	
	gegner_position = root;
	int error;
	unsigned long time_act;
	__u16 state_old = read_with_time(gegner_fdc, &time_act);
	state_old ^= 0x0800;
	__u16 state_act = 0;
	struct timespec gegner_timestamp;
	
	struct timespec gegner_sleeptime;
	gegner_sleeptime.tv_sec = 0;
	gegner_sleeptime.tv_nsec = 5000000;

	while (1) {
		do {
			if ((error=clock_nanosleep(CLOCK_MONOTONIC,0,&gegner_sleeptime,NULL))!=0){
				printf("clock_nanosleep reporting error %d\n",error);
			}
			state_act = read_with_time(gegner_fdc, &time_act);
			state_act ^= 0x0800;
		} while (state_old == state_act || is_sling(state_act));

		state_old = state_act;

		clock_gettime(CLOCK_MONOTONIC,&gegner_timestamp);
		gegner_time = (gegner_timestamp.tv_sec * 1000000)+(gegner_timestamp.tv_nsec/1000);
		//gegner_time = time_act;
		
		while (gegner_position->type != state_act)
			gegner_position = gegner_position->next;
	
	}
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
	

	gegner_fdc = open ("/dev/Carrera.other", O_RDONLY);
	if ( gegner_fdc < 0){
		perror( "/dev/Carrera.other" );
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
	
	// Gegner-thread

	pthread_t gegner_thread;
	if( pthread_create (&gegner_thread, NULL, gegner_tracking, NULL)!=0){
		fprintf(stderr, "Creation of gegner_thread failed");
		return -1;
	}

	tracking( rounds_to_go );

	pthread_join(gegner_thread, NULL); //auf thread beendigung warten

	set_speed( fdc, 0x0 );
	return 0;
}
