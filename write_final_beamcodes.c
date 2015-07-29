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

#define NEW_PMAT 1 
#define write_to_matrix 0 
#define read_lookup_table 0 
#define write_lookup_table 0
#define CARDS 20 
#define PHASECODES 8192 
#define BEAMCODES 8192 
#define MAX_FREQS 1201 
#define MAX_ANGLES 32 
#define MAX_FSTEPS 64 
#define F_OFFSET 64 
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
  int read_table=read_lookup_table,write_table=write_lookup_table,read_matrix=0,write_matrix=write_to_matrix;
  char *caldir=NULL;
  double highest_time0_value=-1E13,lowest_pwr_mag=-1E13,middle=-1;
  double *pwr_mag[MAX_FREQS];
  double *step_pwr_mag[MAX_FSTEPS];
  double *step_timedelay[MAX_FSTEPS];
  int32_t *step_attencodes[MAX_FSTEPS];
  int32_t *step_phasecodes[MAX_FSTEPS];
  double freqs[MAX_FREQS];
  double angles[MAX_ANGLES];
  double angles_requested_delay[MAX_ANGLES];
  double angles_needed_delay[MAX_ANGLES];
  int32_t lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  //int32_t *ave_phasecodes=NULL, *max_attencodes=NULL;
  //double *max_pwr_mag=NULL;
  int32_t *final_beamcodes[CARDS], *final_attencodes[CARDS];
  double *final_angles[CARDS];
  double *final_freqs[CARDS];
  int32_t a=0,f=0,i=0,card=0,c=0,b=0,rval=0,count=0,attempt=0,index=0; 
  int32_t num_freqs,max_angles,num_angles,num_beamcodes,num_fsteps,fstep,foffset,num_cards;
  double fextent=0.0,f0=0.0,fm=0.0,df=0.0,angle,freq,frequency_lo[MAX_FSTEPS],frequency_hi[MAX_FSTEPS];
  int32_t requested_phasecode=0, requested_attencode=0,beamcode=0;
  int32_t radar,loop=0,read=0;
  uint32_t portA0,portB0,portC0,cntrl0 ;
  uint32_t portA1,portB1,portC1,cntrl1 ;
  int32_t		 temp, pci_handle, j,  IRQ  ;
  unsigned char	 *BASE0, *BASE1;
  uint32_t	 mmap_io_ptr,IOBASE, CLOCK_RES;
  float		 time;
#ifdef __QNX__
	struct		 _clockperiod new, old;
	struct		 timespec start_p, stop_p, start, stop, nsleep;
