#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#ifdef __QNX__
  #include <hw/pci.h>
  #include <hw/inout.h>
  #include <sys/neutrino.h>
  #include <sys/mman.h>
#endif
#include "registers.h"

#define SWITCHES 0
#define ATTEN    1
#define READ     0
#define WRITE    1
#define ON       1
#define OFF      0

#define NEW_PMAT 1 
#define read_lookup_table 1 
#define CARDS 20 
#define PHASECODES 8192 
#define BEAMCODES 8192 
#define MAX_FREQS 1201 
#define MAX_ANGLES 32 
#define MAX_FSTEPS 16 
#define F_OFFSET 64 
#define MIN_FREQ 11.9E6
#define MAX_FREQ 12.1E6
extern char *optarg;
int32_t sock=-1;
int32_t verbose=2;
char beamfile_prefix[20]="beamcodes_cal";
char file_ext[20]=".dat";
char filename[120];
char dir[80];
FILE *beamcodefile=NULL;
FILE *beamtablefile=NULL;
char radar_name[80];

struct timeval t0,t1,t2,t3;
unsigned long elapsed;





int32_t main(int argc, char **argv)
{
  int opt;
  int read_table=read_lookup_table,read_matrix=0;
  char *caldir=NULL;
  double *pwr_mag[MAX_FREQS];
  double freqs[MAX_FREQS];
  double angles[MAX_ANGLES];
  int32_t lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  double *max_pwr_mag=NULL;
  int32_t *final_phasecodes[CARDS], *final_attencodes[CARDS];
  double *final_angles[CARDS];
  double *final_freqs[CARDS];
  int32_t a=0,f=0,i=0,card=0,c=0,b=0,rval=0,count=0,attempt=0; 
  int32_t num_freqs,max_angles,num_angles,num_beamcodes,num_fsteps,fstep,foffset,num_cards;
  double fextent=0.0,f0=0.0,fm=0.0,df=0.0,angle,freq,min_freq=MIN_FREQ,max_freq=MAX_FREQ;
  int32_t requested_phasecode=0, requested_attencode=0,beamcode=0;
  int32_t radar,loop=0,read=0;
  uint32_t portA0,portB0,portC0,cntrl0 ;
  uint32_t portA1,portB1,portC1,cntrl1 ;
  int32_t		 temp, pci_handle, j,  IRQ  ;
  unsigned char	 *BASE0, *BASE1;
  uint32_t	 mmap_io_ptr,IOBASE, CLOCK_RES;
  float		 time;
  int32_t tfreq=0,beamnm=0,best_fstep=0;
  double  fdiff=0.,tdiff=0.,best_angle=0.,best_freq=0.;
  int32_t best_phasecode=0,best_attencode=0;
#ifdef __QNX__
	struct		 _clockperiod new, old;
	struct		 timespec start_p, stop_p, start, stop, nsleep;
#endif
  radar=0;
  while ((opt = getopt(argc, argv, "n:c:lf:b:rRh")) != -1) {
    switch (opt) {
      case 'n':
        radar=atoi(optarg);
        break;
      case 'l':
        loop=1;
        break;
      case 'c':
        card=atoi(optarg);
        break;
      case 'f':
        tfreq=atoi(optarg);
        break;
      case 'b':
        beamnm=atoi(optarg);
        break;
      case 'r':
        read_table=1;
        break;
      case 'R':
        read_matrix=1;
        break;
      case 'h':
      default: /* '?' */
        fprintf(stderr, "Usage: %s [-lrR] [-f] tfreq {Khz} [-b] beam {0-31} [-c] first_card [-n] radar number \n\t l: loop\n\t r: Read existing loopup table\n", argv[0]); 
        fprintf(stderr, "\t This program lets you select a freq optimized beamcode from the lookup table on a card and then verify the phasing and attenuation values stored in the table\n"); 
        exit(EXIT_FAILURE);
    }
  }
  if(read_matrix) {
    printf("Radar: %d\n",radar);
    if((radar !=1) && (radar != 2) ) {
      fprintf(stderr,"%s: invoke with -n 1 or -n 2 to set radar dio outputs correctly\n",argv[0]);
      exit(EXIT_FAILURE);
    }
  } else {
    radar=1;
  }
  caldir=getenv("MSI_CALDIR");
  if (caldir==NULL) {
    caldir=strdup("/data/calibrations/");
  }
  fprintf(stdout,"CALDIR: %s\n",caldir);
    printf("Loop: %d\n",loop);
    printf("\n\nEnter Radar Name: ");
    fflush(stdin);
    fflush(stdout);
    scanf("%s", &radar_name);
    fflush(stdout);
    fflush(stdin);
    printf("Radar: <%s>  Card: %d\n",radar_name,card);
    fflush(stdout);
    sprintf(dir,"/%s/%s/",caldir,radar_name);
    switch(radar) {
      case 1:
            portC0=PC_GRP_0;
            portC1=PC_GRP_1;
            portB0=PB_GRP_0;
            portB1=PB_GRP_1;
            portA0=PA_GRP_0;
            portA1=PA_GRP_1;
            cntrl0=CNTRL_GRP_0;
            cntrl1=CNTRL_GRP_1;
        break;
      case 2:
            portC0=PC_GRP_2;
            portC1=PC_GRP_3;
            portB0=PB_GRP_2;
            portB1=PB_GRP_3;
            portA0=PA_GRP_2;
            portA1=PA_GRP_3;
            cntrl0=CNTRL_GRP_2;
            cntrl1=CNTRL_GRP_3;
        break;
      case 3:
            portC0=PC_GRP_1;
            portC1=PC_GRP_3;
            portB0=PB_GRP_1;
            portB1=PB_GRP_3;
            portA0=PA_GRP_1;
            portA1=PA_GRP_3;
            cntrl0=CNTRL_GRP_1;
            cntrl1=CNTRL_GRP_3;
        break;
      default:
        fprintf(stderr,"Invalid radar number %d",radar);
        exit(-1);
    } 
#ifdef __QNX__
    /* SET THE SYSTEM CLOCK RESOLUTION AND GET THE START TIME OF THIS PROCESS */
	new.nsec=10000;
	new.fract=0;
	temp=ClockPeriod(CLOCK_REALTIME,&new,0,0);
	if(temp==-1){
		perror("Unable to change system clock resolution");
	}
	temp=clock_gettime(CLOCK_REALTIME, &start_p);
	if(temp==-1){
		perror("Unable to read sytem time");
	}
	temp=ClockPeriod(CLOCK_REALTIME,0,&old,0);
	CLOCK_RES=old.nsec;
	printf("CLOCK_RES: %d\n", CLOCK_RES);
    /* OPEN THE PLX9656 AND GET LOCAL BASE ADDRESSES */
	fprintf(stderr,"PLX9052 CONFIGURATION ********************\n");
	clock_gettime(CLOCK_REALTIME, &start);
	temp=_open_PLX9052(&pci_handle, &mmap_io_ptr, &IRQ, 1);
	IOBASE=mmap_io_ptr;
	if(temp==-1){
		fprintf(stderr, "	PLX9052 configuration failed");
	}
	else{
		fprintf(stderr, "	PLX9052 configuration successful!\n");
	}
	printf("IOBASE=%x\n",IOBASE);
    /* INITIALIZE THE CARD FOR PROPER IO */
	// GROUP 0 - PortA=output, PortB=output, PortClo=output, PortChi=output
	out8(IOBASE+cntrl0,0x80);
	// GROUP 1 - PortAinput, PortB=input, PortClo=input, PortChi=output
	out8(IOBASE+cntrl1,0x93);
	out8(IOBASE+portA0,0x00);
	out8(IOBASE+portB0,0x00);
	out8(IOBASE+portC0,0x00);
	out8(IOBASE+portA1,0x00);
	out8(IOBASE+portB1,0x00);
	temp=in8(IOBASE+portC1);
	temp=temp & 0x0f;
	printf("input on group 1, port c is %x\n", temp);
#endif

  for (c=0;c<CARDS;c++) {
    final_phasecodes[c]=calloc(BEAMCODES,sizeof(int32_t));
    final_attencodes[c]=calloc(BEAMCODES,sizeof(int32_t));
    final_angles[c]=calloc(BEAMCODES,sizeof(double));
    final_freqs[c]=calloc(BEAMCODES,sizeof(double));
    for (b=0;b<BEAMCODES;b++) {
      final_phasecodes[c][b]=-1;
      final_attencodes[c][b]=-1;
      final_angles[c][b]=-1;
      final_freqs[c][b]=-1;
    }
  }
  fflush(stdout);
  fflush(stdin);

  if(read_table) {
      sprintf(filename,"%s/beamcode_lookup_table_%s.dat",dir,radar_name,c);
      beamtablefile=fopen(filename,"r");
      printf("%p %s\n",beamtablefile,filename);
      if(beamtablefile!=NULL) {
        printf("Reading from saved beamcode lookup table\n"); 
        fread(&num_freqs,sizeof(int32_t),1,beamtablefile);
        fread(&num_angles,sizeof(int32_t),1,beamtablefile);
        printf("Num angles: %d\n",num_angles);
        fread(&max_angles,sizeof(int32_t),1,beamtablefile);
        fread(&num_beamcodes,sizeof(int32_t),1,beamtablefile);
        fread(&num_fsteps,sizeof(int32_t),1,beamtablefile);
        printf("Num fsteps: %d\n",num_fsteps);
        fread(&fstep,sizeof(int32_t),1,beamtablefile);
        fread(&foffset,sizeof(int32_t),1,beamtablefile);
        printf("Foffset: %d\n",foffset);
        fread(&f0,sizeof(double),1,beamtablefile);
        fread(&fm,sizeof(double),1,beamtablefile);
        fread(&num_cards,sizeof(int32_t),1,beamtablefile);
        fread(freqs,sizeof(double),num_freqs,beamtablefile);
        fread(angles,sizeof(double),num_angles,beamtablefile);
        if(num_fsteps!=MAX_FSTEPS) {
          printf("Wrong number of fsteps %d %d\n",num_fsteps,MAX_FSTEPS);
        }
        printf("Counting saved codes in lookup table\n");
        for (c=0;c<CARDS;c++) { 
          count=0;
          rval=fread(final_phasecodes[c],sizeof(int32_t),num_beamcodes,beamtablefile);
          rval=fread(final_attencodes[c],sizeof(int32_t),num_beamcodes,beamtablefile);
          rval=fread(final_freqs[c],sizeof(int32_t),num_beamcodes,beamtablefile);
          rval=fread(final_angles[c],sizeof(int32_t),num_beamcodes,beamtablefile);
          for (b=0;b<BEAMCODES;b++) {
           if (final_phasecodes[c][b] >= 0 ) {
              count++; 
            }
          }  
          //printf("Lookup Card: %d Count: %d\n",c,count);
        }
        fclose(beamtablefile);
        beamtablefile=NULL;
        if (beamnm < 0)  beamnm=0;
        if (beamnm >= 32)  beamnm=31;
        read=1;
      } else {
        fprintf(stderr,"Error writing beam lookup table file\n");
        read=0;
      }
  }
  c=-1; 
  if (card==-1) {
    if(loop==0) {
    printf("\n\nEnter Phasing Card Number: ");
    fflush(stdin);
    fflush(stdout);
    scanf("%d", &card);
    fflush(stdout);
    fflush(stdin);
    } else {
      card=0;
      printf("Radar: <%s>  All Cards\n",radar_name);
    }
  }
  c=card;
  while((c<CARDS) && (c >=0)) {
    if (tfreq > 0) {
      a=beamnm;
      fdiff=fm;
      best_fstep=0;
      best_freq=0.0;
      best_phasecode=0;
      best_attencode=0;
      for (f=0;f<num_fsteps;f++) {
        b=f*max_angles+a+foffset;    
        tdiff=fabs(final_freqs[c][b]-(double)tfreq*1E3);
        if(tdiff < fdiff) {
          fdiff=tdiff;
          best_fstep=f;
          best_freq=final_freqs[c][b];
          best_angle=final_angles[c][b];
          best_phasecode=final_phasecodes[c][b];
          best_attencode=final_attencodes[c][b];
          beamcode=b;
        }    
      }
    } else {
      beamcode=beamnm;
      b=beamnm;
      best_fstep=0.0;
      best_freq=final_freqs[c][b];
      best_angle=final_angles[c][b];
      best_phasecode=final_phasecodes[c][b];
      best_attencode=final_attencodes[c][b];
    }
    printf("Card: %2d Beam: %6d Tfreq [Khz]: %8d :: Beamcode: %8d :: Phase: %8d Atten: %2d F[Mhz]: %8.3lf A[deg]: %8.3lf\n",c,beamnm,tfreq,beamcode,best_phasecode,best_attencode,best_freq,best_angle);
    temp=beam_code(IOBASE,beamcode,radar);
    if(NEW_PMAT) {
      temp=verify_data_new(IOBASE,c,beamcode,best_phasecode,radar,0);
      temp=verify_attenuators(IOBASE,c,beamcode,best_attencode,radar);
    } else {

    }
    if(loop) c++;
    else c=-1;
  }
//JDS write to lookup table
}

