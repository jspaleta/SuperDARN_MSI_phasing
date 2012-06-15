#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define MAX_CARDS 20
#define MAX_FREQS 1500
#define MAX_PHASES 8192

int verbose=1;
char radar_name[80]="kod";
FILE *calfile=NULL;
struct timeval t0,t1,t2,t3;
unsigned long elapsed;
double timedelays[13]={0.25,0.45,0.8,1.5,2.75,5.0,8.0,15.0,25.0,45.0,80.0,140.0,250.0};
double standard_angles[16]={-30,-20,-10,0,10,20,30,-30,-20,-10,0,10,20,30,40,50};
int antenna_best_code[16];
double antenna_best_time_difference[16];
double antenna_best_time_measured[16];
double antenna_best_time_needed[16];

double spacing=0.1;
double timedelay_needed(double angle,double spacing,int antenna) {
/*
*  angle from broadside (degrees)  spacing in meters
*/
  double deltat=0;
  double needed=0;
  double c=0.299792458; // meters per nanosecond
  deltat=spacing*sin(abs(angle)/180.0*3.14159)/c; //nanoseconds
  if (angle > 0) needed=antenna*deltat;
  if (angle < 0) needed=(15-antenna)*deltat;
  if (needed < 0) {
    printf("Error in Time Needed Calc: %lf %lf\n",needed,deltat);
  }
  return needed;
}

double expected_timedelay(int delaycode) {
  int bit,i,code;
  double timedelay=0;
  code=delaycode;
  for (i=0;i<13;i++) {
    bit=(code & 0x1);
    timedelay+=bit*timedelays[i];
//    printf("    Code: %x Bit: %d Value: %d Delay: %lf\n",code,i,bit,timedelays[i]);
    code=code >> 1;
  }
//  printf("Code: %x Total Time Delay: %lf\n",delaycode,timedelay);
  return timedelay;
}