#endif
  radar=0;
  while ((opt = getopt(argc, argv, "n:c:lrwRWh")) != -1) {
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
      case 'r':
        read_table=1;
        break;
      case 'w':
        write_table=1;
        break;
      case 'R':
        read_matrix=1;
        break;
      case 'W':
        write_matrix=1;
        break;
      case 'h':
      default: /* '?' */
        fprintf(stderr, "Usage: %s [-lrwRW] [-c] first_card [-n] radar number \n\t l: loop\n\t r: Read existing loopup table\n\t w: Write lookup table\n", argv[0]); 
        fprintf(stderr, "\t R: Read Phasing Matrix Memory\n\t W: Write Phasing Matrix Memory\n"); 
        exit(EXIT_FAILURE);
    }
  }
  if(write_matrix || read_matrix) {
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
  for(f=0;f<MAX_FSTEPS;f++) {
    step_attencodes[f]=NULL;
    step_phasecodes[f]=NULL;
    step_pwr_mag[f]=NULL;
  } 
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
    final_beamcodes[c]=calloc(BEAMCODES,sizeof(int32_t));
    final_attencodes[c]=calloc(BEAMCODES,sizeof(int32_t));
    final_angles[c]=calloc(BEAMCODES,sizeof(double));
    final_freqs[c]=calloc(BEAMCODES,sizeof(double));
    for (b=0;b<BEAMCODES;b++) {
      final_beamcodes[c][b]=8191;
      final_attencodes[c][b]=63;
      final_angles[c][b]=-1000;
      final_freqs[c][b]=-1000;
    }
  }
  fflush(stdout);
  fflush(stdin);

  if(read_table) {
      sprintf(filename,"%s/beamcode_lookup_table_%s.dat",dir,radar_name,c);
      beamtablefile=fopen(filename,"r+");
      printf("%p %s\n",beamtablefile,filename);
      if(beamtablefile!=NULL) {
        printf("Reading from saved beamcode lookup table\n"); 
        fread(&num_freqs,sizeof(int32_t),1,beamtablefile);
        fread(&num_angles,sizeof(int32_t),1,beamtablefile);
        fread(&max_angles,sizeof(int32_t),1,beamtablefile);
        fread(&num_beamcodes,sizeof(int32_t),1,beamtablefile);
        fread(&num_fsteps,sizeof(int32_t),1,beamtablefile);
        fread(&df,sizeof(double),1,beamtablefile);
        fread(&foffset,sizeof(int32_t),1,beamtablefile);
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
          rval=fread(final_beamcodes[c],sizeof(int32_t),num_beamcodes,beamtablefile);
          rval=fread(final_attencodes[c],sizeof(int32_t),num_beamcodes,beamtablefile);
          rval=fread(final_freqs[c],sizeof(int32_t),num_beamcodes,beamtablefile);
          rval=fread(final_angles[c],sizeof(int32_t),num_beamcodes,beamtablefile);
          for (b=0;b<BEAMCODES;b++) {
           if (final_beamcodes[c][b] >= 0 ) {
              count++; 
            }
          }  
          printf("Lookup Card: %d Count: %d\n",c,count);
        }
        fclose(beamtablefile);
        beamtablefile=NULL;
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
      sprintf(filename,"%s%s_%s_%02d%s",dir,beamfile_prefix,radar_name,c,file_ext);
      printf("Card: %d File: %s\n",c,filename);
      beamcodefile=fopen(filename,"r");
      if(beamcodefile!=NULL) {
        fread(&highest_time0_value,sizeof(double),1,beamcodefile);
        fread(&lowest_pwr_mag,sizeof(double),1,beamcodefile);
        fread(&temp,sizeof(int32_t),1,beamcodefile);
        if(temp!=num_freqs && read ) fprintf(stderr,"num_freq mismatch! %d %d\n",temp,num_freqs); 
        num_freqs=temp;
        printf("Num freqs: %d\n",num_freqs);
        fread(freqs,sizeof(double),num_freqs,beamcodefile);
        fm=(double)freqs[num_freqs-1];
        f0=(double)freqs[0];
        fextent=fm-f0;
        foffset=F_OFFSET; 

        fread(&temp,sizeof(int32_t),1,beamcodefile);
        if(temp!=num_angles && read) fprintf(stderr,"num_angles mismatch! %d %d\n",temp,num_angles); 
        num_angles=temp;
        printf("Num angles: %d\nAngles: ",num_angles);
        fread(&middle,sizeof(double),1,beamcodefile);
        fread(angles,sizeof(double),num_angles,beamcodefile);
        fread(angles_requested_delay,sizeof(double),num_angles,beamcodefile);
        fread(angles_needed_delay,sizeof(double),num_angles,beamcodefile);
        for(a=0;a<num_angles;a++) printf("%4.1lf ",angles[a]);
        printf("\n");
        fread(&temp,sizeof(int32_t),1,beamcodefile);
        if(temp!=num_fsteps && read) fprintf(stderr,"num_fsteps mismatch! %d %d\n",temp,num_fsteps); 
        num_fsteps=temp;

        df=(fm-f0)/(double)num_fsteps;

        printf("Num fsteps: %d\n",num_fsteps);
        printf("freq 0: %g df: %lf\n",freqs[0],df);
        printf("freq max: %g df: %lf\n",freqs[num_freqs-1],df);
        printf("freq code offset: %d\n",foffset);

        for(f=0;f<=num_fsteps;f++) {
          fread(&index,sizeof(int32_t),1,beamcodefile);
          fread(&frequency_lo[f],sizeof(double),1,beamcodefile);
          fread(&frequency_hi[f],sizeof(double),1,beamcodefile);

          if(step_pwr_mag[f]!=NULL) free(step_pwr_mag[f]);
          step_pwr_mag[f]=calloc(num_angles,sizeof(double));
          if(step_timedelay[f]!=NULL) free(step_timedelay[f]);
          step_timedelay[f]=calloc(num_angles,sizeof(double));
          if(step_attencodes[f]!=NULL) free(step_attencodes[f]);
          step_attencodes[f]=calloc(num_angles,sizeof(int32_t));
          if(step_phasecodes[f]!=NULL) free(step_phasecodes[f]);
          step_phasecodes[f]=calloc(num_angles,sizeof(int32_t));
          rval=fread(step_pwr_mag[f],sizeof(double),num_angles,beamcodefile);
          if( rval!=num_angles) fprintf(stderr,"Error reading step_pwr_mag :: %d %d\n",rval,num_angles);
          rval=fread(step_timedelay[f],sizeof(double),num_angles,beamcodefile);
          if( rval!=num_angles) fprintf(stderr,"Error reading step_pwr_mag :: %d %d\n",rval,num_angles);
          rval=fread(step_attencodes[f],sizeof(int32_t),num_angles,beamcodefile);
          if( rval!=num_angles) fprintf(stderr,"Error reading step_attencodes :: %d %d\n",rval,num_angles);
          rval=fread(step_phasecodes[f],sizeof(int32_t),num_angles,beamcodefile);
          if( rval!=num_angles) fprintf(stderr,"Error reading step_phasecodes :: %d %d\n",rval,num_angles);
          printf("  Step  ::  Bmnum  ::  Ang   ::  Phasecode :: Attencode :: Pwr_mag\n");
          for(a=0;a<num_angles;a++) printf(" %6d :: %8d :: %6.1lf :: %8d :: %8d :: %6.2lf\n",f,a,angles[a],step_phasecodes[f][a],step_attencodes[f][a],step_pwr_mag[f][a]);
          printf("\n"); 
        }
        fclose(beamcodefile); //beamcode file
        read=1;
        printf("num_angles: %d \n", num_angles);
        for (b=0;b<BEAMCODES;b++) {
          final_beamcodes[c][b]=8191;
          final_attencodes[c][b]=63;
          angle=-1000;
          freq=-1000;
          final_freqs[c][b]=freq;
          final_angles[c][b]=angle;
        }
       /* Lets put some diagnostic codes in the upper memory */
        final_beamcodes[c][8191]=8191;
        final_attencodes[c][8191]=0;
        final_beamcodes[c][8190]=0;
        final_attencodes[c][8190]=0;
        final_beamcodes[c][8189]=8191;
        final_attencodes[c][8189]=63;
        final_beamcodes[c][8188]=0;
        final_attencodes[c][8188]=63;
    
        for(a=0;a<num_angles;a++) {
          printf("\n--------------------\n");
          printf("      Freq MHz      ::   Bmnum  ::  Ang   :: Beamcode ::   Pcode  ::   Td    ::  Acode  :: Pwr_mag\n");
          if(a<MAX_ANGLES) {
/*
            requested_phasecode=ave_phasecodes[a];
            requested_attencode=max_attencodes[a];
            angle=angles[a];
            freq=0;
            b=a;
            final_beamcodes[c][b]=requested_phasecode;
            final_attencodes[c][b]=requested_attencode;
            final_freqs[c][b]=freq;
            final_angles[c][b]=angle;
            printf("%8.0lf :: %8d :: %6.1lf :: %8d :: %8d :: %8d :: %6.2lf\n",freq,a,angles[a],b,final_beamcodes[c][b],final_attencodes[c][b],max_pwr_mag[a]);
*/
            for(f=0;f<=num_fsteps;f++) {
              if(f==0) {
                b=a;
                freq=0;
              } else {
                b=(f-1)*MAX_ANGLES+a+foffset;
                freq=(df*(f-1))+freqs[0]+df/2.0;
              } 
              if(b<BEAMCODES) {
                requested_phasecode=step_phasecodes[f][a];
                requested_attencode=step_attencodes[f][a];
                angle=angles[a];
                final_beamcodes[c][b]=requested_phasecode;
                final_attencodes[c][b]=requested_attencode;
                final_freqs[c][b]=freq;
                final_angles[c][b]=angle;
                printf("%8.4f - %8.4f :: %8d :: %6.1lf :: %8d :: %8d ::  %6.2lf :: %7d :: %6.2lf\n",
                frequency_lo[f]/1E6,frequency_hi[f]/1E6,a,angles[a],b,final_beamcodes[c][b],step_timedelay[f][a],final_attencodes[c][b],step_pwr_mag[f][a]);
              } else {
                printf("beamcode out of bounds: f: %d a: %d b: %d\n",f,a,b);
              }
            }
          } else {
                printf("angle out of bounds: a: %d\n",a);
          } 
          //printf("-------------------------------\n"); 
        }  
      } else {
        printf("Bad beamcode file\n");
      }
      if(loop) c++;
      else c=-1;
  } // end of card loop
  c=card;
  while((c<CARDS) && (c >=0)) {
      if (write_matrix) 
        fprintf(stdout,"Writing Beamcodes to Card: %d\n",c);
      if (read_matrix) 
        fprintf(stdout,"Reading Beamcodes to Card: %d\n",c);
      if (write_matrix||read_matrix) {
        for (b=0;b<BEAMCODES;b++) {
         printf("B: %d PhaseCode: %d AttenCode: %d\n",b,final_beamcodes[c][b],final_attencodes[c][b]); 
           if (final_beamcodes[c][b] >= 0 ) {
              if(NEW_PMAT) {
                if (write_matrix) 
                  temp=write_data_new(IOBASE,c,b,final_beamcodes[c][b],radar,0);
                for(attempt=0;attempt<10;attempt++) {
                  temp=verify_data_new(IOBASE,c,b,final_beamcodes[c][b],radar,0);
                  if(temp<0) {
                    printf("Verify Phase Error: attempt: %d\n",attempt); 
                    usleep(10000);
                  } else {
                    break;
                  }
                } 
                if(temp<0) {
                  printf("EXIT on repeat Verify Errors\n");
                  exit(temp); 
                }
                if (write_matrix) 
                  temp=write_attenuators(IOBASE,c,b,final_attencodes[c][b],radar); //JDS need to set attenuators here
                for(attempt=0;attempt<10;attempt++) {
                  temp=verify_attenuators(IOBASE,c,b,final_attencodes[c][b],radar);
                  if(temp<0) {
                    printf("Verify Atten Error: attempt: %d\n",attempt); 
                    usleep(10000);
                  } else {
                    break;
                  }
                } 
                if(temp<0) {
                  printf("EXIT on repeat Verify Errors\n");
                  exit(temp); 
                }

              } else {
                if (write_matrix) 
                  temp=write_data_old(IOBASE,c,b,final_beamcodes[c][b],radar,0);
              }
           } else {
             //printf("final_beamcode error %d\n",b);
              if (write_matrix) {
                if(NEW_PMAT) {
                  temp=write_data_new(IOBASE,c,b,0,radar,0);
                  for(attempt=0;attempt<10;attempt++) {
                    temp=verify_data_new(IOBASE,c,b,0,radar,0);
                    if(temp<0) {
                   //   printf("Verify Error: attempt: %d\n",attempt); 
                    usleep(10000);
                    } else {
                      break;
                    }
                  } 
                  if(temp<0) {
                    printf("EXIT on repeat Verify Errors\n");
                    exit(temp); 
                  }
                  temp=write_attenuators(IOBASE,c,b,63,radar); //JDS need to set attenuators here
                  for(attempt=0;attempt<10;attempt++) {
                    temp=verify_attenuators(IOBASE,c,b,63,radar);
                    if(temp<0) {
                 //   printf("Verify Error: attempt: %d\n",attempt); 
                      usleep(10000);
                    } else {
                      break;
                    }
                  } 
                  if(temp<0) {
                    printf("EXIT on repeat Verify Errors\n");
                    exit(temp); 
                  }

                } else {
                  temp=write_data_old(IOBASE,c,b,0,radar,0);
                }
              }
           }
        } //end beamcode loop 
      } //end write loop 

      if (write_matrix) {
        printf("Verifying Beamcodes to Card: %d\n",c);
        for (b=0;b<BEAMCODES;b++) {
           if (final_beamcodes[c][b] >= 0 ) {
             if(NEW_PMAT) {
                beam_code(IOBASE,b,radar);
                for(attempt=0;attempt<10;attempt++) {
                  temp=verify_data_new(IOBASE,c,b,final_beamcodes[c][b],radar,0);
                  if(temp<0) {
                  //  printf("Verify Error: attempt: %d\n",attempt); 
                    usleep(10000);
                  } else {
                    break;
                  }
                } 
                if(temp<0) {
                  printf("EXIT on repeat Verify Errors\n");
                  exit(temp); 
                }
                for(attempt=0;attempt<10;attempt++) {
                  temp=verify_attenuators(IOBASE,c,b,final_attencodes[c][b],radar);
                  if(temp<0) {
                  //  printf("Verify Error: attempt: %d\n",attempt); 
                    usleep(10000);
                  } else {
                    break;
                  }
                } 
                if(temp<0) {
                  printf("EXIT on repeat Verify Attenuator Errors\n");
                  exit(temp); 
                }

             } else {
               for(attempt=0;attempt<10;attempt++) {
                 temp=verify_data_old(IOBASE,c,b,final_beamcodes[c][b],radar,0);
                 if(temp<0) {
                 //  printf("Verify Error: attempt: %d\n",attempt);
                   delay(10);
                 } else {
                   break;
                 }
               }
               if(temp<0) {
                  printf("EXIT on repeat Verify Errors\n");
                  exit(temp);
               }
             }
          }
        } //end beamcode loop 
      } //end write if
      if(loop) c++;
      else c=-1;
  } // end card loop
