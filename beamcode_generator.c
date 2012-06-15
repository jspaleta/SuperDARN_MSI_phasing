#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#define MAX_CARDS 20 
#define NUM_CARDS 20 
#define MAX_FREQS 201 
//#define MAX_FREQS 1201 
#define MAX_PHASES 8192
#define MAX_ANGLES 32 
#define NUM_ANGLES 22 
#define USE_MEASURED_ATTENS 1 
int32_t verbose=1;
char radar_name[80]="adw";
char dirstub[160]="/home/jspaleta/data/calibrations/adw/";
FILE *timedelayfile=NULL;
FILE *attenfile=NULL;
FILE *summaryfile=NULL;
FILE *beamcodefile=NULL;
struct timeval t0,t1,t2,t3;
unsigned long elapsed;
double angles[MAX_ANGLES];
int32_t antenna_best_code[MAX_CARDS];
int32_t attenfile_exists=0;
double spacing=12.8016; //meters : MSI 42 feet == 12.8016 meters 
double bm_sep=3.24;
//double middle=11.5; //(NUM_ANGLES-1)/2
double middle=10.5; //(NUM_ANGLES-1)/2

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
  return needed; //nanoseconds
}

int32_t main()
{
  double *freq,*pwr_mag[MAX_FREQS],*timedelay[MAX_FREQS];    
  double *atten_freq,*atten_pwr_mag[MAX_FREQS],*atten_phase[MAX_FREQS];    
  double ave_delay0,stdev_delay0;
  int32_t *best_phasecode;
  int32_t *best_attencode[MAX_FREQS];
  double best_pwr_mag,best_timedelay;
  double *ave_timedelay;
  double *ave_pwr_mag;
  double *mid_pwr_mag;
  double *min_pwr_mag;
  double *max_pwr_mag;
  int32_t *ave_attencode;
  int32_t *mid_attencode;
  int32_t *min_attencode;
  int32_t *max_attencode;
  double *best_atten_value[MAX_FREQS];
  int32_t b,c,i,ii,j,p,a,count,data_count;
  int32_t summary_freqs,summary_phases;
  int32_t num_freqs,num_phasecodes,num_cards,num_angles;
  int32_t num_atten_freqs,num_attencodes,num_atten_cards;
  char filename[120];
  int32_t highest_time0_card; // card with highest time0 delay
  double highest_time0_value; // highest time0 delay in ns
  int32_t lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  double lowest_pwr_mag=1E10,needed_atten=0.0; // freq,card,phasecode
  double time_needed,angle,difference;
  double atten,atten_steps[6];
  int32_t best_atten_code;

//  printf("Nulling arrays\n");
  freq=NULL;
  atten_freq=NULL;
  num_freqs=0;
        ave_timedelay=NULL;
        ave_pwr_mag=NULL;
        mid_pwr_mag=NULL;
        min_pwr_mag=NULL;
        max_pwr_mag=NULL;
  	ave_attencode=NULL;
  	mid_attencode=NULL;
  	min_attencode=NULL;
  	max_attencode=NULL;
  highest_time0_value=0;
  highest_time0_card=-1;
  best_phasecode=NULL;
  for(i=0;i<MAX_FREQS;i++) {
        timedelay[i]=NULL;
        pwr_mag[i]=NULL;
        atten_pwr_mag[i]=NULL;
        atten_phase[i]=NULL;    
  	best_attencode[i]=NULL;
  	best_atten_value[i]=NULL;
  }
  for(i=0;i<6;i++) {
    atten_steps[i]=0.5*pow(2,i);
  }

  for(i=0;i<NUM_ANGLES;i++) {
    angles[i]=(i-middle)*bm_sep;
//    printf("%d %lf\n",i,angles[i]);
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
  if(lowest_pwr_mag > 0.0 ) lowest_pwr_mag=0.0;
  printf("Final: lowest_pwr_mag %lf\n",lowest_pwr_mag);
  for(c=0;c<20;c++) {
    sprintf(filename,"%s/phasing_cal_%s_%02d.att",dirstub,radar_name,c);
    attenfile=fopen(filename,"r");
    printf("%p %s\n",attenfile,filename); 
    if (attenfile!=NULL) {
      attenfile_exists=1;
      fread(&num_attencodes,sizeof(int32_t),1,attenfile);
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
      }
      count=fread(atten_freq,sizeof(double),num_atten_freqs,attenfile);
//      if (verbose > 0 )fprintf(stdout,"%d %d\n",num_atten_freqs,count);
      count=1;
//      if (verbose > 0 ) fprintf(stdout,"Reading in data\n");
      data_count=0;
      while(count>0) {
          count=fread(&ii,sizeof(int32_t),1,attenfile);
          //if (verbose > 0) fprintf(stdout,"Freq index: %d Phase Count: %d\n",ii,count);
          if (count==0) {
            break;
          }
          count=fread(atten_phase[ii],sizeof(double),num_attencodes,attenfile);
          //if (verbose > 0) fprintf(stdout,"Phase Count: %d\n",count);
          count=fread(atten_pwr_mag[ii],sizeof(double),num_attencodes,attenfile);
//          if (verbose > 1) fprintf(stdout,"Freq index: %d Pwr-mag Count: %d\n",ii,count);
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
      //printf("data_count:: %d %d\n",num_atten_freqs,data_count);
    } else {
      attenfile_exists=0;
    }
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

        if (ave_pwr_mag!=NULL) free(ave_pwr_mag);
        ave_pwr_mag=calloc(MAX_ANGLES,sizeof(double));
        if (mid_pwr_mag!=NULL) free(mid_pwr_mag);
        mid_pwr_mag=calloc(MAX_ANGLES,sizeof(double));
        if (max_pwr_mag!=NULL) free(max_pwr_mag);
        max_pwr_mag=calloc(MAX_ANGLES,sizeof(double));
        if (min_pwr_mag!=NULL) free(min_pwr_mag);
        min_pwr_mag=calloc(MAX_ANGLES,sizeof(double));

        if (min_attencode!=NULL) free(min_attencode);
        min_attencode=calloc(MAX_ANGLES,sizeof(int32_t));
        if (ave_attencode!=NULL) free(ave_attencode);
        ave_attencode=calloc(MAX_ANGLES,sizeof(int32_t));
        if (mid_attencode!=NULL) free(mid_attencode);
        mid_attencode=calloc(MAX_ANGLES,sizeof(int32_t));
        if (max_attencode!=NULL) free(max_attencode);
        max_attencode=calloc(MAX_ANGLES,sizeof(int32_t));

        if (best_phasecode!=NULL) free(best_phasecode);
        best_phasecode=calloc(MAX_ANGLES,sizeof(int32_t));
      for(i=0;i<num_freqs;i++) {
        freq=calloc(num_freqs,sizeof(double));
        if (timedelay[i]!=NULL) free(timedelay[i]);
        timedelay[i]=calloc(num_phasecodes,sizeof(double));
        if (pwr_mag[i]!=NULL) free(pwr_mag[i]);
        pwr_mag[i]=calloc(num_phasecodes,sizeof(double));

        if (best_attencode[i]!=NULL) free(best_attencode[i]);
        best_attencode[i]=calloc(MAX_ANGLES,sizeof(int32_t));
        if (best_atten_value[i]!=NULL) free(best_atten_value[i]);
        best_atten_value[i]=calloc(MAX_ANGLES,sizeof(double));
      }


//      printf("Reading frequency array\n");

      count=fread(freq,sizeof(double),num_freqs,timedelayfile);
      printf("%d %d\n",num_freqs,count);
      count=fread(ave_timedelay,sizeof(double),num_phasecodes,timedelayfile);
      printf("Ave tdelay: %d\n",count);
      count=1;
      printf("Reading in data\n");
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
        if (feof(timedelayfile)) printf("End of File!\n");
      }
      fclose(timedelayfile);
/*
      for(i=0;i<num_phasecodes;i++) {
         printf("%lf : %d : %lf\n",freq[0],i,pwr_mag[0][i]);
      }
*/
      sprintf(filename,"%s/beamcodes_cal_%s_%d.dat",dirstub,radar_name,c);
      beamcodefile=fopen(filename,"w+");
      printf("%p %s\n",beamcodefile,filename); 
      if(USE_MEASURED_ATTENS && attenfile_exists) printf("Using measured Attenuations\n"); 
      if (beamcodefile!=NULL) {
        fwrite(&num_freqs,sizeof(int32_t),1,beamcodefile);
        fwrite(freq,sizeof(double),num_freqs,beamcodefile);

        num_angles=NUM_ANGLES;
        fwrite(&num_angles,sizeof(int32_t),1,beamcodefile);
        fwrite(angles,sizeof(double),num_angles,beamcodefile);
        for(i=0;i<num_freqs;i++) {
          for(b=0;b<num_angles;b++) {
            angle=angles[b];
            time_needed=timedelay_needed(angle,spacing,c)+highest_time0_value; 
            difference=1E13;
            best_phasecode[b]=-1;
            for(p=0;p<num_phasecodes;p++) {
              if(fabs(time_needed-ave_timedelay[p])<difference) {
                best_phasecode[b]=p;
                difference=fabs(time_needed-ave_timedelay[p]);
              }
            } //phase code looop
            p=best_phasecode[b];
//            if(b==22) printf("Freq: %8.3lf p: %d T0: %lf At: %lf Tdelay: %8.3lf\n",
//                        freq[i],p,highest_time0_value,timedelay_needed(angle,spacing,c),ave_timedelay[p]);
            needed_atten=pwr_mag[i][best_phasecode[b]]-lowest_pwr_mag;
//            if(b==22) {
//              printf("Freq: %lf Angle : %lf dT: %lf  Best_B: %d  Pwr_mag: %lf\n",freq[i],angles[b],difference,best_phasecode[b],
//                pwr_mag[i][best_phasecode[b]]);
//            }
            best_attencode[i][b]=0;
            best_atten_value[i][b]=0.0;
            for(ii=0;ii<64;ii++) {
              atten=0;
              if(USE_MEASURED_ATTENS && attenfile_exists) {
                atten=-atten_pwr_mag[i][ii]+atten_pwr_mag[i][0];
              } else {
                for(j=0;j<6;j++) {
                  if((ii & (int32_t)pow(2,j))==pow(2,j)) {
                    atten+=atten_steps[j];
                  }
                }
              } 
              if(fabs(atten-needed_atten) < fabs(best_atten_value[i][b]-needed_atten)) {
                  best_attencode[i][b]=ii;
                  best_atten_value[i][b]=atten;
              }
            } // end atten bit loop
/*
            if(i==40) { 
              printf("C: %2d F: %7.1lf Ang: %6.2lf lo_T: %6.2lf N_T: %6.2lf 0_T: %6.2lf :: Code: %4d :: lo_P: %6.2lf P: %6.2lf N_A: %6.2lf BA_val: %6.2lf BA_code: %3d\n",
                     c,freq[i],angle,highest_time0_value,time_needed,ave_timedelay[0],best_phasecode[b]
                     ,lowest_pwr_mag,pwr_mag[i][best_phasecode[b]],
                     needed_atten,best_atten_value[i][b],best_attencode[i][b]);  
            }
*/
          } //angle for loop
          fwrite(best_attencode[i],sizeof(int32_t),num_angles,beamcodefile);
        } // frequency loop
        fwrite(best_phasecode,sizeof(int32_t),num_angles,beamcodefile);

//find min,max,mid, average
        for(b=0;b<num_angles;b++) {
          angle=angles[b];
          ave_pwr_mag[b]=0.0;
          ave_attencode[b]=0;
          min_pwr_mag[b]=1E13;
          max_pwr_mag[b]=-1E13;
          for(i=0;i<num_freqs;i++) {
                  p=best_phasecode[b]; 
                  a=best_attencode[i][b]; 

                  best_pwr_mag=pwr_mag[i][p];

                  ave_pwr_mag[b]+=best_pwr_mag;
                  ave_attencode[b]+=a;
                  if(i==(int32_t)num_freqs/2) {
                    mid_pwr_mag[b]=best_pwr_mag; 
                    mid_attencode[b]=a;
                  }
                  if(best_pwr_mag > max_pwr_mag[b]) {
                    max_pwr_mag[b]=best_pwr_mag;
                    max_attencode[b]=a;
                  }
                  if(best_pwr_mag < min_pwr_mag[b]){
                    min_pwr_mag[b]=best_pwr_mag;
                    min_attencode[b]=a;
                  }
          } // end freq for loop
          ave_pwr_mag[b]/=num_freqs;
          ave_attencode[b]/=num_freqs;
          printf("Angle: %6.2lf  Pcode: %5d Max pwr_mag: %8.3lf acode: %6d\n",angle,best_phasecode[b],max_pwr_mag[b],max_attencode[b]);
        } // end angle for loop 
        fwrite(max_pwr_mag,sizeof(double),num_angles,beamcodefile);
        fwrite(max_attencode,sizeof(int32_t),num_angles,beamcodefile);
        fclose(beamcodefile);
      }  //beamcode file if
    }  //timedelay file if

// free card arrays
  } //card loop
} //main
