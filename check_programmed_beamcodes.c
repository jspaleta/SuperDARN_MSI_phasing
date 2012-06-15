#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
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
#define CARDS 32 
#define PHASECODES 8192 
#define BEAMCODES 8192 
#define MAX_FREQS 1300
#define MAX_ANGLES 200
#define SIMPLE_BEAMS 32
int sock=-1;
int verbose=0;
double atten,tdelay,delays[13]={0.25,0.45,0.8,1.5,2.75,5.0,8.0,15.0,25.0 ,45.0,80.0,140.0,250.0};
double attens[5]={0.5,1.0,2.0,4.0,8.0};
char summaryfile_prefix[20]="beamcodes";
char beamfile_prefix[20]="beamcodes_cal";
char file_ext[20]=".dat";
char filename[120];
char dir[80];
FILE *beamcodefile=NULL;
FILE *beamtablefile=NULL;
FILE *summaryfile=NULL;
char radar_name[80];

struct timeval t0,t1,t2,t3;
unsigned long elapsed;
/*-GET_DEALY---------------------------------------------------------*/
float get_delay(int code){
	
	int	i;
	float	delay;
	float	delaylist[]={0.25, 0.45, 0.8, 1.5, 2.75, 5, 8, 15, 25, 45, 80, 140, 250};

	delay=0;
	for(i=0;i<13;i++){
		delay+=( (code & (int)pow(2,i)) >> i)*delaylist[i];
	}	
	return delay;
}