//JDS write to lookup table
    if(write_table) {
      printf("Writing Beam lookup table\n");
      sprintf(filename,"%s/beamcode_lookup_table_%s.dat",dir,radar_name,c);
      beamtablefile=fopen(filename,"w+");
      printf("%p %s\n",beamtablefile,filename);
      if(beamtablefile!=NULL) {
        temp=num_freqs;
        fwrite(&temp,sizeof(int32_t),1,beamtablefile);
        temp=num_angles;
        fwrite(&temp,sizeof(int32_t),1,beamtablefile);
        temp=MAX_ANGLES;
        fwrite(&temp,sizeof(int32_t),1,beamtablefile);
        temp=BEAMCODES;
        fwrite(&temp,sizeof(int32_t),1,beamtablefile);
        temp=MAX_FSTEPS;
        fwrite(&temp,sizeof(int32_t),1,beamtablefile);
        fstep=0.;
        fwrite(&fstep,sizeof(double),1,beamtablefile);
        fwrite(&foffset,sizeof(int32_t),1,beamtablefile);
        fwrite(&f0,sizeof(double),1,beamtablefile);
        fwrite(&fm,sizeof(double),1,beamtablefile);
        temp=CARDS;
        fwrite(&temp,sizeof(int32_t),1,beamtablefile);
        fwrite(freqs,sizeof(double),num_freqs,beamtablefile);
        fwrite(angles,sizeof(double),num_angles,beamtablefile);
        for (c=0;c<CARDS;c++) {
          rval=fwrite(final_beamcodes[c],sizeof(int32_t),BEAMCODES,beamtablefile);
          rval=fwrite(final_attencodes[c],sizeof(int32_t),BEAMCODES,beamtablefile);
          rval=fwrite(final_freqs[c],sizeof(int32_t),BEAMCODES,beamtablefile);
          rval=fwrite(final_angles[c],sizeof(int32_t),BEAMCODES,beamtablefile);
        }
        fclose(beamtablefile);
      } else {
        fprintf(stderr,"Error writing beam lookup table file\n");
      }
   }
}

