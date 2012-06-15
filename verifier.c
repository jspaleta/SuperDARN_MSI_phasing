#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#define NEW_CARD 0  
#define OLD_CARD 0 
#define MAX_CARDS 1 
#define MAX_FREQS 1300
#define MAX_PHASES 8192
#define EXAMINE_CODE 5809 
int32_t verbose=2;
char new_radar_name[80]="ade_mod";
//char new_radar_name[80]="kansas_west";
char old_radar_name[80]="ade";

char new_dirstub[80]="/home/jspaleta/data/calibrations/ade/";
//char new_dirstub[80]="/root/cal_files/";
char old_dirstub[80]="/home/jspaleta/data/calibrations/ade/";
FILE *old_calfile=NULL;
FILE *new_calfile=NULL;
struct timeval t0,t1,t2,t3;
unsigned long elapsed;
double expected_timedelays[14]={7.5,0.25,0.45,0.8,1.5,2.75,5.0,8.0,15.0,25.0,45.0,80.0,140.0,250.0};
int32_t main()
{
  int32_t old_num_phasecodes[MAX_CARDS],old_num_cards[MAX_CARDS],old_num_freqs[MAX_CARDS],old_active[MAX_CARDS];
  int32_t new_num_phasecodes[MAX_CARDS],new_num_cards[MAX_CARDS],new_num_freqs[MAX_CARDS],new_active[MAX_CARDS];
  int32_t b,count,jump_adjust,adjusted;
  long c,i,ii;
  int32_t f,d,p,o;
  char tmp;
  double *new_freq[MAX_CARDS],*new_phase[MAX_FREQS][MAX_CARDS],*new_pwr_mag[MAX_FREQS][MAX_CARDS];    
  double *old_freq[MAX_CARDS],*old_phase[MAX_FREQS][MAX_CARDS],*old_pwr_mag[MAX_FREQS][MAX_CARDS];    
  double ave_phase_diff,min_phase_diff,max_phase_diff=0,phase_diff=0;
  double old_offset,new_offset;
  double old_p,new_p;
  double max_delay,min_delay;
  double max_pwr_diff,pwr_diff;
  char new_filename[120];
  char old_filename[120];
  int32_t errflag=0;
  int32_t code, index;
  if (verbose > 0 ) printf("Prepping\n");
  if (verbose > 0 ) printf("Nulling arrays\n");
  for(c=0; c < MAX_CARDS ; c++) {
    new_active[c]=0;
    new_freq[c]=NULL;
    new_num_freqs[c]=0;
    for(ii=0; ii<=MAX_FREQS; ii=ii+1) {
        printf("C: %d ii: %d\n",c,ii);
        new_phase[ii][c]=NULL;
        new_pwr_mag[ii][c]=NULL;
    }
  }
  if (verbose > 0 ) printf("Done Nulling arrays\n");
  for(c=0;c<=0;c++) {
    sprintf(new_filename,"%s/phasing_cal_%s_%02d.dat",new_dirstub,new_radar_name,NEW_CARD);
    new_calfile=fopen(new_filename,"r");
    if (verbose > 0 ) fprintf(stdout,"Opening: %p %s\n",new_calfile,new_filename); 
    if (new_calfile!=NULL) {
      errflag=0;
      new_active[c]=1;
      fread(&new_num_phasecodes[c],sizeof(int32_t),1,new_calfile);
      if (verbose > 0 ) fprintf(stdout,"New PhaseCodes: %d\n",new_num_phasecodes[c]);
      fread(&new_num_cards[c],sizeof(int32_t),1,new_calfile);
      if (verbose > 0 ) fprintf(stdout,"Cards: %d\n",new_num_cards[c]);
      fread(&new_num_freqs[c],sizeof(int32_t),1,new_calfile);
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
        new_phase[i][c]=calloc(new_num_phasecodes[c],sizeof(double));
        if (new_pwr_mag[i][c]!=NULL) free(new_pwr_mag[i][c]);
        new_pwr_mag[i][c]=calloc(new_num_phasecodes[c],sizeof(double));
      }


      if (verbose > 0 ) fprintf(stdout,"Reading frequency array\n");

      count=fread(new_freq[c],sizeof(double),new_num_freqs[c],new_calfile);
      if (verbose > 0 )fprintf(stdout,"%d %d\n",new_num_freqs[c],count);
      count=1;
      if (verbose > 0 ) fprintf(stdout,"Reading in data\n");
      while(count>0) {
          count=fread(&ii,sizeof(int32_t),1,new_calfile);
          if (count==0) {
            break;
          }
          count=fread(new_phase[ii][c],sizeof(double),new_num_phasecodes[c],new_calfile);
          if (verbose > 1) fprintf(stdout,"Freq index: %d Phase Count: %d\n",ii,count);
          count=fread(new_pwr_mag[ii][c],sizeof(double),new_num_phasecodes[c],new_calfile);
/*
          if(ii==100) { 
            for(i=0;i<new_num_phasecodes[c];i++)
              printf("%lf : %d : %lf\n",new_freq[c][ii],i,new_pwr_mag[ii][c][i]);
            exit(0);
          }
*/
          if (verbose > 1) fprintf(stdout,"Freq index: %d Pwr-mag Count: %d\n",ii,count);
          if (count==0) {
            break;
          }
      }
      if (count==0) {
        if (feof(new_calfile)) if (verbose > 0 ) fprintf(stdout,"End of File!\n");
      }
      fclose(new_calfile);
    } // good new_calfile 



    sprintf(old_filename,"%s/phasing_cal_%s_%02d.dat",old_dirstub,old_radar_name,OLD_CARD);
    old_calfile=fopen(old_filename,"r");
    if (verbose > 0 ) fprintf(stdout,"Opening: %p %s\n",old_calfile,old_filename); 
    if (old_calfile!=NULL) {
      errflag=0;
      old_active[c]=1;
      fread(&old_num_phasecodes[c],sizeof(int32_t),1,old_calfile);
      if (verbose > 0 ) fprintf(stdout," Old PhaseCodes: %d\n",old_num_phasecodes[c]);
      fread(&old_num_cards[c],sizeof(int32_t),1,old_calfile);
      if (verbose > 0 ) fprintf(stdout,"Cards: %d\n",old_num_cards[c]);
      fread(&old_num_freqs[c],sizeof(int32_t),1,old_calfile);
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
        old_phase[i][c]=calloc(old_num_phasecodes[c],sizeof(double));
        if (old_pwr_mag[i][c]!=NULL) free(old_pwr_mag[i][c]);
        old_pwr_mag[i][c]=calloc(old_num_phasecodes[c],sizeof(double));
      }


      if (verbose > 0 ) fprintf(stdout,"Reading frequency array\n");

      count=fread(old_freq[c],sizeof(double),old_num_freqs[c],old_calfile);
      if (verbose > 0 )fprintf(stdout,"%d %d\n",old_num_freqs[c],count);
      count=1;
      if (verbose > 0 ) fprintf(stdout,"Reading in data\n");
      while(count>0) {
          count=fread(&ii,sizeof(int32_t),1,old_calfile);
          if (count==0) {
            break;
          }
          count=fread(old_phase[ii][c],sizeof(double),old_num_phasecodes[c],old_calfile);
          if (verbose > 1) fprintf(stdout,"Freq index: %d Phase Count: %d\n",ii,count);
          count=fread(old_pwr_mag[ii][c],sizeof(double),old_num_phasecodes[c],old_calfile);
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

//    if(old_num_phasecodes[c]!=new_num_phasecodes[c]) {
//      fprintf(stderr,"Num phasecodes mismatch in Cal\n");
//      exit(0);
//    }
//    if(old_num_freqs[c]!=new_num_freqs[c]) {
//      fprintf(stderr,"Num freqs mismatch in Cal\n");
//      exit(0);
//    }
    printf("Check the phase delay drift for Zero length delay\n");
    max_delay=-1E6;
    min_delay=1E6;
    for(i=0;i<new_num_freqs[c];i++) {
      if(old_phase[i][c][0] > max_delay) max_delay=old_phase[i][c][0];
      if(old_phase[i][c][0] < min_delay) min_delay=old_phase[i][c][0];
    }
    printf("  Old Phase Drift %lf\n",max_delay-min_delay);
    max_delay=-1E6;
    min_delay=1E6;
    for(i=0;i<new_num_freqs[c];i++) {
      if(new_phase[i][c][0] > max_delay) max_delay=new_phase[i][c][0];
      if(new_phase[i][c][0] < min_delay) min_delay=new_phase[i][c][0];
    }
    printf("  new Phase Drift %lf\n",max_delay-min_delay);
    printf("Check the phase delay drift for 100th length delay\n");
    max_delay=-1E6;
    min_delay=1E6;
    for(i=0;i<new_num_freqs[c];i++) {
      if(old_phase[i][c][100] > max_delay) max_delay=old_phase[i][c][100];
      if(old_phase[i][c][100] < min_delay) min_delay=old_phase[i][c][100];
    }
    printf("  Old Phase Drift %lf\n",max_delay-min_delay);
    max_delay=-1E6;
    min_delay=1E6;
    for(i=0;i<new_num_freqs[c];i++) {
      if(new_phase[i][c][100] > max_delay) max_delay=new_phase[i][c][100];
      if(new_phase[i][c][100] < min_delay) min_delay=new_phase[i][c][100];
    }
    printf("  new Phase Drift %lf\n",max_delay-min_delay);
   
    printf("Check the max power  difference at zero delay\n");
    max_pwr_diff=-1E6;
    for(i=0;i<new_num_freqs[c];i++) {
      pwr_diff=fabs(old_pwr_mag[i][c][0]-new_pwr_mag[i][c][0]);
      //printf("%lf : %lf %lf\n",new_freq[c][i],old_pwr_mag[i][c][0],new_pwr_mag[i][c][0]);
      if(pwr_diff > max_pwr_diff) max_pwr_diff=pwr_diff;
    }
    printf("  Max Pwr Diff abs(Old-New) %lf\n",max_pwr_diff);

    printf("Check the max power  difference across all delays\n");
    max_pwr_diff=-1E6;
    for(p=0;p<new_num_phasecodes[c];p++) {
      for(i=0;i<new_num_freqs[c];i++) {
        pwr_diff=fabs(old_pwr_mag[i][c][p]-new_pwr_mag[i][c][p]);
        if(pwr_diff > max_pwr_diff) max_pwr_diff=pwr_diff;
      }
    }
    printf("  Max Pwr Diff abs(Old-New) Across all Delays %lf\n",max_pwr_diff);

    printf("Check the lowest New power across all delays\n");

    for(ii=0;ii<=13;ii++) {
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
      printf(" %d:  Lowest New Pwr %lf freq index:  %d code: %d\n",p,max_pwr_diff,index,code);
    }

    printf(" Check Old Phase Bit linearity\n");
    for(ii=0;ii<=13;ii++) {
      jump_adjust=1;
      adjusted=0;
      while(jump_adjust) {
        jump_adjust=0;
        max_phase_diff=-1E6;
        min_phase_diff=1E6;
        ave_phase_diff=0.0;
        if(ii==0) p=0;
        else p= 1 << (ii-1);
        for(i=1;i<new_num_freqs[c];i++) {
          old_p=old_phase[i-1][c][p];
          new_p=old_phase[i][c][p];
          phase_diff=fabs(new_p-old_p);
          ave_phase_diff+=phase_diff; 
          if (phase_diff >= 360) {
//            printf("Warning: OLD 360 phase:: %d %d : %lf %lf\n",i,p,old_phase[i-1][c][p],old_phase[i][c][p]); 
            if( (new_p - old_p) >=  360 )  old_phase[i][c][p] -=360.0;
            if( (new_p - old_p) <= -360 )  old_phase[i][c][p] +=360.0;
            jump_adjust=1;
            break;
          }
          if(phase_diff < min_phase_diff) min_phase_diff=phase_diff;
          if(phase_diff > min_phase_diff) max_phase_diff=phase_diff;
        }
        if(!jump_adjust) {
          ave_phase_diff=ave_phase_diff/(double)(new_num_freqs[c]-1.0);
          if(adjusted) printf(" *");
          else printf("  ");
          printf("%d Old Pairwise Phase Diff: %lf %lf : %lf\n",p,min_phase_diff,max_phase_diff,ave_phase_diff);
        }
      }  
    }

    printf(" Check New Phase Bit linearity\n");
    for(ii=0;ii<=13;ii++) {
      jump_adjust=1;
      adjusted=0;
      while(jump_adjust) {
        jump_adjust=0;
        max_phase_diff=-1E6;
        min_phase_diff=1E6;
        ave_phase_diff=0.0;
        if(ii==0) p=0;
        else p= 1 << (ii-1);
        for(i=1;i<new_num_freqs[c];i++) {
          old_p=new_phase[i-1][c][p];
          new_p=new_phase[i][c][p];
          phase_diff=fabs(new_p-old_p);
          ave_phase_diff+=phase_diff; 
          if (phase_diff >= 360) {
//            printf("Warning: NEW 360 phase:: %d %d : %lf %lf\n",i,p,new_phase[i-1][c][p],new_phase[i][c][p]); 
            if( (new_p - old_p) >=  360 )  new_phase[i][c][p] -=360.0;
            if( (new_p - old_p) <= -360 )  new_phase[i][c][p] +=360.0;
            jump_adjust=1;
            adjusted=1;
            break;
          }
          if(phase_diff < min_phase_diff) min_phase_diff=phase_diff;
          if(phase_diff > min_phase_diff) max_phase_diff=phase_diff;
        }
        if(!jump_adjust) {
          ave_phase_diff=ave_phase_diff/(double)(new_num_freqs[c]-1.0);
          if(adjusted) printf(" *");
          else printf("  ");
          printf("%d New Pairwise Phase Diff: %lf %lf : %lf\n",p,min_phase_diff,max_phase_diff,ave_phase_diff);
        }
      }  
    }

    printf(" Check Phase Bit differences\n");
    for(ii=0;ii<=0;ii++) {
      max_phase_diff=-1E6;
      if(ii==0) p=0;
      else p= 1 << (ii-1);
      old_offset=old_phase[0][c][p];
      new_offset=new_phase[0][c][p];
      for(i=0;i<new_num_freqs[c];i++) {
        old_p=old_phase[i][c][p]-old_offset;
        new_p=new_phase[i][c][p]-new_offset;
        phase_diff=fabs(new_p-old_p);
//          printf("Warning: 360 phase:: %d %d : %lf %lf: %lf %lf : %lf %lf\n",i,p,new_freq[c][i],old_freq[c][i],old_offset,new_offset,old_phase[i][c][p],new_phase[i][c][p]); 
//        if (phase_diff >= 360) {
//          printf("Warning: 360 phase:: %d %d : %lf %lf : %lf %lf\n",i,p,old_offset,new_offset,old_phase[i][c][p],new_phase[i][c][p]); 
//        }
        if(phase_diff > max_phase_diff) {
          max_phase_diff=phase_diff;
          index=i;
          code=p;
        }
      }
      printf("  Highest Phase Diff: %lf :  %lf %lf \n",max_phase_diff,old_offset,new_offset);
    }

    printf(" Check New Bit Time Delay\n");
    for(ii=0;ii<=13;ii++) {
        if(ii==0) p=0;
        else p= 1 << (ii-1);
        old_p=new_phase[0][c][p];
        new_p=new_phase[new_num_freqs[c]-1][c][p];
        phase_diff=fabs(new_p-old_p);
        if (p==0) { 
          new_offset=phase_diff;
          phase_diff=fabs(phase_diff)/fabs(new_freq[c][new_num_freqs[c]-1]-new_freq[c][0]);

        } else {
          phase_diff=fabs(phase_diff-new_offset)/fabs(new_freq[c][new_num_freqs[c]-1]-new_freq[c][0]);
        }
        phase_diff=phase_diff/360.0*1E9;
        printf("%d New Time Delay [ns]:  %e %e\n",p,phase_diff,expected_timedelays[ii]);
    }
  } // end card loop


  exit(0);
}
