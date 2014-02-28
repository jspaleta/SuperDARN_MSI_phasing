#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define EXIT_ON_ATTEN_CHECK 1 
#define MIN_CARD 0 
#define MAX_CARD 19 
#define MAX_FSTEPS 36 
#define MAX_FREQS 1501 
//#define MAX_FREQS 1201 
#define MAX_PHASES 8192
#define MAX_ANGLES 32 
#define NUM_ANGLES 24 
#define USE_MEASURED_ATTENS 1 
#define MIN_ATTEN_FREQ_HZ 10E6
#define MAX_ATTEN_FREQ_HZ 16E6
#define HAS_TDELAY 0 
int32_t verbose=1;
FILE *timedelayfile=NULL;
FILE *attenfile=NULL;
FILE *summaryfile=NULL;
FILE *beamcodefile=NULL;
struct timeval t0,t1,t2,t3;
unsigned long elapsed;
double angles[MAX_ANGLES];
int32_t attenfile_exists=0;

double spacing=12.8016; //meters : MSI 42 feet == 12.8016 meters 

/*
double spacing=15.24; //meters : SPS 50 feet == 15.24 meters 
*/
double bm_sep=3.24;
//double middle=11.5; //(NUM_ANGLES-1)/2

double timedelay_needed(double angle,double spacing,int32_t card) {
/*
*  angle from broadside (degrees)  spacing in meters
*/
  double deltat=0;
  double needed=0;
  double c=0.299792458; // meters per nanosecond
  int32_t antenna=-1;
  double radians=0.0;
  if (card > 15) antenna=card-10;
  else antenna=card;
  deltat=(spacing/c)*sin((fabs(angle)*3.14159)/180.0); //nanoseconds
  if (angle > 0) needed=antenna*deltat;
  if (angle < 0) needed=(15-antenna)*deltat;
  if (needed < 0) {
    printf("Error in Time Needed Calc: %lf %lf\n",needed,deltat);
  }
//  fprintf(stdout,"Deltat: %lf Time delay needed: %lf Card: %d Antenna: %d Angle: %lf\n", deltat,needed,card,antenna,angle);
  return needed; //nanoseconds
}