int main(int argc, char **argv)
{
  double tdelay,atten;
  int code;
  double *pwr_mag[MAX_FREQS];
  double freqs[MAX_FREQS];
  double angles[MAX_ANGLES],std_angles[MAX_ANGLES];
  int lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  int *best_phasecode[MAX_FREQS], *best_std_phasecode[MAX_FREQS];
  int *best_attencode[MAX_FREQS], *best_std_attencode[MAX_FREQS];
  int *final_beamcodes[CARDS], *final_attencodes[CARDS];
  double *final_angles[CARDS];
  double *final_freqs[CARDS];
  int a=0,i=0,card=0,c=0,b=0,rval=0,count=0,attempt=0; 
  int num_freqs,num_angles,num_std_angles,num_beamcodes,num_cards;
  int std_angle_index_offset=0,angle_index_offset=192;
  double df=0.0,angle,freq;
  int requested_phasecode=0, requested_attencode=0,beamcode=0;
  int radar,loop=1,read=0;
  unsigned int portA0,portB0,portC0,cntrl0 ;
  unsigned int portA1,portB1,portC1,cntrl1 ;
	int		 temp, pci_handle, j,  IRQ  ;
	unsigned char	 *BASE0, *BASE1;
	unsigned int	 mmap_io_ptr,IOBASE, CLOCK_RES;
	float		 time;
#ifdef __QNX__
	struct		 _clockperiod new, old;
	struct		 timespec start_p, stop_p, start, stop, nsleep;
#endif
  
    if(argc <2 ) {

      fprintf(stderr,"%s: invoke with radar number (1 or 2)\n",argv[0]);
      fprintf(stderr,"%s:   optional second argument: loop over cards (1)\n",argv[0]);
      exit(0);
    }
    radar=atoi(argv[1]);
    //printf("Radar: %d\n",radar);
    if (argc==3) loop=atoi(argv[2]);
    //printf("Loop: %d\n",loop);
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
            sprintf(radar_name,"kansas_tx_east");
            sprintf(dir,"/root/site_data/site.kansas/calibrations/fhe/");
//            sprintf(radar_name,"cvw");
//            sprintf(dir,"/root/site_data/site.oregon/calibrations/cvw/");
//            sprintf(radar_name,"mcm");
//            sprintf(dir,"/root/site_data/site.mcm/calibrations/mcm/");
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
            sprintf(radar_name,"cvw");
            sprintf(dir,"/root/site_data/site.oregon/calibrations/cvw/");
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
            sprintf(radar_name,"cvw");
            sprintf(dir,"/root/site_data/site.oregon/calibrations/cvw/");
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
	//printf("CLOCK_RES: %d\n", CLOCK_RES);
    /* OPEN THE PLX9656 AND GET LOCAL BASE ADDRESSES */
	fprintf(stderr,"PLX9052 CONFIGURATION ********************\n");
	clock_gettime(CLOCK_REALTIME, &start);
	temp=_open_PLX9052(&pci_handle, &mmap_io_ptr, &IRQ, 0);
	IOBASE=mmap_io_ptr;
	if(temp==-1){
		fprintf(stderr, "	PLX9052 configuration failed");
	}
	else{
		fprintf(stderr, "	PLX9052 configuration successful!\n");
	}
	//printf("IOBASE=%x\n",IOBASE);
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
	//printf("input on group 1, port c is %x\n", temp);
#endif

  for (c=0;c<CARDS;c++) {
    final_beamcodes[c]=calloc(BEAMCODES,sizeof(int));
    final_attencodes[c]=calloc(BEAMCODES,sizeof(int));
    final_angles[c]=calloc(BEAMCODES,sizeof(double));
    final_freqs[c]=calloc(BEAMCODES,sizeof(double));
    for (b=0;b<BEAMCODES;b++) {
      final_beamcodes[c][b]=-1;
      final_attencodes[c][b]=-1;
      final_angles[c][b]=-1;
      final_freqs[c][b]=-1;
    }
  }
  fflush(stdout);
  fflush(stdin);

  if(read_lookup_table) {
      sprintf(filename,"%s/beamcode_lookup_table_%s.dat",dir,radar_name,c);
      beamtablefile=fopen(filename,"r+");
      fprintf(stderr,"%p %s\n",beamtablefile,filename);
      if(beamtablefile!=NULL) {
        //printf("Reading from saved beamcode lookup table\n"); 
        fread(&num_freqs,sizeof(int),1,beamtablefile);
        //printf("%d\n",num_freqs); 
        fread(&num_std_angles,sizeof(int),1,beamtablefile);
        //printf("%d\n",num_std_angles); 
        fread(&num_angles,sizeof(int),1,beamtablefile);
        //printf("%d\n",num_angles); 
        fread(&num_beamcodes,sizeof(int),1,beamtablefile);
        //printf("%d\n",num_beamcodes); 
        fread(&num_cards,sizeof(int),1,beamtablefile);
        //printf("%d\n",num_cards); 
        fread(&std_angle_index_offset,sizeof(int),1,beamtablefile);
        //printf("%d\n",std_angle_index_offset); 
        fread(&angle_index_offset,sizeof(int),1,beamtablefile);
        //printf("%d\n",angle_index_offset); 
        fread(freqs,sizeof(double),num_freqs,beamtablefile);
        //printf("got freqs\n"); 
        fread(std_angles,sizeof(double),num_std_angles,beamtablefile);
        fread(angles,sizeof(double),num_angles,beamtablefile);

        //printf("Counting saved codes in lookup table\n");
        for (c=0;c<CARDS;c++) { 
          count=0;
          rval=fread(final_beamcodes[c],sizeof(int),num_beamcodes,beamtablefile);
          rval=fread(final_attencodes[c],sizeof(int),num_beamcodes,beamtablefile);
          for (b=0;b<BEAMCODES;b++) {
           if (final_beamcodes[c][b] >= 0 ) {
              count++; 
            }
          }  
          //printf("Lookup Card: %d Count: %d\n",c,count);
        }
        fclose(beamtablefile);
        beamtablefile=NULL;
        read=1;
      } else {
        fprintf(stderr,"Error writing beam lookup table file\n");
        read=0;
      }
  }

  if(loop==0) {
    c=-1; 
    printf("\n\nEnter Phasing Card Number: ");
    fflush(stdin);
    fflush(stdout);
    scanf("%d", &card);
    //printf("Radar: <%s>  Card: %d\n",radar_name,card);
    fflush(stdout);
  } else {
    card=0;
    printf("Radar: <%s>  All Cards\n",radar_name);
  }
  c=card;
/*
  while((c<CARDS) && (c >=0)) {
      sprintf(filename,"%s%s_%s_%d%s",dir,beamfile_prefix,radar_name,c,file_ext);
      printf("File: %s\n",filename);
      beamcodefile=fopen(filename,"r");
      if(beamcodefile!=NULL) {
        fread(&temp,sizeof(int),1,beamcodefile);
        if(temp!=num_freqs && read ) fprintf(stderr,"num_freq mismatch! %d %d\n",temp,num_freqs); 
        num_freqs=temp;
        printf("Num freqs: %d\n",num_freqs);
        fread(freqs,sizeof(double),num_freqs,beamcodefile);
        df=(double)freqs[1]-(double)freqs[0];

        printf("freq 0: %g df: %lf\n",freqs[0],df);
        printf("freq max: %g df: %lf\n",freqs[num_freqs-1],df);
        fread(&temp,sizeof(int),1,beamcodefile);
        if(temp!=num_std_angles && read) fprintf(stderr,"num_std_angles mismatch! %d %d\n",temp,num_std_angles); 
        num_std_angles=temp;
        printf("Num standard angles: %d\n",num_std_angles);
        fread(std_angles,sizeof(double),num_std_angles,beamcodefile);
        printf("\n");
        for(i=0;i<num_freqs;i++) {
          best_std_phasecode[i]=calloc(num_std_angles,sizeof(int));
          rval=fread(best_std_phasecode[i],sizeof(int),num_std_angles,beamcodefile);
          if( rval!=num_std_angles) fprintf(stderr,"Error reading std_phasecodes for freq: %d :: %d %d\n",i,rval,num_std_angles);
          best_std_attencode[i]=calloc(num_std_angles,sizeof(int));
          rval=fread(best_std_attencode[i],sizeof(int),num_std_angles,beamcodefile);
          if( rval!=num_std_angles) fprintf(stderr,"Error reading std attencodes for freq: %d :: %d %d\n",i,rval,num_std_angles);
        }
        printf("  Freq  ::  Bmnum  ::  Ang   ::  Phasecode :: Attencode\n");
        for(a=0;a<num_std_angles;a++) printf("%8.0lf :: %8d :: %6.1lf :: %8d :: %8d\n",freqs[0],a,std_angles[a],best_std_phasecode[0][a],best_std_attencode[0][a]);
 
        fread(&temp,sizeof(int),1,beamcodefile);
        if(temp!=num_angles && read) fprintf(stderr,"num_angles mismatch! %d %d\n",temp,num_angles); 
        num_angles=temp;
        printf("Num angles: %d\nAngles: ",num_angles);
        fread(angles,sizeof(double),num_angles,beamcodefile);
        for(a=0;a<num_angles;a++) printf("%4.1lf ",angles[a]);
        printf("\n");
        for(i=0;i<num_freqs;i++) {
          best_phasecode[i]=calloc(num_angles,sizeof(int));
          rval=fread(best_phasecode[i],sizeof(int),num_angles,beamcodefile);
          best_attencode[i]=calloc(num_angles,sizeof(int));
          rval=fread(best_attencode[i],sizeof(int),num_angles,beamcodefile);

          if( rval!=num_angles) fprintf(stderr,"Error reading angles for freq: %d :: %d %d\n",i,rval,num_angles);
        }

        fclose(beamcodefile); //beamcode file
        read=1;
        for (b=0;b<BEAMCODES;b++) {
          i=(b)/100;
          if (b<angle_index_offset) {
            i=b/num_std_angles*(int)(1E6/df);
            a=b%num_std_angles;
            requested_phasecode=best_std_phasecode[i][a];
            requested_attencode=best_std_attencode[i][a];
            angle=std_angles[a];
            freq=freqs[i];
//            printf("calculate: %d %d %lf %lf\n",b,i,freq,freqs[i]);
          } else {
            i=(b-angle_index_offset)/num_angles*2E5/df;
            a=(b-angle_index_offset) % num_angles ;
            if (i < num_freqs) {
              requested_phasecode=best_phasecode[i][a];
              requested_attencode=best_attencode[i][a];
              angle=angles[a];
              freq=freqs[i];
            } else {
              requested_phasecode=-1;
              angle=-1E10;
              freq=-1E10;
            }
          } 
          final_beamcodes[c][b]=requested_phasecode;
          final_attencodes[c][b]=requested_attencode;
          final_freqs[c][b]=freq;

          final_angles[c][b]=angle;
        }
      } else {
        printf("Bad beamcode file\n");
      }
      if(loop) c++;
      else c=-1;
  } // end of card loop
*/
  c=card;
  printf("  Radar   ,    Card  ,   Beam   ,   Pcode  ,  Delay, Acode   , Atten\n");
 while((c<CARDS) && (c >=0)) {
      for (b=0;b<=24;b++) {
         if (final_beamcodes[c][b] >= 0 ) {
              if(NEW_PMAT) {
                delay(10);
                if(b < num_std_angles) {
		  tdelay=0.0;
      		  for(i=0; i<13;i++ ) {
         	    code= (1 << i);
         		if ((final_beamcodes[c][b] & code) == code) tdelay+=delays[i];
      		  }
      		  atten=0.0;
      		  for(i=0; i<5;i++ ) {
         	    code= (1 << i);
         	    if ((final_attencodes[c][b] & code) == code) atten+=attens[i];
      		  }
                  printf("  %s , %4d , %4d , %5d , %8.3lf , %5d , %8.3lf\n"
                  ,radar_name,c,b,final_beamcodes[c][b],tdelay,final_attencodes[c][b],atten);
                }
              } else {
                if(b < num_std_angles) {
                  if (final_beamcodes[c][b] >= 0 ) {
                    printf("Radar: %d  Beam: %d Phasecode: 0x%x %d\n",radar,b,final_beamcodes[c][b],final_beamcodes[c][b]);
                  }
                }
              }
         }
      } //end beamcode loop 
      if(loop) c++;
      else c=-1;
  } // end card loop
//JDS write to lookup table
}

