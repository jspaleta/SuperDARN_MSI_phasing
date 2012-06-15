#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <gsl/gsl_fit.h>

#define MAX_CARDS 20 
#define MAX_FREQS 1500
#define MAX_PHASES 8192

int verbose=1;
char radar_name1[80]="kansas_tx";
char radar_name2[80]="kansas_tx_repair";
int CARD=0;
FILE *calfile1=NULL;
FILE *calfile2=NULL;
FILE *summaryfile=NULL;
struct timeval t0,t1,t2,t3;
unsigned long elapsed;
double expected_timedelays[13]={0.25,0.45,0.8,1.5,2.75,5.0,8.0,15.0,25.0,45.0,80.0,140.0,250.0};
double pwr_threshold=-20.0;

double expected_timedelay(int delaycode) {
  int bit,i,code;
  double timedelay=0;
  code=delaycode;
  for (i=0;i<13;i++) {
    bit=(code & 0x1);
    timedelay+=bit*expected_timedelays[i];
    code=code >> 1;
  }
  return timedelay;
}

double phase_to_timedelay(double phase,double freq)
{
/*
* phase in degrees  freq in Hz timedelay in ns   ratio (0-1)
*/
  double timedelay=0;
  timedelay=phase/360.0/freq*1E9; // nanoseconds
  return -timedelay;
}
int main()
{
  int num_phasecodes[MAX_CARDS],num_cards[MAX_CARDS],num_freqs[MAX_CARDS],active[MAX_CARDS];
  int i,b,c,ii,cc,count,summary_freqs;
  int f,d,p,o;
  double offset,slope,slope_variance,sumsq;
  double best_sumsq[MAX_PHASES],best_offset[MAX_PHASES],best_slope[MAX_PHASES],best_var[MAX_PHASES];
  char tmp;
  int lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  double lowest_pwr_mag=1E10; // freq,card,phasecode
  int highest_time_delay_card[MAX_FREQS]; // freq
  double highest_time_delay[MAX_FREQS];
  double *freq[MAX_CARDS],*phase[MAX_FREQS][MAX_CARDS];
  double *pwr_mag[MAX_FREQS][MAX_CARDS];
  double *timedelay1[MAX_FREQS][MAX_CARDS];    
  double *timedelay2[MAX_FREQS][MAX_CARDS];    
  double minimum_timedelay=0;
  double Y[MAX_FREQS],time0[MAX_FREQS];    
  char filename1[120];
  char filename2[120];
  double max_Y;
  double diff,var,max_var[13],max_var0;
  int  hmm;
  for(i=0;i<MAX_FREQS;i++) {
    highest_time_delay_card[i]=-1;
    highest_time_delay[i]=-1000;
  }
  printf("Nulling arrays\n");
  for(c=0;c<MAX_CARDS;c++) {
    active[c]=0;
    freq[c]=NULL;
    num_freqs[c]=0;
    for(i=0;i<MAX_FREQS;i++) {
        phase[i][c]=NULL;
        pwr_mag[i][c]=NULL;
        timedelay1[i][c]=NULL;
        timedelay2[i][c]=NULL;
    }
  }
  for(c=CARD;c<=CARD;c++) {
    sprintf(filename1,"/tmp/phasing_cal_%s_%d.dat",radar_name1,c);
    calfile1=fopen(filename1,"r");
    printf("1: %p %s\n",calfile1,filename1); 
    if (calfile1!=NULL ) {
      fread(&num_phasecodes[c],sizeof(int),1,calfile1);
      fread(&num_cards[c],sizeof(int),1,calfile1);
      fread(&num_freqs[c],sizeof(int),1,calfile1);
      if (num_freqs[c]>MAX_FREQS) {
        printf("Too many stored frequencies...up the MAX_FREQS define!\n");
        exit(0);
      }

      printf("Allocating arrays\n");
      for(i=0;i<num_freqs[c];i++) {
        if (freq[c]!=NULL) free(freq[c]);
        freq[c]=calloc(num_freqs[c],sizeof(double));
        if (phase[i][c]!=NULL) free(phase[i][c]);
        phase[i][c]=calloc(num_phasecodes[c],sizeof(double));
        if (pwr_mag[i][c]!=NULL) free(pwr_mag[i][c]);
        pwr_mag[i][c]=calloc(num_phasecodes[c],sizeof(double));
        if (timedelay1[i][c]!=NULL) free(timedelay1[i][c]);
        timedelay1[i][c]=calloc(num_phasecodes[c],sizeof(double));
      }


      printf("Reading frequency array\n");

      count=fread(freq[c],sizeof(double),num_freqs[c],calfile1);
      count=1;
      printf("Reading in data\n");
      while(count>0) {
          count=fread(&ii,sizeof(int),1,calfile1);
          if (count==0) {
            break;
          }
          count=fread(phase[ii][c],sizeof(double),num_phasecodes[c],calfile1);
          if (verbose > 1) printf("Freq index: %d Phase Count: %d\n",ii,count);
          count=fread(pwr_mag[ii][c],sizeof(double),num_phasecodes[c],calfile1);
          if (verbose > 1) printf("Freq index: %d Pwr-mag Count: %d\n",ii,count);
          if (count==0) {
            break;
          }
      }
      if (count==0) {
        if (feof(calfile1)) printf("End of File!\n");
      }
      fclose(calfile1);
      printf("Processing Phase Information for Radar: %s Card: %d\n",radar_name1,c);
      for (p=0;p<num_phasecodes[c];p++) {
//        printf("Phase Code: %d\n",p);

        best_offset[p]=1000;
        best_sumsq[p]=1E100;
        for (o=-20;o<20;o++) {
          offset=360.0*o;
          max_Y=-1000;
          for (i=0;i<num_freqs[c];i++) {
            Y[i]=phase[i][c][p]+offset;
            if (Y[i]> max_Y) max_Y=Y[i];
          }
/*
*  Linear Regression Y=slope*freq
*/
          gsl_fit_mul (freq[c], 1, Y, 1, num_freqs[c], &slope, &slope_variance, &sumsq);
          if (max_Y < 0 ) {
            if (sumsq < best_sumsq[p]) {
              best_offset[p]=offset; 
              best_sumsq[p]=sumsq;
              best_var[p]=slope_variance;
              best_slope[p]=slope;
            }
          }
        } // end offset loop
        printf("%d :: Best Offset: %lf  Phase0: %lf\n",p,best_offset[p],phase[0][c][p]);
/*
*  Build the timedelays arrays using the best phase offset from the linear regression
*/
        for (i=0;i<num_freqs[c];i++) {
          time0[i]=phase_to_timedelay(phase[i][c][0]+best_offset[0],freq[c][i]);
          timedelay1[i][c][p]=phase_to_timedelay(phase[i][c][p]+best_offset[p],freq[c][i]);
          if((phase[i][c][p]+best_offset[p]) > 0 ) {
            printf("ERROR: Phase error: %d %d %d %lfi %lf\n",i,c,p,phase[i][c][p],best_offset[p]);
            exit(0);
      
          }
//          printf("Phase: %d :: Freq %e :: Time Delay %% Diff  %lf\n",
//                p,freq[c][i],fabs((timedelay[i][c][p]-expected_timedelay(p)-time0[i])/(expected_timedelay(p)+time0[i]))*100);
        }  //freq loop
        if(timedelay1[0][c][p] < minimum_timedelay) {
          printf("ERROR: Timedelay1 switch error: %d %lf\n",p,timedelay1[0][c][p]);
          exit(0);
        }
        if(pwr_mag[0][c][p] < pwr_threshold) {
          printf("ERROR: pwr_mag switch error: %d\n",p);
          exit(0);
        }

      }  //phasecode loop
    } // end calfile1 check

    sprintf(filename2,"/tmp/phasing_cal_%s_%d.dat",radar_name2,c);
    calfile2=fopen(filename2,"r");
    printf("1: %p %s\n",calfile2,filename2); 
    if (calfile2!=NULL ) {
      fread(&num_phasecodes[c],sizeof(int),1,calfile2);
      fread(&num_cards[c],sizeof(int),1,calfile2);
      fread(&num_freqs[c],sizeof(int),1,calfile2);
      if (num_freqs[c]>MAX_FREQS) {
        printf("Too many stored frequencies...up the MAX_FREQS define!\n");
        exit(0);
      }

      printf("Allocating arrays\n");
      for(i=0;i<num_freqs[c];i++) {
        if (freq[c]!=NULL) free(freq[c]);
        freq[c]=calloc(num_freqs[c],sizeof(double));
        if (phase[i][c]!=NULL) free(phase[i][c]);
        phase[i][c]=calloc(num_phasecodes[c],sizeof(double));
        if (pwr_mag[i][c]!=NULL) free(pwr_mag[i][c]);
        pwr_mag[i][c]=calloc(num_phasecodes[c],sizeof(double));
        if (timedelay2[i][c]!=NULL) free(timedelay2[i][c]);
        timedelay2[i][c]=calloc(num_phasecodes[c],sizeof(double));
      }


      printf("Reading frequency array\n");

      count=fread(freq[c],sizeof(double),num_freqs[c],calfile2);
      count=1;
      printf("Reading in data\n");
      while(count>0) {
          count=fread(&ii,sizeof(int),1,calfile2);
          if (count==0) {
            break;
          }
          count=fread(phase[ii][c],sizeof(double),num_phasecodes[c],calfile2);
          if (verbose > 1) printf("Freq index: %d Phase Count: %d\n",ii,count);
          count=fread(pwr_mag[ii][c],sizeof(double),num_phasecodes[c],calfile2);
          if (verbose > 1) printf("Freq index: %d Pwr-mag Count: %d\n",ii,count);
          if (count==0) {
            break;
          }
      }
      if (count==0) {
        if (feof(calfile2)) printf("End of File!\n");
      }
      fclose(calfile2);
      printf("Processing Phase Information for Radar: %s Card: %d\n",radar_name2,c);
      for (p=0;p<num_phasecodes[c];p++) {
//        printf("Phase Code: %d\n",p);

        best_offset[p]=1000;
        best_sumsq[p]=1E100;
        for (o=-20;o<20;o++) {
          offset=360.0*o;
          max_Y=-1000;
          for (i=0;i<num_freqs[c];i++) {
            Y[i]=phase[i][c][p]+offset;
            if (Y[i]> max_Y) max_Y=Y[i];
          }
/*
*  Linear Regression Y=slope*freq
*/
          gsl_fit_mul (freq[c], 1, Y, 1, num_freqs[c], &slope, &slope_variance, &sumsq);
          if (max_Y < 0 ) {
            if (sumsq < best_sumsq[p]) {
            
              best_offset[p]=offset; 
              best_sumsq[p]=sumsq;
              best_var[p]=slope_variance;
              best_slope[p]=slope;
            }
          }
        } // end offset loop
        printf("%d :: Best Offset: %lf  Phase0: %lf\n",p,best_offset[p],phase[0][c][p]);
/*
*  Build the timedelays arrays using the best phase offset from the linear regression
*/
        for (i=0;i<num_freqs[c];i++) {
          time0[i]=phase_to_timedelay(phase[i][c][0]+best_offset[0],freq[c][i]);
          timedelay2[i][c][p]=phase_to_timedelay(phase[i][c][p]+best_offset[p],freq[c][i]);
//          printf("Phase: %d :: Freq %e :: Time Delay %% Diff  %lf\n",
//                p,freq[c][i],fabs((timedelay[i][c][p]-expected_timedelay(p)-time0[i])/(expected_timedelay(p)+time0[i]))*100);
        }  //freq loop
        if(timedelay2[0][c][p] < minimum_timedelay) {
          printf("ERROR: Timedelay2 switch error: %d %lf\n",p,timedelay2[0][c][p]);
          exit(0);
        }
        if(pwr_mag[0][c][p] < pwr_threshold) {
          printf("ERROR: pwr_mag switch error: %d\n",p);
          exit(0);
        }

      }  //phasecode loop


    } // end calfile2 check
    max_var0=-1000;
    for (b=0;b<13;b++) {
      max_var[b]=-1000.0;
    }
    for (p=0;p<num_phasecodes[c];p++) {
      for (i=0;i<num_freqs[c];i++) {
        diff=timedelay1[i][c][p]-timedelay2[i][c][p];
        var=fabs(diff);
        if (p == 0 ) {
         if (var > max_var0) max_var0=var;
        }
        for (b=0;b<13;b++) {
          hmm=pow(2,b);
          if( p == hmm ) {
            if (var > max_var[b]) max_var[b]=var;
          }
        }
      }
    }

    printf("Code: %d Max Diff (ns): %lf\n",0,max_var0);
    for (b=0;b<13;b++) {
      printf("Code: %d Max Diff (ns): %lf\n",(int)pow(2,b),max_var[b]);
    }
  }  // End card Loop

} // end of main