int32_t main()
{
  double *freq,*pwr_mag[MAX_FREQS],*timedelay[MAX_FREQS];    
  double *atten_freq,*atten_pwr_mag[MAX_FREQS],*atten_phase[MAX_FREQS],*atten_tdelay[MAX_FREQS];    
  double min_atten_tdelay,max_atten_tdelay,diff_atten_tdelay;
  double min_atten_freq,max_atten_freq;
  double ave_delay0,stdev_delay0;
  int32_t best_phasecode,fallback_phasecode;
  int32_t best_hi_attencode;
  double best_hi_atten_value;
  int32_t best_lo_attencode;
  double best_lo_atten_value;
  int32_t best_attencode;
  double best_atten_value;
  double best_pwr_mag;
  double *ave_timedelay;
  double *beam_pwr_mag;
  int32_t *beam_attencode;
  int32_t *beam_phasecode;
  double ave_phasecode_td,best_phasecode_td,worst_phasecode_td,fallback_phasecode_td;
  double allowed_td_diff,allowed_td_span,phasecode_td_span,worst_phasecode_td_diff,fallback_td_span;
  int    within_allowed_td,within_allowed_span;
  double *beam_atten_value,av;
  double frequency_lo,frequency_hi,frequency,df;
  int32_t b,c,i,ii,j,p,f,ang,a,count,data_count;
  int32_t summary_freqs,summary_phases;
  int32_t ave_num_freqs,num_freqs,num_phasecodes,num_cards,num_steps,num_angles;
  int32_t num_atten_freqs,num_attencodes,num_atten_cards;
  char filename[120];
  int32_t highest_time0_card; // card with highest time0 delay
  double highest_time0_value; // highest time0 delay in ns
  int32_t lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  double lowest_pwr_mag=1E10;
  double best_needed_atten=0.0,ave_needed_atten=0.0,ave_pwr=0.0,pwr0=0.0,fallback_needed_atten=0.0,needed_atten=0.0; 
  double time_needed,angle;
  double min_ave_diff,min_diff,max_diff,min_td,max_td,min_span,max_span,untuned_span;
  double expected_atten,atten,atten_steps[6];
  int bad_atten_pwr,bad_atten_steps[6];

  int num_ave_freqs;
  char *caldir=NULL;
  char radar_name[80]="";
  char dirstub[160]="";
  char poop;
  int ick;
  double middle=(float)(NUM_ANGLES-1)/2.0;
  printf("Middle: %lf\n",middle);
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

//  printf("Nulling arrays\n");
  freq=NULL;
  atten_freq=NULL;
  num_freqs=0;
  ave_timedelay=NULL;
  beam_pwr_mag=NULL;
  beam_attencode=NULL;
  beam_atten_value=NULL;
  beam_phasecode=NULL;
  highest_time0_value=0;
  highest_time0_card=-1;
  for(i=0;i<MAX_FREQS;i++) {
        timedelay[i]=NULL;
        pwr_mag[i]=NULL;
        atten_pwr_mag[i]=NULL;
        atten_phase[i]=NULL;    
        atten_tdelay[i]=NULL;    
  }
  for(i=0;i<6;i++) {
    atten_steps[i]=0.5*pow(2,i);
    bad_atten_steps[i]=0;
  }

  for(i=0;i<NUM_ANGLES;i++) {
    angles[i]=(i-middle)*bm_sep;
  }

  sprintf(filename,"%s/timedelay_summary_%s.dat",dirstub,radar_name);
  summaryfile=fopen(filename,"r");
  printf("%p %s\n",summaryfile,filename); 
  if (summaryfile!=NULL) {
    fread(&summary_freqs,sizeof(int32_t),1,summaryfile);
    fread(&highest_time0_card,sizeof(int32_t),1,summaryfile);
    fread(&highest_time0_value,sizeof(double),1,summaryfile);
    fread(lowest_pwr_mag_index,sizeof(int32_t),3,summaryfile);
    fread(&lowest_pwr_mag,sizeof(double),1,summaryfile);
    printf("Summary Lowest Mag Index:: freq: %d card: %d phasecode: %d\n",
      lowest_pwr_mag_index[0],lowest_pwr_mag_index[1],lowest_pwr_mag_index[2]);
    printf("Summary: lowest_pwr_mag %lf\n",lowest_pwr_mag);
    fclose(summaryfile);
  }
  if(highest_time0_value > 10.0 ) {
    printf("Longest Time0 is longer than 10 nanoseconds\n!!!!!");
    exit(1);
  }
  else highest_time0_value=10.0;

  printf("Time0 delay to shoot for: %lf\n",highest_time0_value);
  if(lowest_pwr_mag > -2.0 ) {
    lowest_pwr_mag=-2.0;
  } else {
    printf("Lowest Power is less than -2 dB\n!!!!!");
    exit(1);
  }
  printf("Final: lowest_pwr_mag %lf\n",lowest_pwr_mag);
  for(c=MIN_CARD;c<=MAX_CARD;c++) {
    for(i=0;i<6;i++) {
      bad_atten_steps[i]=0;
    }
    ick=0;
    sprintf(filename,"%s/phasing_cal_%s_%02d.att",dirstub,radar_name,c);
    attenfile=fopen(filename,"r");
    printf("%p %s\n",attenfile,filename); 
    if (attenfile!=NULL) {
      attenfile_exists=1;
      fread(&num_attencodes,sizeof(int32_t),1,attenfile);
      fprintf(stdout,"Num attencodes: %d\n",num_attencodes);
      fread(&num_atten_cards,sizeof(int32_t),1,attenfile);
      fread(&num_atten_freqs,sizeof(int32_t),1,attenfile);
      if (num_atten_freqs>MAX_FREQS) {
        fprintf(stderr,"Too many stored frequencies...up the MAX_FREQS define!\n");
        exit(0);
      }

      if (verbose > 0 ) fprintf(stdout,"Allocating arrays\n");
      for(i=0;i<num_atten_freqs;i++) {
        if (atten_freq!=NULL) free(atten_freq);
        atten_freq=calloc(num_atten_freqs,sizeof(double));
        if (atten_phase[i]!=NULL) free(atten_phase[i]);
        atten_phase[i]=calloc(num_attencodes,sizeof(double));
        if (atten_pwr_mag[i]!=NULL) free(atten_pwr_mag[i]);
        atten_pwr_mag[i]=calloc(num_attencodes,sizeof(double));
        if (atten_tdelay[i]!=NULL) free(atten_tdelay[i]);
        atten_tdelay[i]=calloc(num_attencodes,sizeof(double));
      }
      count=fread(atten_freq,sizeof(double),num_atten_freqs,attenfile);
      count=1;
      data_count=0;
      while(count>0) {
          count=fread(&ii,sizeof(int32_t),1,attenfile);
          if (count==0) {
            break;
          }
          count=fread(atten_phase[ii],sizeof(double),num_attencodes,attenfile);
          count=fread(atten_pwr_mag[ii],sizeof(double),num_attencodes,attenfile);
          if (HAS_TDELAY) count=fread(atten_tdelay[ii],sizeof(double),num_attencodes,attenfile);
          if (count==0) {
            break;
          }
	  data_count++;
          if(data_count==num_atten_freqs) break;
      }
      if (count==0) {
        if (feof(attenfile)) if (verbose > 0 ) fprintf(stdout,"End of File!\n");
      }
      fclose(attenfile);
      frequency_lo= MIN_ATTEN_FREQ_HZ;
      frequency_hi= MAX_ATTEN_FREQ_HZ;
      min_atten_tdelay=1E13;
      diff_atten_tdelay=1E13;
      max_atten_tdelay=-1.;
      for(i=0;i<num_atten_freqs;i++) {
        for(p=0;p<num_attencodes;p++) {
          if(atten_freq[i]> frequency_lo && atten_freq[i] < frequency_hi) {   

            if(atten_tdelay[i][p]<min_atten_tdelay) {
              min_atten_tdelay=atten_tdelay[i][p]; 
              min_atten_freq=atten_freq[i]; 
            } 
            if(atten_tdelay[i][p]>max_atten_tdelay) {
              max_atten_tdelay=atten_tdelay[i][p]; 
              max_atten_freq=atten_freq[i]; 
            } 

          }

        } 
      } //freq loop
    } else {
      attenfile_exists=0;
    }
    diff_atten_tdelay=fabs(max_atten_tdelay-min_atten_tdelay);
    fprintf(stdout,"Atten tdelay:: Min: %8.3g (F: %8.3g) Max: %8.3g  (F: %8.3g) Diff: %8.3g\n",
            min_atten_tdelay,min_atten_freq,max_atten_tdelay,max_atten_freq,diff_atten_tdelay); 
    sprintf(filename,"%s/timedelay_cal_%s_%02d.dat",dirstub,radar_name,c);
    timedelayfile=fopen(filename,"r");
    printf("%p %s\n",timedelayfile,filename); 
    if (timedelayfile!=NULL) {
      fread(&num_phasecodes,sizeof(int32_t),1,timedelayfile);
      printf("PhaseCodes: %d\n",num_phasecodes);
      fread(&num_cards,sizeof(int32_t),1,timedelayfile);
      printf("Cards: %d\n",num_cards);
      fread(&num_freqs,sizeof(int32_t),1,timedelayfile);
      printf("Freqs: %d\n",num_freqs);
      fread(&ave_delay0,sizeof(double),1,timedelayfile);
      fread(&stdev_delay0,sizeof(double),1,timedelayfile);

      if (num_freqs>MAX_FREQS) {
        printf("Too many stored frequencies...up the MAX_FREQS define!\n");
        exit(0);
      }

      printf("Allocating arrays\n");
        if (freq!=NULL) free(freq);
        if (ave_timedelay!=NULL) free(ave_timedelay);
        ave_timedelay=calloc(num_phasecodes,sizeof(double));

        if (beam_pwr_mag!=NULL) free(beam_pwr_mag);
        beam_pwr_mag=calloc(MAX_ANGLES,sizeof(double));

        if (beam_attencode!=NULL) free(beam_attencode);
        beam_attencode=calloc(MAX_ANGLES,sizeof(int32_t));
        if (beam_atten_value!=NULL) free(beam_atten_value);
        beam_atten_value=calloc(MAX_ANGLES,sizeof(double));
        if (beam_phasecode!=NULL) free(beam_phasecode);
        beam_phasecode=calloc(MAX_ANGLES,sizeof(int32_t));

      for(i=0;i<num_freqs;i++) {
        freq=calloc(num_freqs,sizeof(double));
        if (timedelay[i]!=NULL) free(timedelay[i]);
        timedelay[i]=calloc(num_phasecodes,sizeof(double));
        if (pwr_mag[i]!=NULL) free(pwr_mag[i]);
        pwr_mag[i]=calloc(num_phasecodes,sizeof(double));
      }
      count=fread(freq,sizeof(double),num_freqs,timedelayfile);
      printf("%d %d\n",num_freqs,count);
      count=fread(ave_timedelay,sizeof(double),num_phasecodes,timedelayfile);
      printf("Ave tdelay: %d\n",count);
      count=1;
      printf("Reading in data\n");
      data_count=0;
      while(count>0) {
          count=fread(&ii,sizeof(int32_t),1,timedelayfile);
          if (count==0) {
            break;
          }
          //fprintf(stdout,"Freq: %d ",ii);
          count=fread(timedelay[ii],sizeof(double),num_phasecodes,timedelayfile);
          //fprintf(stdout,"Td_count: %d ",count);
          count=fread(pwr_mag[ii],sizeof(double),num_phasecodes,timedelayfile);
          //fprintf(stdout,"pwr_count: %d ",count);
          if (count==0) {
            break;
          }
          //fprintf(stdout,"\n");
          data_count++;
      }
      if (data_count!=MAX_FREQS) {
        if (feof(timedelayfile)) printf("End of File! Read in: %d Freq points\n",data_count);
      }
      fclose(timedelayfile);
      sprintf(filename,"%s/beamcodes_cal_%s_%02d.dat",dirstub,radar_name,c);
      beamcodefile=fopen(filename,"w+");
      printf("%p %s\n",beamcodefile,filename); 

      if(USE_MEASURED_ATTENS && attenfile_exists) printf("Using measured Attenuations\n"); 
      if (beamcodefile!=NULL) {
        fwrite(&num_freqs,sizeof(int32_t),1,beamcodefile);
        fwrite(freq,sizeof(double),num_freqs,beamcodefile);

        num_angles=NUM_ANGLES;
        fwrite(&num_angles,sizeof(int32_t),1,beamcodefile);
        fwrite(angles,sizeof(double),num_angles,beamcodefile);
        num_steps=MAX_FSTEPS;
        fwrite(&num_steps,sizeof(int32_t),1,beamcodefile);

//find best phasecode and attencode to match average card response over frequency range of interest 
        df=(freq[num_freqs-1]-freq[0])/(double)MAX_FSTEPS;
        printf("Freq Tuned Steps: %d Count: %8.3g MHz\n",MAX_FSTEPS,df*1E-6);
        max_span=-1;
        untuned_span=-1;
        for(f=0;f<=MAX_FSTEPS;f++) {
          if(f==0) {
            frequency_lo= MIN_ATTEN_FREQ_HZ;
            frequency_hi= MAX_ATTEN_FREQ_HZ;
          } else {
            frequency_lo=df*(f-1)+freq[0];
            frequency_hi=df*(f)+freq[0];
          }
          printf("Ave over frequency span: %lf %lf\n",frequency_lo,frequency_hi); 

          allowed_td_diff=3.0;
          for(b=0;b<num_angles;b++) {
            angle=angles[b];
            beam_pwr_mag[b]=-1E13;
            beam_attencode[b]=-1;
            beam_phasecode[b]=-1;
            best_phasecode=-1;
            best_phasecode_td=-1;
            best_needed_atten=0;
            time_needed=timedelay_needed(angle,spacing,c)+highest_time0_value;
            if (f==0) {
              allowed_td_span=1E13 ;
            } else { 
              allowed_td_span=0.05*time_needed;
            }
            if (allowed_td_span < 5.0) allowed_td_span=5.0;
            min_diff=1E13;
            min_span=1E13;
            min_ave_diff=1E13;
            within_allowed_span=1;
            within_allowed_td=0;
            for(p=0;p<num_phasecodes;p++) {
              ave_num_freqs=0;
              ave_phasecode_td=0.0;
              ave_pwr=0;
              ave_needed_atten=0;
              min_td=1E13;
              max_td=-1;
              phasecode_td_span=-1;
              for(i=0;i<num_freqs;i++) {
                if(freq[i] > frequency_lo ) {
                  if(freq[i] < frequency_hi ) {
                    if (timedelay[i][p] > max_td) {
                      max_td=timedelay[i][p];   
                    }
                    if (timedelay[i][p] < min_td) {
                      min_td=timedelay[i][p];   
                    }
                    ave_num_freqs++;
                    ave_phasecode_td+=timedelay[i][p];
/*
                    if(f==33 && b==0 && p==5655) {
                      printf("F_index: %d Num: %d Tdelay: %8.3lf Sum: %8.3lf Ave: %8.3lf\n",i,ave_num_freqs,timedelay[i][p],ave_phasecode_td,ave_phasecode_td/ave_num_freqs);
                    }
*/
                    ave_pwr+=pwr_mag[i][p];
                  } //freq range test
                } //freq range test 
              }  // freq loop  
              ave_phasecode_td=ave_phasecode_td/(float)ave_num_freqs;
              ave_pwr/=ave_num_freqs;
              if(p==0) pwr0=ave_pwr;
              ave_needed_atten=ave_pwr-lowest_pwr_mag;
/*
              if(fabs(ave_phasecode_td-time_needed) < min_diff) {
                min_diff=fabs(ave_phasecode_td-time_needed);
                best_phasecode=p;
                best_phasecode_td=ave_phasecode_td;
                best_needed_atten=ave_needed_atten;
              }
*/
              if (fabs(ave_phasecode_td-time_needed) < min_ave_diff) {
                min_ave_diff=fabs(ave_phasecode_td-time_needed);
                fallback_phasecode=p;
                fallback_phasecode_td=ave_phasecode_td;
                fallback_needed_atten=ave_needed_atten;
                fallback_td_span=max_td-min_td;
              }
              if (fabs(ave_phasecode_td-time_needed) < allowed_td_diff) {
                within_allowed_td+=1; 
                phasecode_td_span=max_td-min_td;
                if (phasecode_td_span < min_span) {
                  min_span=phasecode_td_span;
                  best_phasecode=p;
                  best_phasecode_td=ave_phasecode_td;
                  best_needed_atten=ave_needed_atten;
                }
              }
            } // phasecode loop  
            if (f==0) {
              best_phasecode=fallback_phasecode;
              best_phasecode_td=fallback_phasecode_td;
              best_needed_atten=fallback_needed_atten;
              min_span=fallback_td_span;
              if (min_span > untuned_span) {
                untuned_span=min_span;
              }
            } else {
              if(within_allowed_td == 0) {
                printf("ERROR: No phasecode with allowed ave timedelay difference\n");
                printf("  Fstep: %3d Beam: %3d Angle: %8.3lf F_lo %8.3lf F_hi: % 8.3lf Allowed Diff: %8.3lf\n",
                  f,b,angles[b],frequency_lo,frequency_hi,allowed_td_diff);
                best_phasecode=fallback_phasecode;
                best_phasecode_td=fallback_phasecode_td;
                best_needed_atten=fallback_needed_atten;
                min_span=fallback_td_span;
              
              } else {
                if(fallback_td_span < 5.0) {
                  if( fabs(fallback_phasecode_td-time_needed) < fabs(best_phasecode_td-time_needed) ) {
                    best_phasecode=fallback_phasecode;
                    best_phasecode_td=fallback_phasecode_td;
                    best_needed_atten=fallback_needed_atten;
                    min_span=fallback_td_span;
                    //printf("INFO: Using fallback phasecode with span < 5 nsecs\n");
                    //printf("  Fstep: %3d Beam: %3d Angle: %8.3lf F_lo %8.3lf F_hi: % 8.3lf Allowed Diff: %8.3lf\n",
                    //  f,b,angles[b],frequency_lo,frequency_hi,allowed_td_diff);
                  }
                }
              }
              if ( min_span < 0 || min_span > allowed_td_span) {
                within_allowed_span=0;
                printf("WARNING: No phasecode with allowed timedelay variance across span\n");
                printf("  Fstep: %3d Beam: %3d Angle: %8.3lf F_lo %8.3lf F_hi: % 8.3lf Allowed Diff: %8.3lf\n",
                  f,b,angles[b],frequency_lo,frequency_hi,allowed_td_span);
              }
              if (min_span > max_span) {
                max_span=min_span;
              }
            }
            // if best_phasecode < 0 error out
            beam_phasecode[b]=best_phasecode; 
            ave_phasecode_td=best_phasecode_td; 
            p=best_phasecode; 
            needed_atten=best_needed_atten;
            // find worst excursion from average in frequency range

            min_td=1E13;
            max_td=-1;
            max_diff=0.0;
            worst_phasecode_td=-1;
            for(i=0;i<num_freqs;i++) {
              if(freq[i] > frequency_lo ) {
                if(freq[i] < frequency_hi ) {
                  if (timedelay[i][p] > max_td) {
                      max_td=timedelay[i][p];   
                  }
                  if (timedelay[i][p] < min_td) {
                      min_td=timedelay[i][p];   
                  }
                  if (fabs(timedelay[i][p]-time_needed) > max_diff) {
                    //printf("New max diff: %lf :: %lf  %lf\n",freq[i],timedelay[i][p],time_needed);
                    max_diff=fabs(timedelay[i][p]-time_needed);
                    worst_phasecode_td=timedelay[i][p];
                  } 

                  if (fabs(timedelay[i][p]-time_needed) < min_diff) {
                    //printf("New min diff: %lf :: %lf  %lf\n",freq[i],timedelay[i][p],time_needed);
                    min_diff=fabs(timedelay[i][p]-time_needed);
                    best_phasecode_td=timedelay[i][p];
                  } 
                } //freq range test
              } //freq range test 
            }  // freq loop  
            phasecode_td_span=max_td-min_td;
            

            //find attenuation that best matches for the best phasecode for the freq span.
            best_attencode=0;
            best_atten_value=0.0;
            best_hi_attencode=0;
            best_hi_atten_value=-1E13;
            best_lo_attencode=0;
            best_lo_atten_value=1E13;
            for(ii=0;ii<64;ii++) {
              //average needed atten over frequency range
              ave_num_freqs=0;
              atten=0;
              for(i=0;i<num_freqs;i++) {
                if(freq[i] > frequency_lo ) {
                  if(freq[i] < frequency_hi ) {
                    ave_num_freqs++;
                    if(USE_MEASURED_ATTENS && attenfile_exists) {
                      atten+=-atten_pwr_mag[i][ii]+atten_pwr_mag[i][0];
                    } else {
                      for(j=0;j<6;j++) {
                        if((ii & (int32_t)pow(2,j))==pow(2,j)) {
                          atten+=atten_steps[j];
                        }
                      }
                    } 
                  } //freq range test
                } //freq range test 
              }  // freq loop  
              atten/=ave_num_freqs;
              //now calculate best attencode for average atten just caculated
              bad_atten_pwr=0;
              expected_atten=0;
              for(j=0;j<6;j++) {
                if((ii & (int32_t)pow(2,j))==pow(2,j)) {
                  expected_atten+=atten_steps[j];
                }
              }
              if(expected_atten > 0) {
                if((fabs(atten-expected_atten)/expected_atten) > 0.5 ) {
                  bad_atten_pwr=1;
                  for(j=0;j<6;j++) {
                    if(ii ==pow(2,j)) {
                      if(bad_atten_steps[j]==0)
                        printf("ii: %d Found a bad atten bit: %d measured: %lf expected: %lf\n",ii,j,atten,expected_atten);
                      bad_atten_steps[j]=1;
                    }
                  }
                  if(ick==0) {
                    ick=1;
                    printf(">>>>>>> Card: %d Problem with atten:\n",c);
                    printf("    Expected: %lf\n",expected_atten);
                    printf("     Measured: %lf\n",atten);
                  }
                }
              }
              for(j=0;j<6;j++) {
                if((ii & (int32_t)pow(2,j))==pow(2,j)) {
                        if(bad_atten_steps[j]==1) bad_atten_pwr=1;
                      }
              }
              if(bad_atten_pwr==0) {
                  /*  For a lower  atten code */
                if((atten-needed_atten) <= 0) {
                  if(fabs(atten-needed_atten) < fabs(best_lo_atten_value-needed_atten)) {
                    best_lo_attencode=ii;
                    best_lo_atten_value=atten;
                  }
                }
                  /*  For a higher  atten code */
                if((atten-needed_atten) >= 0) {
                  if(fabs(atten-needed_atten) < fabs(best_hi_atten_value-needed_atten)) {
                    best_hi_attencode=ii;
                    best_hi_atten_value=atten;
                  }
                }
              }
            } // end atten bit loop
            if (fabs(best_hi_atten_value-needed_atten) < 1.0 ) {
                    a=best_hi_attencode; 
                    av=best_hi_atten_value; 
            } else {
                    a=best_lo_attencode; 
                    av=best_lo_atten_value; 
            }
            best_attencode=a; 
            best_atten_value=av; 
            //load values into beam angle array structure and write out to file
            beam_pwr_mag[b]=needed_atten;
            beam_attencode[b]=best_attencode; 
            beam_atten_value[b]=best_atten_value; 
            printf("Freq Step: %d  Angle: %6.2lf  Beam: %5d Pcode: %5d :: T_n: %6.2lf T_ave: %6.2lf T_best: %6.2lf Max_Diff: %6.2lf Span: %6.2g  Allowed: %6.2g Pwr_0: %8.3lf Max pwr_mag: %8.3lf acode: %6d aval: %8.3lf\n",
              f,angle,b,beam_phasecode[b], time_needed,ave_phasecode_td,best_phasecode_td,max_diff,phasecode_td_span,allowed_td_span,pwr0,beam_pwr_mag[b],beam_attencode[b],beam_atten_value[b]);
          } //angle for loop b
          fwrite(&f,sizeof(int32_t),1,beamcodefile);
          fwrite(&frequency_lo,sizeof(double),1,beamcodefile);
          fwrite(&frequency_hi,sizeof(double),1,beamcodefile);
          fwrite(beam_pwr_mag,sizeof(double),num_angles,beamcodefile);
          fwrite(beam_attencode,sizeof(int32_t),num_angles,beamcodefile);
          fwrite(beam_phasecode,sizeof(int32_t),num_angles,beamcodefile);
        }  //freq step loop
        printf("Max Span:: Untuned: %6.2g  Freq Tuned: %6.2g\n",untuned_span,max_span);
        fclose(beamcodefile);
      }  //beamcode file if
    }  //timedelay file if

// free card arrays
  } //card loop
} //main
