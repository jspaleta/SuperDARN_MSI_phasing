#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#define MAX_CARDS 20 
#define MAX_FREQS 201 
//#define MAX_FREQS 1201 
#define MAX_PHASES 8192
int32_t verbose=0;
FILE *timedelayfile=NULL;
FILE *summaryfile=NULL;
struct timeval t0,t1,t2,t3;
unsigned long elapsed;



int32_t main()
{
  double *freq,*pwr_mag[MAX_FREQS],*ave_timedelay=NULL,*timedelay[MAX_FREQS];    
//  int32_t best_phasecode[MAX_FREQS][MAX_ANGLES];
  int32_t missing_card[MAX_CARDS],bad_card[MAX_CARDS];
  int32_t b,c,i,ii,p,count;
  int32_t summary_freqs,summary_phases;
  int32_t num_freqs,num_phasecodes,num_cards,num_angles;
  char filename[120];
  int32_t highest_time0_card; // card with highest time0 delay
  double highest_time0_value; // highest time0 delay in ns
  int32_t lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  double lowest_pwr_mag=1E10; // freq,card,phasecode
  double time_needed,angle,difference;
  double ave_delay0,stdev_delay0;
  char *caldir=NULL;
  char radar_name[80]="";
  char dirstub[160]="";

  caldir=getenv("MSI_CALDIR");
  if (caldir==NULL) {
    caldir=strdup("/data/calibrations/");
  }
  fprintf(stdout,"CALDIR: %s\n",caldir);
  printf("\n\nEnter Radar Name: ");
  fflush(stdin);
  fflush(stdout);
  scanf("%s", &radar_name);
  fflush(stdout);
  fflush(stdin);
  printf("Radar: <%s>\n",radar_name);
  fflush(stdout);
  sprintf(dirstub,"/%s/%s/",caldir,radar_name);

  if (verbose>1) printf("Nulling arrays\n");
  freq=NULL;
  ave_timedelay=NULL;
  num_freqs=0;
  highest_time0_value=-1000;
  highest_time0_card=-1;
  for(i=0;i<MAX_FREQS;i++) {
        timedelay[i]=NULL;
        pwr_mag[i]=NULL;
  }

  for(c=0;c<MAX_CARDS;c++) {
    bad_card[c]=0;
    missing_card[c]=1;
    sprintf(filename,"%s/timedelay_cal_%s_%02d.dat",dirstub,radar_name,c);
    timedelayfile=fopen(filename,"r");
    printf("%p %s\n",timedelayfile,filename); 
    if (timedelayfile!=NULL) {
      missing_card[c]=0;
      fread(&num_phasecodes,sizeof(int32_t),1,timedelayfile);
      if (verbose>1) printf("PhaseCodes: %d\n",num_phasecodes);
      if (num_phasecodes != MAX_PHASES) fprintf(stderr,"FILE: %s  ERROR:  Wrong number of phases  %d %d\n",filename,num_phasecodes,MAX_PHASES);
      fread(&num_cards,sizeof(int32_t),1,timedelayfile);
      if (verbose>1) printf("Cards: %d\n",num_cards);
      if (num_cards != MAX_CARDS) fprintf(stderr,"FILE: %s  ERROR:  Wrong number of cards  %d %d\n",filename,num_cards,MAX_CARDS);
      fread(&num_freqs,sizeof(int32_t),1,timedelayfile);
      if (verbose>1) printf("Freqs: %d\n",num_freqs);
      if (num_freqs != MAX_FREQS) fprintf(stderr,"FILE: %s  ERROR:  Wrong number of freqs  %d %d\n",filename,num_freqs,MAX_FREQS);
      if (num_freqs>MAX_FREQS) {
        fprintf(stderr,"Too many stored frequencies...up the MAX_FREQS define!\n");
        exit(0);
      }
      fread(&ave_delay0,sizeof(double),1,timedelayfile);
      fread(&stdev_delay0,sizeof(double),1,timedelayfile);

      if (verbose>1) printf("Allocating arrays\n");
      if (ave_timedelay!=NULL) free(ave_timedelay);
      ave_timedelay=calloc(num_phasecodes,sizeof(double));
      for(i=0;i<num_freqs;i++) {
        if (freq!=NULL) free(freq);
        freq=calloc(num_freqs,sizeof(double));
        if (timedelay[i]!=NULL) free(timedelay[i]);
        timedelay[i]=calloc(num_phasecodes,sizeof(double));
        if (pwr_mag[i]!=NULL) free(pwr_mag[i]);
        pwr_mag[i]=calloc(num_phasecodes,sizeof(double));
      }


      if (verbose>1) printf("Reading frequency array\n");

      count=fread(freq,sizeof(double),num_freqs,timedelayfile);
      if (verbose>1) printf("%d %d\n",num_freqs,count);
      count=fread(ave_timedelay,sizeof(double),num_phasecodes,timedelayfile);
      if (verbose>1) printf("Ave_timedelays %d\n",count);
      count=1;
      if (verbose>1) printf("Reading in data\n");
      while(count>0) {
          count=fread(&ii,sizeof(int32_t),1,timedelayfile);
          if (count==0) {
            break;
          }
          count=fread(timedelay[ii],sizeof(double),num_phasecodes,timedelayfile);
//          printf("Freq index: %d Phase Count: %d\n",ii,count);
          count=fread(pwr_mag[ii],sizeof(double),num_phasecodes,timedelayfile);
//          printf("Freq index: %d Pwr-mag Count: %d\n",ii,count);
          if (count==0) {
            break;
          }
      }
      if (count==0) {
        if (feof(timedelayfile)) if (verbose>1) printf("End of File!\n");
      }
      fclose(timedelayfile);
      printf("Card: %d ave_T0: %lf %lf\n",c,ave_timedelay[0],ave_delay0);
      if (ave_timedelay[0]>highest_time0_value) {
            highest_time0_card=c;
            highest_time0_value=ave_timedelay[0];
      }        
      for(i=0;i<num_freqs;i++) {
        for (b=0;b<num_phasecodes;b++) {
          if (pwr_mag[i][b]< -20.0) bad_card[c]=1;
          if (pwr_mag[i][b]<lowest_pwr_mag) {
            lowest_pwr_mag_index[0]=i;
            lowest_pwr_mag_index[1]=c;
            lowest_pwr_mag_index[2]=b;
            lowest_pwr_mag=pwr_mag[i][b];
          }        
        }
      }


    }  //timedelay file if
  } //card loop
//Summary stats
  printf("::: Summary Stats for All Cards :::\n");
  if (highest_time0_card >=0) {
        printf("Highest 0-Time Delay: %8.3lf card: %d\n",highest_time0_value,highest_time0_card);
  }
  printf("Lowest Mag: %lf ",lowest_pwr_mag);
  printf(" Index:: freq: %d card: %d phasecode: %d\n",
     lowest_pwr_mag_index[0],lowest_pwr_mag_index[1],lowest_pwr_mag_index[2]);
  printf("\n-------\n");
  for (c=0;c<MAX_CARDS;c++) {
    if (bad_card[c]) printf("Card %d has low pwr\n",c);
    if (missing_card[c]) printf("Card %d information unprocessed\n",c);
  }
  sprintf(filename,"%s/timedelay_summary_%s.dat",dirstub,radar_name);
  summaryfile=fopen(filename,"w+");
  if (verbose > 1 ) printf("Creating %p %s\n",summaryfile,filename); 
  if (summaryfile!=NULL) {
    summary_freqs=MAX_FREQS;
    fwrite(&summary_freqs,sizeof(int32_t),1,summaryfile);
    fwrite(&highest_time0_card,sizeof(int32_t),1,summaryfile);
    fwrite(&highest_time0_value,sizeof(double),1,summaryfile);
    fwrite(lowest_pwr_mag_index,sizeof(int32_t),3,summaryfile);
    fwrite(&lowest_pwr_mag,sizeof(double),1,summaryfile);
    fclose(summaryfile);
  }


} //main