double phase_to_timedelay(double phase,double freq,double timedelay,double offset,double ratio,int *cycles,double *delta)
{
/*
* phase in degrees  freq in Hz timedelay in ns   ratio (0-1)
*/
/* takes phase in degrees 0 -360 */
  double time_phase=0;
  double initial_time=0;
  double difference=0;
  double max_time=600.0;
  int count=0;
  while(phase <= -360.0) {
            phase+=360.0;
  }
  while(phase > 0.0) {
            phase-=360.0;
  }
  if ((phase > 0) ||(phase <=-360)) {
            printf("Phase Error: %lf %lf\n",freq,phase);
            exit(0);
  }
  time_phase=-phase/360.0/freq*1E9; // nanoseconds
  initial_time=time_phase;
  difference=timedelay+offset-time_phase;
      printf("%d %lf %lf %lf %lf %lf\n",count, time_phase, timedelay,difference,(difference/timedelay),ratio);
  if (timedelay!=0) {
    while(((difference/timedelay) > ratio) && (time_phase < max_time)) {
      time_phase+=1/(freq*1E-9);
      difference=timedelay+offset-time_phase;
      count++;
      printf("%d %lf %lf %lf %lf %lf\n",count, time_phase, timedelay,difference,(difference/timedelay),ratio);
    }
  }
  if( ((difference/timedelay) < 0) && (count > 0)) {
      time_phase-=1/(freq*1E-9);
      difference=timedelay+offset-time_phase;
      count--;
  } 
    printf("%d %lf %lf %lf %lf %lf\n",count, time_phase, timedelay,difference,(difference/timedelay),ratio);
  *cycles=count;
  *delta=1/(freq*1E-9);
  return time_phase;
}
int main()
{
  int num_phasecodes[MAX_CARDS],num_cards[MAX_CARDS],num_freqs[MAX_CARDS],active[MAX_CARDS];
  int i,b,c,ii,cc,count;
  int f,d,p;
  char tmp;
  int lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  double lowest_pwr_mag=1E10; // freq,card,phasecode
  int highest_phase_delay_index[MAX_FREQS][2]; // freq,card,phasecode
  double highest_phase_delay[MAX_FREQS];
  double *freq[MAX_CARDS],*phase[MAX_FREQS][MAX_CARDS],*pwr_mag[MAX_FREQS][MAX_CARDS];    
  double time_expected=0.0,time_measured=0.0,time_difference=0.0,time0_measured=0.0,time_needed=0.0;
  double delta=0.0;
  int cycles=0;
  int angle,ant;
  char filename[120];
  for(i=0;i<MAX_FREQS;i++) {
    for(ii=0;ii<2;ii++) highest_phase_delay_index[i][ii]=-1;
    highest_phase_delay[i]=-1000;
  }
  printf("Nulling arrays\n");
  for(c=0;c<MAX_CARDS;c++) {
    active[c]=0;
    freq[c]=NULL;
    num_freqs[c]=0;
    for(i=0;i<MAX_FREQS;i++) {
        phase[i][c]=NULL;
        pwr_mag[i][c]=NULL;
    }
  }
  for(c=0;c<MAX_CARDS;c++) {
    sprintf(filename,"/tmp/phasing_cal_%s_%d.dat",radar_name,c);
    calfile=fopen(filename,"r");
    printf("%p %s\n",calfile,filename); 
    if (calfile!=NULL) {
      active[c]=1;
      fread(&num_phasecodes[c],sizeof(int),1,calfile);
      printf("PhaseCodes: %d\n",num_phasecodes[c]);
      fread(&num_cards[c],sizeof(int),1,calfile);
      printf("Cards: %d\n",num_cards[c]);
      fread(&num_freqs[c],sizeof(int),1,calfile);
      printf("Freqs: %d\n",num_freqs[c]);
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
      }


      printf("Reading frequency array\n");

      count=fread(freq[c],sizeof(double),num_freqs[c],calfile);
      printf("%d %d\n",num_freqs[c],count);
      count=1;
      printf("Reading in data\n");
      while(count>0) {
          count=fread(&ii,sizeof(int),1,calfile);
          if (count==0) {
            break;
          }
          count=fread(phase[ii][c],sizeof(double),num_phasecodes[c],calfile);
          printf("Freq index: %d Phase Count: %d\n",ii,count);
          count=fread(pwr_mag[ii][c],sizeof(double),num_phasecodes[c],calfile);
          printf("Freq index: %d Pwr-mag Count: %d\n",ii,count);
          if (count==0) {
            break;
          }
      }
      if (count==0) {
        if (feof(calfile)) printf("End of File!\n");
      }
      fclose(calfile);
      for(i=0;i<num_freqs[c];i++) {
        if (phase[i][c][0]>highest_phase_delay[i]) {
            highest_phase_delay_index[i][0]=c;
            highest_phase_delay_index[i][1]=0;
            highest_phase_delay[i]=phase[i][c][0];
        }        
        for (b=0;b<num_phasecodes[c];b++) {
          if (pwr_mag[i][c][b]<lowest_pwr_mag) {
            lowest_pwr_mag_index[0]=i;
            lowest_pwr_mag_index[1]=c;
            lowest_pwr_mag_index[2]=b;
            lowest_pwr_mag=pwr_mag[i][c][b];
          }        
        }
      }
    }
  }
  printf("Highest Phase Delay:\n",lowest_pwr_mag);
  for(i=0;i<MAX_FREQS;i++) {
    printf("  freq: %d card: %d phasecode: %d\n",
     i,highest_phase_delay_index[i][0],highest_phase_delay_index[i][1]);
  } //End of Card Loop
  printf("Lowest Mag: %lf\n",lowest_pwr_mag);
  printf("Lowest Mag Index:: freq: %d card: %d phasecode: %d\n",
     lowest_pwr_mag_index[0],lowest_pwr_mag_index[1],lowest_pwr_mag_index[2]);
  for(c=0;c<MAX_CARDS;c++) {
    if(freq[c]!=NULL) free(freq[c]);
    freq[c]=NULL;
    for(i=0;i<MAX_FREQS;i++) {
        if(phase[i][c]!=NULL) free(phase[i][c]);
        if(pwr_mag[i][c]!=NULL) free(pwr_mag[i][c]);
        phase[i][c]=NULL;  
        pwr_mag[i][c]=NULL;
    }
  } 
