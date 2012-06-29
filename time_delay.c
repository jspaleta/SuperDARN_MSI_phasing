#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <gsl/gsl_fit.h>

#define MIN_CARD 0 
#define MAX_CARD 19 
#define MAX_CARDS 20 
#define MAX_FREQS 1201 
#define MAX_PHASES 8192

int32_t quick_flag=1;
int32_t fatal_error=1;
int32_t verbose=0;
FILE *calfile=NULL;
FILE *timedelayfile=NULL;
struct timeval t0,t1,t2,t3;
unsigned long elapsed;
double expected_timedelays[13]={0.25,0.45,0.8,1.5,2.75,5.0,8.0,15.0,25.0,45.0,80.0,140.0,250.0};
double pwr_threshold=-21.0;

double expected_timedelay(int32_t delaycode) {
  int32_t bit,i,code;
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

int32_t main()
{

  int32_t error_flag=0,num_phasecodes,num_cards,num_freqs,active;
  int32_t i,b,c,ii,cc,count,index,summary_freqs;
  int32_t f,d,p,o;
  int32_t lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  double lowest_pwr_mag=1E10; // freq,card,phasecode
  int32_t highest_time_delay_card[MAX_FREQS]; // freq
  double highest_time_delay[MAX_FREQS];
  double *freq,*phase[MAX_FREQS],*pwr_mag[MAX_FREQS],*timedelay[MAX_FREQS],stdev_timedelay[MAX_PHASES],ave_timedelay[MAX_PHASES];    
  double ave_delay0,stdev_delay0,delay0[MAX_FREQS],mag0[MAX_FREQS];    
  double T_diff,T_360,expected,expected_sum[MAX_FREQS],measured_sum[MAX_FREQS],atten_sum[MAX_FREQS];
  double max_timedelay_diff,max_atten_diff,ave_timedelay_diff,ave_atten_diff,var_atten_diff,var_timedelay_diff;
  double min_timedelay_diff;
  double  ave_delay_bit1, stdev_delay_bit1,ave_atten_bit1,stdev_atten_bit1;
  double  ave_delay_bit2, stdev_delay_bit2,ave_atten_bit2,stdev_atten_bit2;
  double  ave_delay_bit3, stdev_delay_bit3,ave_atten_bit3,stdev_atten_bit3;
  int32_t basecode,lowcode,refill,min_timedelay_index,max_timedelay_index,max_atten_index;
  char filename[120];
  int32_t errflag=0;
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

  for(i=0;i<MAX_FREQS;i++) {
    highest_time_delay_card[i]=-1;
    highest_time_delay[i]=-1000;
  }
  if (verbose > 1 ) fprintf(stdout,"Nulling arrays\n");
  active=0;
  freq=NULL;
  num_freqs=0;
  for(i=0;i<MAX_FREQS;i++) {
      phase[i]=NULL;
      pwr_mag[i]=NULL;
      timedelay[i]=NULL;
  }

  for(c=MIN_CARD;c<=MAX_CARD;c++) {
    sprintf(filename,"%s/phasing_cal_%s_%02d.dat",dirstub,radar_name,c);
    if (verbose > -1 ) fprintf(stdout,"Opening: %s\n",filename); 
    calfile=fopen(filename,"r");
    //if (verbose > 1 ) fprintf(stdout,"Opening: %p %s\n",calfile,filename); 
    if (calfile!=NULL) {
      errflag=0;
      active=1;
      fread(&num_phasecodes,sizeof(int32_t),1,calfile);
      if (verbose > 1 ) fprintf(stdout,"PhaseCodes: %d\n",num_phasecodes);
      fread(&num_cards,sizeof(int32_t),1,calfile);
      if (verbose > 1 ) fprintf(stdout,"Cards: %d\n",num_cards);
      fread(&num_freqs,sizeof(int32_t),1,calfile);
      if (verbose > 1 ) fprintf(stdout,"Freqs: %d\n",num_freqs);
      if (num_freqs>MAX_FREQS) {
        fprintf(stderr,"Too many stored frequencies...up the MAX_FREQS define! %d\n",num_freqs);
        exit(0);
      }
      if (verbose > 1 ) fprintf(stdout,"Allocating arrays\n");
      if (freq!=NULL) free(freq);
      freq=calloc(num_freqs,sizeof(double));
      for(i=0;i<num_freqs;i++) {
        if (verbose > 1 ) fprintf(stdout,"Allocating freq arrays %d\n",i);
        if (phase[i]!=NULL) free(phase[i]);
        phase[i]=calloc(num_phasecodes,sizeof(double));
        if (pwr_mag[i]!=NULL) free(pwr_mag[i]);
        pwr_mag[i]=calloc(num_phasecodes,sizeof(double));
        if (timedelay[i]!=NULL) free(timedelay[i]);
        timedelay[i]=calloc(num_phasecodes,sizeof(double));
      }


      if (verbose > 1 ) fprintf(stdout,"Reading frequency array\n");

      count=fread(freq,sizeof(double),num_freqs,calfile);
      //printf("Freqs: %lf %lf\n",freq[0],freq[200]);
      if (verbose > 1 )fprintf(stdout,"%d %d\n",num_freqs,count);
      count=1;
      if (verbose > 1 ) fprintf(stdout,"Reading in data\n");
      while(count>0) {
          count=fread(&ii,sizeof(int32_t),1,calfile);
          if (count==0) {
            break;
          }
          count=fread(phase[ii],sizeof(double),num_phasecodes,calfile);
          if (verbose > 1) fprintf(stdout,"Freq index: %d Phase Count: %d\n",ii,count);
          count=fread(pwr_mag[ii],sizeof(double),num_phasecodes,calfile);
          if (verbose > 1) fprintf(stdout,"Freq index: %d Pwr-mag Count: %d\n",ii,count);
        i=0;
        while(i<8192) {
          if(pwr_mag[ii][i] < -10 ) {
            printf("%d %lf\n",i,pwr_mag[ii][i]);
            error_flag=1;
          }
          i=i+1 ;
        }
        if(error_flag) exit(0);
         if (count==0) {
            break;
          }
      }
      if (count==0) {
        if (feof(calfile)) if (verbose > 1 ) fprintf(stdout,"End of File!\n");
      }
      fclose(calfile);
      if (verbose > -1 ) fprintf(stdout,"Processing Phase Information for Card: %d\n",c);
//      o_0=20;
//      last_collect=0; 
//      current_collect=0; 

      for (p=0;p<num_phasecodes;p++) {
//        printf("%d %d %d\n",p,num_phasecodes,quick_flag);
/*
*  Build the timedelays arrays from average of slope 
*/
        ave_timedelay[p]=0; 
        if(p==0) {
          ave_delay0=0; 
          stdev_delay0=0; 
        } 
        refill=1;
        if(p==0) refill=0;
        if(p==(num_phasecodes-1)) refill=0;
        if(p==1) refill=0;
        if(p==2) refill=0;
        if(p==4) refill=0;
        if((p % 8)==0) refill=0;
        for (i=0;i<num_freqs;i++) {
          if(i==0) timedelay[0][p]=phase_to_timedelay(phase[num_freqs-1][p]-phase[0][p],freq[num_freqs-1]-freq[0]);
          else timedelay[i][p]=phase_to_timedelay(phase[i][p]-phase[0][p],freq[i]-freq[0]);
          if(p==0) { 
            mag0[i]=pwr_mag[i][0];
            delay0[i]=timedelay[i][0];
            ave_delay0+=delay0[i];
          }
          //if(p==(num_phasecodes-1)) printf("P: %d F: %d phase: %lf Tdelay: %lf\n",p,i,phase[i][p],timedelay[i][p]);
          if(quick_flag) {
            if(p==(num_phasecodes-1)) refill=0;
            if(refill) {
              basecode=p-( p % 8 ); 
              lowcode=p % 8 ;
              timedelay[i][p]=timedelay[i][basecode];
              //printf("Refill: Basecode: %d Lowcode: %d Basedelay: %lf\n",basecode,lowcode,timedelay[i][basecode]);
              pwr_mag[i][p]=pwr_mag[i][basecode];
              for(b=0;b<3;b++) {
                index=(int32_t)pow(2,b);
                if((lowcode & index) == index) {
                  timedelay[i][p]+=timedelay[i][index]-delay0[i];
                  pwr_mag[i][p]+=pwr_mag[i][index]-mag0[i];
                }
              }
            } else {
              //if(i==0)  printf("Quick Cal  No Refill: %d\n",p);
            }
          } // end quick cal  
          ave_timedelay[p]+=timedelay[i][p];
        }  //freq loop 
        ave_timedelay[p]/=num_freqs;
        if(p==0) {
          ave_delay0/=(double)num_freqs;
        }
        stdev_timedelay[p]=0;
        for(i=0;i<num_freqs;i++) {
          stdev_timedelay[p]+=pow((timedelay[i][p]-ave_timedelay[p] ),2.0 );
          if(p==0) { 
            stdev_delay0+=pow((delay0[i]-ave_delay0 ),2.0 );
          }
        }  //freq loop
        stdev_timedelay[p]=sqrt(stdev_timedelay[p]/(double)num_freqs);
        if(p==0) {
          stdev_delay0=sqrt(stdev_delay0/(double)num_freqs);
        }
        //printf("%d: Refill: %d Ave timedelay: %lf Stdev %lf\n",p, refill,ave_timedelay[p]-ave_timedelay[0],stdev_timedelay[p]);
      }  //phasecode loop
      for(i=0;i<num_freqs;i++) {
        if (timedelay[i][0]>highest_time_delay[i]) {
            highest_time_delay_card[i]=c;
            highest_time_delay[i]=timedelay[i][0];
        }        
        for (b=0;b<num_phasecodes;b++) {
          if (pwr_mag[i][b]<lowest_pwr_mag) {
            lowest_pwr_mag_index[0]=i;
            lowest_pwr_mag_index[1]=c;
            lowest_pwr_mag_index[2]=b;
            lowest_pwr_mag=pwr_mag[i][b];
          }        
        }
      }
     max_timedelay_diff=0.0; 
     min_timedelay_diff=1E13; 
     max_atten_diff=0.0; 
     ave_timedelay_diff=0.0; 
     ave_atten_diff=0.0; 
     ave_delay_bit1=0.0;
     ave_delay_bit2=0.0;
     ave_delay_bit3=0.0;
     ave_atten_bit1=0.0;
     ave_atten_bit2=0.0;
     ave_atten_bit3=0.0;
     fprintf(stdout,"Common Time Delay Offset(ns): %lf %lf\n",ave_timedelay[0],ave_delay0);
     fprintf(stdout,"Common Time Delay StDev(ns): %lf %lf\n",stdev_timedelay[0],stdev_delay0);
     for(i=0;i<num_freqs;i++) {
      expected_sum[i]=0.0;
      measured_sum[i]=0.0;
      atten_sum[i]=0.0;
      for(b=0;b<13;b++) {
        index=(int32_t)pow(2,b);
        expected_sum[i]+=expected_timedelays[b];
        measured_sum[i]+=timedelay[i][index]-ave_delay0; 
        atten_sum[i]+=pwr_mag[i][index]-mag0[i]; 
      }
      if(fabs(measured_sum[i]-(timedelay[i][8191]-ave_delay0)) > max_timedelay_diff) {
        max_timedelay_diff=fabs(measured_sum[i]-(timedelay[i][8191]-ave_delay0));
        max_timedelay_index=i;
      }
      ave_timedelay_diff+=fabs(measured_sum[i]-(timedelay[i][8191]-ave_delay0));

      if(fabs(measured_sum[i]-(timedelay[i][8191]-ave_delay0)) < min_timedelay_diff) {
        min_timedelay_diff=fabs(measured_sum[i]-(timedelay[i][8191]-ave_delay0));
        min_timedelay_index=i;
      }

      ave_timedelay_diff+=measured_sum[i]-(timedelay[i][8191]-ave_delay0);
      ave_delay_bit1+=(timedelay[i][1]-ave_delay0);
      ave_delay_bit2+=(timedelay[i][2]-ave_delay0);
      ave_delay_bit3+=(timedelay[i][4]-ave_delay0);
      if(fabs(atten_sum[i]-(pwr_mag[i][8191]-mag0[i])) > max_atten_diff ) {
        max_atten_diff=fabs(atten_sum[i]-(pwr_mag[i][8191]-mag0[i]));
        max_atten_index=i;
      }
      ave_atten_diff+=atten_sum[i]-(pwr_mag[i][8191]-mag0[i]);
      ave_atten_bit1+=-(pwr_mag[i][1]-mag0[i]);
      ave_atten_bit2+=-(pwr_mag[i][2]-mag0[i]);
      ave_atten_bit3+=-(pwr_mag[i][4]-mag0[i]);
     }
     ave_timedelay_diff=ave_timedelay_diff/(double)num_freqs;
     ave_delay_bit1=ave_delay_bit1/(double)num_freqs;
     ave_delay_bit2=ave_delay_bit2/(double)num_freqs;
     ave_delay_bit3=ave_delay_bit3/(double)num_freqs;
     ave_atten_diff=ave_atten_diff/(double)num_freqs;
     ave_atten_bit1=ave_atten_bit1/(double)num_freqs;
     ave_atten_bit2=ave_atten_bit2/(double)num_freqs;
     ave_atten_bit3=ave_atten_bit3/(double)num_freqs;
     var_timedelay_diff=0.0; 
     var_atten_diff=0.0; 
     stdev_delay_bit1=0.0;
     stdev_delay_bit2=0.0;
     stdev_delay_bit3=0.0;
     stdev_atten_bit1=0.0;
     stdev_atten_bit2=0.0;
     stdev_atten_bit3=0.0;
     for(i=0;i<num_freqs;i++) {
      var_timedelay_diff+=pow(((measured_sum[i]-(timedelay[i][8191]))-ave_timedelay_diff),2.0);
      stdev_delay_bit1+=pow( ( (timedelay[i][1]-ave_delay0 )-ave_delay_bit1 ),2.0 );
      stdev_delay_bit2+=pow( ( (timedelay[i][2]-ave_delay0 )-ave_delay_bit2 ),2.0 );
      stdev_delay_bit3+=pow( ( (timedelay[i][4]-ave_delay0 )-ave_delay_bit3 ),2.0 );
      var_atten_diff+=pow(((atten_sum[i]-(pwr_mag[i][8191]-mag0[i]))-ave_atten_diff),2.0);
      stdev_atten_bit1+=pow( ( (pwr_mag[i][1]-mag0[i] )-ave_atten_bit1 ),2.0 );
      stdev_atten_bit2+=pow( ( (pwr_mag[i][2]-mag0[i] )-ave_atten_bit2 ),2.0 );
      stdev_atten_bit3+=pow( ( (pwr_mag[i][4]-mag0[i] )-ave_atten_bit3 ),2.0 );
     }
     var_timedelay_diff=sqrt(var_timedelay_diff/(double)num_freqs);
     stdev_delay_bit1=sqrt(stdev_delay_bit1/(double)num_freqs);
     stdev_delay_bit2=sqrt(stdev_delay_bit2/(double)num_freqs);
     stdev_delay_bit3=sqrt(stdev_delay_bit3/(double)num_freqs);
     var_atten_diff=sqrt(var_atten_diff/(double)num_freqs);
     stdev_atten_bit1=sqrt(stdev_atten_bit1/(double)num_freqs);
     stdev_atten_bit2=sqrt(stdev_atten_bit2/(double)num_freqs);
     stdev_atten_bit3=sqrt(stdev_atten_bit3/(double)num_freqs);
     printf("Ave Timedelay Diff: %lf (ns) StDev: %lf (ns)\n--------------\n",ave_timedelay_diff,var_timedelay_diff);
     printf("Max Timedelay Diff: %lf (ns) at freq: %lf\n",max_timedelay_diff,freq[max_timedelay_index]);
     i=max_timedelay_index;
     fprintf(stdout,"Freq: %lf Code: %4d :: Expected(ns): %8.3lf Measured(ns): %8.3lf Ave(ns): %8.3lf : Corrected(ns): %8.3lf Mag: %8.3lf\n",
      freq[i],0, 0.0,timedelay[i][0],ave_timedelay[0],ave_timedelay[0]-ave_delay0,pwr_mag[i][0]);
     for(b=0;b<13;b++) {
      index=(int32_t)pow(2,b);
        fprintf(stdout,"Freq: %lf Code: %4d :: Expected(ns): %8.3lf Measured(ns): %8.3lf Ave(ns): %8.3lf : Corrected(ns): %8.3lf Mag: %8.3lf\n",
          freq[i],index, expected_timedelays[b],timedelay[i][index],ave_timedelay[index],ave_timedelay[index]-ave_delay0,pwr_mag[i][index]);
        if ( b > 0 ) {
          fprintf(stdout,"Freq: %lf Code: %4d :: Expected(ns): %8.3lf Measured(ns): %8.3lf Ave(ns): %8.3lf : Corrected(ns): %8.3lf Mag: %8.3lf\n",
            freq[i],index+1, expected_timedelays[0]+expected_timedelays[b],timedelay[i][index+1],ave_timedelay[index+1],ave_timedelay[index+1]-ave_delay0,pwr_mag[i][index+1]);
        }
     }
     fprintf(stdout,"Freq: %lf Code: %4d :: Expected(ns): %8.3lf Measured(ns): %8.3lf Ave(ns): %8.3lf : Corrected(ns): %8.3lf Mag: %8.3lf\n",
      freq[i],8191, expected_sum[i],timedelay[i][8191],ave_timedelay[8191],ave_timedelay[8191]-ave_delay0,pwr_mag[i][8191]);
     printf("---------\n");
     printf("Min Timedelay Diff: %lf (ns) at freq: %lf\n",min_timedelay_diff,freq[min_timedelay_index]);
     i=min_timedelay_index;
     for(b=0;b<13;b++) {
      index=(int32_t)pow(2,b);
        fprintf(stdout,"Freq: %lf Code: %4d :: Expected(ns): %8.3lf Measured(ns): %8.3lf Ave(ns): %8.3lf : Corrected(ns): %8.3lf Mag: %8.3lf\n",
          freq[i],index, expected_timedelays[b],timedelay[i][index],ave_timedelay[index],ave_timedelay[index]-ave_delay0,pwr_mag[i][index]);
        if ( b > 0 ) {
          fprintf(stdout,"Freq: %lf Code: %4d :: Expected(ns): %8.3lf Measured(ns): %8.3lf Ave(ns): %8.3lf : Corrected(ns): %8.3lf Mag: %8.3lf\n",
            freq[i],index+1, expected_timedelays[0]+expected_timedelays[b],timedelay[i][index+1],ave_timedelay[index+1],ave_timedelay[index+1]-ave_delay0,pwr_mag[i][index+1]);
        }
     }
     fprintf(stdout,"Freq: %lf Code: %4d :: Expected(ns): %8.3lf Measured(ns): %8.3lf Ave(ns): %8.3lf : Corrected(ns): %8.3lf Mag: %8.3lf\n",
          freq[i],8191, expected_sum[i],timedelay[i][8191],ave_timedelay[8191],ave_timedelay[8191]-ave_delay0,pwr_mag[i][8191]);
     printf("---------\n");


      printf("Ave Atten Diff: %lf (db) StDev: %lf (db)\n--------------\n",ave_atten_diff,var_atten_diff);
      printf("Max Atten Diff: %lf (db) at freq: %lf\n",max_atten_diff,freq[max_atten_index]);
      i=max_atten_index;
      for(b=0;b<13;b++) {
        index=(int32_t)pow(2,b);
      }
      printf("Ave Timedelay Bit1: %lf (ns) StDev: %lf (ns)\n",ave_delay_bit1,stdev_delay_bit1);
      printf("Ave Timedelay Bit2: %lf (ns) StDev: %lf (ns)\n",ave_delay_bit2,stdev_delay_bit2);
      printf("Ave Timedelay Bit3: %lf (ns) StDev: %lf (ns)\n",ave_delay_bit3,stdev_delay_bit3);

      printf("Ave Atten Bit1: %lf (db) StDev: %lf (db)\n",ave_atten_bit1,stdev_atten_bit1);
      printf("Ave Atten Bit2: %lf (db) StDev: %lf (db)\n",ave_atten_bit2,stdev_atten_bit2);
      printf("Ave Atten Bit3: %lf (db) StDev: %lf (db)\n",ave_atten_bit3,stdev_atten_bit3);

      sprintf(filename,"%s/timedelay_cal_%s_%02d.dat",dirstub,radar_name,c);
      timedelayfile=fopen(filename,"w+");
      if (verbose > 1 ) fprintf(stdout,"Creating: %p %s\n",timedelayfile,filename); 
      count=num_phasecodes;
      fwrite(&count,sizeof(int32_t),1,timedelayfile);
      count=MAX_CARDS;
      fwrite(&count,sizeof(int32_t),1,timedelayfile);
      count=num_freqs;
      fwrite(&count,sizeof(int32_t),1,timedelayfile);
      fwrite(&ave_delay0,sizeof(double),1,timedelayfile);
      fwrite(&stdev_delay0,sizeof(double),1,timedelayfile);
      count=0;
      fwrite(freq,sizeof(double),num_freqs,timedelayfile);
      count=fwrite(ave_timedelay,sizeof(double),num_phasecodes,timedelayfile);
      for(i=0;i<num_freqs;i++) {
        if (verbose > 1) fprintf(stdout,"Freq %lf:  Time_0:%lf Time_8191: %lf\n",freq[i],timedelay[i][0],timedelay[i][8191]);
        fwrite(&i,sizeof(int32_t),1,timedelayfile);
        count=fwrite(timedelay[i],sizeof(double),num_phasecodes,timedelayfile);
        count=fwrite(pwr_mag[i],sizeof(double),num_phasecodes,timedelayfile);
      }
      if (verbose > 1 ) fprintf(stdout,"Closing timedelay File\n");
      fclose(timedelayfile);
      if(freq!=NULL) free(freq);
      freq=NULL;
      for(i=0;i<MAX_FREQS;i++) {
        if(phase[i]!=NULL) free(phase[i]);
        if(timedelay[i]!=NULL) free(timedelay[i]);
        if(pwr_mag[i]!=NULL) free(pwr_mag[i]);
        phase[i]=NULL;  
        pwr_mag[i]=NULL;
        timedelay[i]=NULL;
      }
      fflush(stdout);
      fflush(stderr);
    } else {
      printf("calfile not opening\n");
    }
  }  // End card Loop
//Summary stats
  printf("-------------------\n");
  fprintf(stdout,"::: Summary Stats for All Cards :::\n");
  if(verbose > 1 ) {
    for(i=0;i<MAX_FREQS;i++) {
      if (highest_time_delay_card[i] >=0) {
        fprintf(stdout,"Highest 0-Time Delay: %lf\n",highest_time_delay[i]);
        fprintf(stdout,"  freq: %d card: %d\n",
          i,highest_time_delay_card[i]);
      }
    } //End of Freq Loop
  }
  fprintf(stdout,"Lowest Mag: %lf\n",lowest_pwr_mag);
  fprintf(stdout,"Lowest Mag Index:: freq: %d card: %d phasecode: %d\n",
     lowest_pwr_mag_index[0],lowest_pwr_mag_index[1],lowest_pwr_mag_index[2]);
} // end of main

