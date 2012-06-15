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
#define MAX_FREQS 2000
#define MAX_ANGLES 100

int32_t sock=-1;
int32_t verbose=2;
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

int32_t main(int32_t argc, char **argv)
{
  double *pwr_mag[MAX_FREQS];
  double freqs[MAX_FREQS];
  double angles[MAX_ANGLES],std_angles[MAX_ANGLES];
  int32_t lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  int32_t *best_phasecode[MAX_FREQS], *best_std_phasecode[MAX_FREQS];
  int32_t *final_beamcodes[CARDS];
  int32_t *final_attencodes[CARDS];
  double *final_angles[CARDS];
  double *final_freqs[CARDS];
  int32_t a=0,i=0,c=0,b=0,rval=0,count=0; 
  int32_t num_freqs,num_angles,num_std_angles,num_beamcodes,num_cards;
  int32_t std_angle_index_offset=0,angle_index_offset=192;
  double df=0.0,angle,freq;
  int32_t requested_phasecode=0, beamcode=0;
  char radar_name[80];
  int32_t		 temp, j;
  float		 time;

  for (c=0;c<CARDS;c++) {
    final_beamcodes[c]=calloc(BEAMCODES,sizeof(int32_t));
    final_attencodes[c]=calloc(BEAMCODES,sizeof(int32_t));
    final_angles[c]=calloc(BEAMCODES,sizeof(double));
    final_freqs[c]=calloc(BEAMCODES,sizeof(double));
    for (b=0;b<BEAMCODES;b++) {
      final_beamcodes[c][b]=-1;
      final_attencodes[c][b]=-1;
      final_angles[c][b]=-1;
      final_freqs[c][b]=-1;
    }
  }
  printf("\n\nEnter Radar Name: ");
  fflush(stdin);
  scanf("%s", &radar_name);
  fflush(stdout);
  fflush(stdin);

  sprintf(dir,"/home/jspaleta/data/calibrations/%s/",radar_name);
  if(read_lookup_table) {
      sprintf(filename,"%s/beamcode_lookup_table_%s.dat",dir,radar_name);
      beamtablefile=fopen(filename,"r+");
      printf("%p %s\n",beamtablefile,filename);
      if(beamtablefile!=NULL) {
        printf("Reading from saved beamcode lookup table\n"); 
        fread(&num_freqs,sizeof(int32_t),1,beamtablefile);
        fread(&num_std_angles,sizeof(int32_t),1,beamtablefile);
        fread(&num_angles,sizeof(int32_t),1,beamtablefile);
        fread(&num_beamcodes,sizeof(int32_t),1,beamtablefile);
        fread(&num_cards,sizeof(int32_t),1,beamtablefile);
        fread(&std_angle_index_offset,sizeof(int32_t),1,beamtablefile);
        fread(&angle_index_offset,sizeof(int32_t),1,beamtablefile);
        fread(freqs,sizeof(double),num_freqs,beamtablefile);
        fread(std_angles,sizeof(double),num_std_angles,beamtablefile);
        fread(angles,sizeof(double),num_angles,beamtablefile);

        printf("Counting saved codes in lookup table\n");
        for (c=0;c<CARDS;c++) {
          count=0;
          rval=fread(final_beamcodes[c],sizeof(int32_t),num_beamcodes,beamtablefile);
          rval=fread(final_attencodes[c],sizeof(int32_t),num_beamcodes,beamtablefile);
          for (b=0;b<BEAMCODES;b++) {
           if (final_beamcodes[c][b] >= 0 ) {
              count++;
            }
          }
          printf("Lookup Card: %d Count: %d\n",c,count);
        }
        fclose(beamtablefile);
        beamtablefile=NULL;
      } else {
        fprintf(stderr,"Error writing beam lookup table file\n");
      }
      printf("Counting saved codes in lookup table\n");
      for (c=0;c<CARDS;c++) {
        count=0;
        for (b=0;b<BEAMCODES;b++) {
          if (final_beamcodes[c][b] >= 0 ) {
            count++; 
          }
        }
        printf("Lookup Card: %d Count: %d\n",c,count);
     }
    
  }
/*
  printf("\n\nEnter Beamcode: ");
  fflush(stdin);
  fflush(stdout);
  scanf("%d", &b);
  printf("Radar: <%s>  Beamcode: %d \n",radar_name,b);
  for(c=0;c<20;c++) {
    if ( b < angle_index_offset )
      printf("  Card: %d Angle: %lf  PhaseCode: %d AttenCode: %d\n",c,std_angles[b],final_beamcodes[c][b],final_attencodes[c][b]);
  }
  fflush(stdout);
*/
}

