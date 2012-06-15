#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define NEW_CARD 18  
#define OLD_CARD 18 
#define MAX_CARDS 1 
#define MAX_FREQS 1300
#define MAX_ATTENS 64 
#define EXAMINE_CODE 5809 
int verbose=1;
char new_radar_name[80]="spare";
//char new_radar_name[80]="kansas_west";
char old_radar_name[80]="spare";

char new_dirstub[80]="/home/jspaleta/Desktop/CV_phasing/calibrations/spare/";
//char new_dirstub[80]="/root/cal_files/";
char old_dirstub[80]="/home/jspaleta/Desktop/CV_phasing/calibrations/spare/";
FILE *old_calfile=NULL;
FILE *new_calfile=NULL;
struct timeval t0,t1,t2,t3;
unsigned long elapsed;
double expected_timedelays[14]={7.5,0.25,0.45,0.8,1.5,2.75,5.0,8.0,15.0,25.0,45.0,80.0,140.0,250.0};
double expected_attenuation[7]={0,0.5,1,2,4,8,16};
int main()
{
  int old_num_attencodes[MAX_CARDS],old_num_cards[MAX_CARDS],old_num_freqs[MAX_CARDS],old_active[MAX_CARDS];
  int new_num_attencodes[MAX_CARDS],new_num_cards[MAX_CARDS],new_num_freqs[MAX_CARDS],new_active[MAX_CARDS];
  int b,count,jump_adjust,adjusted;
  long c,i,ii;
  int f,d,p,o;
  char tmp;
  double *new_freq[MAX_CARDS],*new_phase[MAX_FREQS][MAX_CARDS],*new_pwr_mag[MAX_FREQS][MAX_CARDS];    
  double *old_freq[MAX_CARDS],*old_phase[MAX_FREQS][MAX_CARDS],*old_pwr_mag[MAX_FREQS][MAX_CARDS];    
  double ave_phase_diff,min_phase_diff,max_phase_diff=0,phase_diff=0;
  double old_offset,new_offset;
  double old_p,new_p;
  double max_delay,min_delay;
  double max_expected_attenuation,max_pwr,min_pwr,max_pwr_diff,pwr_diff;
  char new_filename[120];
  char old_filename[120];
  int errflag=0;
  int code, index;
  if (verbose > 0 ) printf("Prepping\n");
  if (verbose > 0 ) printf("Nulling arrays\n");
  for(c=0; c <= 0 ; c++) {
    new_active[c]=0;
    new_freq[c]=NULL;
    new_num_freqs[c]=0;
    for(ii=0; ii<=MAX_FREQS; ii=ii+1) {
        new_phase[ii][c]=NULL;
        new_pwr_mag[ii][c]=NULL;
    }
  }
  if (verbose > 0 ) printf("Done Nulling arrays\n");
  for(c=0;c<=0;c++) {
    sprintf(new_filename,"%s/phasing_cal_%s_%d.att",new_dirstub,new_radar_name,NEW_CARD);
    new_calfile=fopen(new_filename,"r");
    if (verbose > 0 ) fprintf(stdout,"Opening: %p %s\n",new_calfile,new_filename); 
    if (new_calfile!=NULL) {
      errflag=0;
      new_active[c]=1;
      fread(&new_num_attencodes[c],sizeof(int),1,new_calfile);
      if (verbose > 0 ) fprintf(stdout,"New AttenCodes: %d\n",new_num_attencodes[c]);
      fread(&new_num_cards[c],sizeof(int),1,new_calfile);
      if (verbose > 0 ) fprintf(stdout,"Cards: %d\n",new_num_cards[c]);
      fread(&new_num_freqs[c],sizeof(int),1,new_calfile);
      if (verbose > 0 ) fprintf(stdout,"Freqs: %d\n",new_num_freqs[c]);
      if (new_num_freqs[c]>MAX_FREQS) {
        fprintf(stderr,"Too many stored frequencies...up the MAX_FREQS define!\n");
        exit(0);
      }

      if (verbose > 0 ) fprintf(stdout,"Allocating arrays\n");
      for(i=0;i<new_num_freqs[c];i++) {
        if (new_freq[c]!=NULL) free(new_freq[c]);
        new_freq[c]=calloc(new_num_freqs[c],sizeof(double));
        if (new_phase[i][c]!=NULL) free(new_phase[i][c]);
        new_phase[i][c]=calloc(new_num_attencodes[c],sizeof(double));
        if (new_pwr_mag[i][c]!=NULL) free(new_pwr_mag[i][c]);
        new_pwr_mag[i][c]=calloc(new_num_attencodes[c],sizeof(double));

      }


      if (verbose > 0 ) fprintf(stdout,"Reading frequency array\n");

      count=fread(new_freq[c],sizeof(double),new_num_freqs[c],new_calfile);
      if (verbose > 0 )fprintf(stdout,"%d %d\n",new_num_freqs[c],count);
      count=1;
      if (verbose > 0 ) fprintf(stdout,"Reading in data\n");
      while(count>0) {
          count=fread(&ii,sizeof(int),1,new_calfile);
          if (count==0) {
            break;
          }
          count=fread(new_phase[ii][c],sizeof(double),new_num_attencodes[c],new_calfile);
          if (verbose > 1) fprintf(stdout,"Freq index: %d Phase Count: %d\n",ii,count);
          count=fread(new_pwr_mag[ii][c],sizeof(double),new_num_attencodes[c],new_calfile);
          if (verbose > 1) fprintf(stdout,"Freq index: %d Pwr-mag Count: %d\n",ii,count);
          if (count==0) {
            break;
          }
/*
        for(i=0;i<new_num_attencodes[c];i++)
            printf("%lf : %d : %lf\n",new_freq[c][0],i,new_pwr_mag[0][c][i]);
        exit(0);
*/
      }
      if (count==0) {
        if (feof(new_calfile)) if (verbose > 0 ) fprintf(stdout,"End of File!\n");
      }
      fclose(new_calfile);
    } // good new_calfile 



    sprintf(old_filename,"%s/phasing_cal_%s_%d.att",old_dirstub,old_radar_name,OLD_CARD);
    old_calfile=fopen(old_filename,"r");
    if (verbose > 0 ) fprintf(stdout,"Opening: %p %s\n",old_calfile,old_filename); 
    if (old_calfile!=NULL) {
      errflag=0;
      old_active[c]=1;
      fread(&old_num_attencodes[c],sizeof(int),1,old_calfile);
      if (verbose > 0 ) fprintf(stdout," Old AttenCodes: %d\n",old_num_attencodes[c]);
      fread(&old_num_cards[c],sizeof(int),1,old_calfile);
      if (verbose > 0 ) fprintf(stdout,"Cards: %d\n",old_num_cards[c]);
      fread(&old_num_freqs[c],sizeof(int),1,old_calfile);
      if (verbose > 0 ) fprintf(stdout,"Freqs: %d\n",old_num_freqs[c]);
      if (old_num_freqs[c]>MAX_FREQS) {
        fprintf(stderr,"Too many stored frequencies...up the MAX_FREQS define!\n");
        exit(0);
      }

      if (verbose > 0 ) fprintf(stdout,"Allocating arrays\n");
      for(i=0;i<old_num_freqs[c];i++) {
        if (old_freq[c]!=NULL) free(old_freq[c]);
        old_freq[c]=calloc(old_num_freqs[c],sizeof(double));
        if (old_phase[i][c]!=NULL) free(old_phase[i][c]);
        old_phase[i][c]=calloc(old_num_attencodes[c],sizeof(double));
        if (old_pwr_mag[i][c]!=NULL) free(old_pwr_mag[i][c]);
        old_pwr_mag[i][c]=calloc(old_num_attencodes[c],sizeof(double));
      }


      if (verbose > 0 ) fprintf(stdout,"Reading frequency array\n");

      count=fread(old_freq[c],sizeof(double),old_num_freqs[c],old_calfile);
      if (verbose > 0 )fprintf(stdout,"%d %d\n",old_num_freqs[c],count);
      count=1;
      if (verbose > 0 ) fprintf(stdout,"Reading in data\n");
      while(count>0) {
          count=fread(&ii,sizeof(int),1,old_calfile);
          if (count==0) {
            break;
          }
          count=fread(old_phase[ii][c],sizeof(double),old_num_attencodes[c],old_calfile);
          if (verbose > 1) fprintf(stdout,"Freq index: %d Phase Count: %d\n",ii,count);
          count=fread(old_pwr_mag[ii][c],sizeof(double),old_num_attencodes[c],old_calfile);
          if (verbose > 1) fprintf(stdout,"Freq index: %d Pwr-mag Count: %d\n",ii,count);
          if (count==0) {
            break;
          }
      }
      if (count==0) {
        if (feof(old_calfile)) if (verbose > 0 ) fprintf(stdout,"End of File!\n");
      }
      fclose(old_calfile);
    } // good old_calfile 

    printf("Check the max power  Old/New difference at zero delay\n");
    max_pwr_diff=-1E6;
    for(i=0;i<new_num_freqs[c];i++) {
      pwr_diff=fabs(old_pwr_mag[i][c][0]-new_pwr_mag[i][c][0]);
      //printf("%lf : %lf %lf\n",new_freq[c][i],old_pwr_mag[i][c][0],new_pwr_mag[i][c][0]);
      if(pwr_diff > max_pwr_diff) max_pwr_diff=pwr_diff;
    }
    printf("  Max Pwr Diff %lf\n",max_pwr_diff);

    printf("Check the max power  Old/New difference across all delays\n");
    max_pwr_diff=-1E6;
    for(p=0;p<new_num_attencodes[c];p++) {
      for(i=0;i<new_num_freqs[c];i++) {
        pwr_diff=fabs(old_pwr_mag[i][c][p]-new_pwr_mag[i][c][p]);
        if(pwr_diff > max_pwr_diff) max_pwr_diff=pwr_diff;
      }
    }
    printf("  Max Pwr Diff %lf\n",max_pwr_diff);

    printf("Check the lowest power  across all delays\n");
    max_pwr_diff=1E6;
    for(p=0;p<new_num_attencodes[c];p++) {
      for(i=0;i<new_num_freqs[c];i++) {
        pwr_diff=old_pwr_mag[i][c][p];
        if(pwr_diff < max_pwr_diff) {
          max_pwr_diff=pwr_diff;
          index=i;
          code=p;
        }
      }
    }
    printf("  Lowest Old Pwr %lf freq index:  %d code: %d\n",max_pwr_diff,index,code);
    if (max_pwr_diff < 0.0) {
        for(i=0;i<new_num_freqs[c];i++) {
        //  printf("%lf : %lf %lf\n",old_freq[c][i],old_phase[i][c][code],old_pwr_mag[i][c][code]);
        }
    }
    max_pwr_diff=1E6;
    for(p=0;p<new_num_attencodes[c];p++) {
      for(i=0;i<new_num_freqs[c];i++) {
        pwr_diff=new_pwr_mag[i][c][p];
        if(pwr_diff < max_pwr_diff) {
          max_pwr_diff=pwr_diff;
          index=i;
          code=p;
        }
      }
    }

    printf("  Lowest New Pwr %lf freq index:  %d code: %d\n",max_pwr_diff,index,code);
    for(ii=0;ii<=6;ii++) {
      max_pwr_diff=1E6;
      if(ii==0) p=0;
      else p= 1 << (ii-1);
      for(i=0;i<new_num_freqs[c];i++) {
        pwr_diff=new_pwr_mag[i][c][p];
        if(pwr_diff < max_pwr_diff) {
          max_pwr_diff=pwr_diff;
          index=i;
          code=p;
        }
      }
      printf("  %d Lowest New Pwr %lf freq index:  %d code: %d\n",p,max_pwr_diff,index,code);
    }


    printf(" Check New Atten Bit Freq Variance\n");
    for(ii=0;ii<=6;ii++) {
      max_pwr=-1E6;
      min_pwr=1E6;
      if(ii==0) p=0;
      else p= 1 << (ii-1);
      for(i=0;i<new_num_freqs[c];i++) {
        new_p=new_pwr_mag[i][c][p];
        if(new_p > max_pwr) {
          max_pwr=new_p;
        }
        if(new_p < min_pwr) {
          min_pwr=new_p;
        }
      }
      pwr_diff=fabs(max_pwr-min_pwr);
      printf("  %d Min Pwr: %lf Max Pwr:  %lf Diff: %lf\n",p,min_pwr,max_pwr,pwr_diff);
    }

    printf(" Check New Bitwise Attenuation Frequency Average\n");
    for(ii=0;ii<=6;ii++) {
        if(ii==0) p=0;
        else p= 1 << (ii-1);
        pwr_diff=0;
        for(i=0;i<new_num_freqs[c];i++) {
          old_p=new_pwr_mag[i][c][p];
          if (p==0) { 
            new_offset=old_p;
            pwr_diff=0.0;

          } else {
            pwr_diff+=new_offset-old_p;
          }
        }
        pwr_diff/=new_num_freqs[c];
        printf("%d New Attenuation [dB]:  %e %e\n",p,pwr_diff,expected_attenuation[ii]);
    }
    max_expected_attenuation=0.0;
    for(ii=0;ii<=6;ii++) {
      max_expected_attenuation+=expected_attenuation[ii];
    }
    pwr_diff=0;
    for(i=0;i<new_num_freqs[c];i++) {
      pwr_diff+=new_offset-new_pwr_mag[0][c][63];
    }
    pwr_diff/=new_num_freqs[c];
    printf("63 New Max Attenuation [dB]:  %e %e\n",pwr_diff,max_expected_attenuation);
  } // end card loop


  exit(0);
}