/* Now Process all cards */
  for(c=0;c<MAX_CARDS;c++) {
    if (active[c]) {
      sprintf(filename,"/tmp/phasing_cal_%s_%d.dat",radar_name,c);
      calfile=fopen(filename,"r");
      printf("%p %s\n",calfile,filename); 
      if (calfile!=NULL) {
        fread(&num_phasecodes[c],sizeof(int),1,calfile);
        printf("PhaseCodes: %d\n",num_phasecodes[c]);
        fread(&num_cards[c],sizeof(int),1,calfile);
        printf("Cards: %d\n",num_cards[c]);
        fread(&num_freqs[c],sizeof(int),1,calfile);
        printf("Freqs: %d\n",num_freqs[c]);
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
        }
        printf("Reading frequency array\n");
        count=fread(freq[c],sizeof(double),num_freqs[c],calfile);
        printf("%d %d\n",num_freqs[c],count);
        count=1;
        printf("Reading in data\n");
        while(count>0) {
          count=fread(&ii,sizeof(int),1,calfile);
          if (count==0) {
            break;
          }
          count=fread(phase[ii][c],sizeof(double),num_phasecodes[c],calfile);
          printf("Freq index: %d Phase Count: %d\n",ii,count);
          count=fread(pwr_mag[ii][c],sizeof(double),num_phasecodes[c],calfile);
          printf("Freq index: %d Pwr-mag Count: %d\n",ii,count);
          if (count==0) {
            break;
          }
        }
        if (count==0) {
          if (feof(calfile)) printf("End of File!\n");
        }
        fclose(calfile);
        for(f=0;f<num_freqs[c];f++) {
          printf("Freq Index: %d\n",f);
          for(ant=0;ant<16;ant++) {
            antenna_best_code[ant]=-1;
            antenna_best_time_difference[ant]=1E8;
            antenna_best_time_measured[ant]=-1;
            antenna_best_time_needed[ant]=-1;
          }
          p=1;
          for(i=0;i<13;i++) {
            time0_measured=phase_to_timedelay(phase[f][c][0],freq[c][f],0.0,0.0,0.1,&cycles, &delta);
            time_expected=expected_timedelay(p);
            time_measured=phase_to_timedelay(phase[f][c][p],freq[c][f],time_expected,time0_measured,0.1,&cycles, &delta);
//            timedelays[i]=time_measured-time0_measured;
            printf("Time Delay: %d %d %lf %lf %lf %d\n",i,p,time_expected,time_measured,delta,cycles);
            p=p<<1;
          }
          exit(0);
          for(angle=0;angle<16;angle++) {
              for(ant=0;ant<16;ant++) {
            for(p=0;p<num_phasecodes[c];p++) {
              time_expected=expected_timedelay(p);
              time0_measured=phase_to_timedelay(phase[f][c][0],freq[c][f],0.0,0.0,0.1,&cycles, &delta);
              time_measured=phase_to_timedelay(phase[f][c][p],freq[c][f],time_expected,time0_measured,0.1,&cycles, &delta);
                  time_needed=timedelay_needed(standard_angles[angle],spacing,ant)+time0_measured;
                  time_difference=time_needed-time_measured;
                if (abs(time_difference) < abs(antenna_best_time_difference[ant])) {
                    antenna_best_code[ant]=p;
                    antenna_best_time_difference[ant]=time_difference;
                    antenna_best_time_measured[ant]=time_measured;
                    antenna_best_time_needed[ant]=time_needed;
                }
//                printf("%d %d %d %lf %lf %d\n",angle,ant,p,time_needed,time_measured,cycles);
              } // ant loop
            } //phase
            printf("Freq %lf Angle: %lf:\n",freq[c][f],standard_angles[angle]);  
            for(ant=0;ant<16;ant++) printf("  Antenna %d Phasecode: %d T: %lf N: %lf\n",
              ant,antenna_best_code[ant],antenna_best_time_measured[ant],antenna_best_time_needed[ant]);  
          } //angle loop
        } //freq loop
/* Mean time delay at a phasecode across all frequencies 
*  for 100 beam directions
*/ 


/* Highly accurate Time delay at a phasecode for each frequency 
*  for 100 beam directions
*/ 
      } //if calfile 
    } // if active
  } //card loop  
} // end of main

