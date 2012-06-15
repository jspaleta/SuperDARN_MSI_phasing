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

#define CARDS 200 
#define PHASECODES 8192 
//#define PHASECODES  64 
#define FREQS 1500
int stupid_flag=0;
int test_flag=-1000;
int sock=-1;
int verbose=2;
//char *hostip="209.114.113.119";
//char *hostip="137.229.27.122";
char *hostip="192.168.1.4";
char *file_prefix="rf_cal";
char *file_ext=".dat";
char filename[120];
char *dir="/tmp/";
FILE *calfile=NULL;
int port=23;
char command[80];
char radar_name[80];
char freq_start[10]="8E6";
char freq_stop[10]="20E6";
char freq_steps[10]="201";

struct timeval t0,t1,t2,t3,t4,t5,t6;
struct timeval t10,t11;
unsigned long elapsed;

int mlog_data_command(char *command,double *array[FREQS],int b) {
  int count,rval,sample_count;
  char output[10]="";
  char command2[80];
  char cmd_str[80],prompt_str[10],data_str[1000];
  int cr,lf;
      strcpy(command2,command);
      if (verbose>2) printf("%d Command: %s\n",strlen(command2),command2);
      write(sock, &command2, sizeof(char)*strlen(command2));
      cr=0;
      lf=0;
      count=0;
      if (verbose>2) fprintf(stdout,"Command Output String::\n");
      strcpy(cmd_str,"");
      while((cr==0) || (lf==0)){
        rval=read(sock, &output, sizeof(char)*1);
#ifdef __QNX__
        if (rval<1) delay(1);
#else
        if (rval<1) {
          usleep(10);
  
        }
#endif
        if (output[0]==13) {
          cr++;
          continue;
        }
        if (output[0]==10) {
          lf++;
          continue;
        }
        count+=rval;
        strncat(cmd_str,output,rval);
        if (verbose>2) fprintf(stdout,"%c",output[0]);
      }
      if (verbose>2) printf("Processing Data\n");

      cr=0;
      lf=0;
      count=0;
      sample_count=0;
      if (verbose>2) fprintf(stdout,"\nData Output String::\n");
      strcpy(data_str,"");
      if (verbose>2) fprintf(stdout,"%d: ",sample_count);
      while((cr==0) || (lf==0)){
        rval=read(sock, &output, sizeof(char)*1);
        if (output[0]==13) {
          cr++;
          continue;
        }
        if (output[0]==10) {
          lf++;
          continue;
        }
        if(output[0]==',') {
             if((sample_count % 2) == 0) {
               if (sample_count/2 >=FREQS) {
                 printf("ERROR: too many samples... aborting\n");
                 exit(-1);
               }
               array[sample_count/2][b]=atof(data_str);
               if (verbose>2) fprintf(stdout,"%s  ::  %lf",data_str,array[sample_count/2][b]);
             }
             sample_count++;
             if (verbose>2) fprintf(stdout,"\n%d: ",sample_count);
             strcpy(data_str,"");
        } else {
             strncat(data_str,output,rval);
        }
      }
      if((sample_count % 2) == 0) {
        if (sample_count/2 >=FREQS) {
          printf("ERROR: too many samples... aborting\n");
          exit(-1);
        }
        array[sample_count/2][b]=atof(data_str);
        if (verbose>2) fprintf(stdout,"%s  ::  %lf",data_str,array[sample_count/2][b]);
      }
      sample_count++;
      strcpy(data_str,"");
      if (verbose>2) fprintf(stdout,"\nSamples: %d\n",sample_count/2);
      if (verbose>2) fprintf(stdout,"\nPrompt String::\n");
      while(output[0]!='>'){
        rval=read(sock, &output, sizeof(char)*1);
#ifdef __QNX__
        if (rval<1) delay(1);
#else
        if (rval<1) usleep(10);
#endif
        strncat(prompt_str,output,rval);
        if (verbose>2) fprintf(stdout,"%c",output[0]);
      }
  return 0;
}

int button_command(char *command) {
  int count,rval;
  char output[10]="";
  char command2[80];
  char prompt_str[80];
/*
*  Process Command String with No feedback 
*/
      strcpy(command2,command);
      if (verbose>2) fprintf(stdout,"%d Command: %s\n",strlen(command2),command2);
      write(sock, &command2, sizeof(char)*strlen(command2));
      count=0;
      if (verbose>2) fprintf(stdout,"\nPrompt String::\n");
      while(output[0]!='>'){
        rval=read(sock, &output, sizeof(char)*1);
        strncat(prompt_str,output,rval);
        if (verbose>2) fprintf(stdout,"%c",output[0]);
        count++;
      }
      if (verbose>2) fprintf(stdout,"Command is done\n",command2);
      fflush(stdout);
  return 0;
}
void mypause ( void ) 
{ 
  fflush ( stdin );
  printf ( "Press [Enter] to continue . . ." );
  fflush ( stdout );
  getchar();
} 

int main(int argc, char **argv)
{
  char output[40],strout[40];
  char cmd_str[80],prompt_str[10],data_str[1000];
  double *phase[FREQS],*pwr_mag[FREQS];
  double freq[FREQS];
  int rval,count,sample_count,fail,cr,lf;
  int i=0,c=0,data=0,index=0; 
  unsigned int b=0;
  int beamcode=0;
  double fstart;
  double fstop;
  double fstep;
  int fnum;
  int radar;
  unsigned int portA0,portB0,portC0,cntrl0 ;
  unsigned int portA1,portB1,portC1,cntrl1 ;
	int		 temp, pci_handle, j,  IRQ  ;
	unsigned char	 *BASE0, *BASE1;
	unsigned int	 mmap_io_ptr,IOBASE, CLOCK_RES;
	float		 time;
#ifdef __QNX__
	struct		 _clockperiod new, old;
	struct		 timespec start_p, stop_p, start, stop, nsleep;

    if(argc <2 ) {
      fprintf(stderr,"%s: invoke with radar number (1 or 2 or 3)\n",argv[0]);
      fflush(stderr);
      exit(0);
    }
    if(argc ==3 ) {
      stupid_flag=1;
    } else {
      stupid_flag=0;
    }
    if(argc ==4 ) {
      test_flag=atoi(argv[3]);
      c=atoi(argv[2]); 
    } else {
      test_flag=-1000;
    } 
    radar=atoi(argv[1]);
    printf("Radar: %d\n",radar);
    printf("Test flag: %d\n",test_flag);
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
#endif
  fnum=atoi(freq_steps);
  fstart=atof(freq_start);
  fstop=atof(freq_stop);
  fstep=(fstop-fstart)/(fnum-1);
  for(i=0;i<fnum;i++) {
    freq[i]=fstart+i*fstep;
    phase[i]=calloc(PHASECODES,sizeof(double));
    pwr_mag[i]=calloc(PHASECODES,sizeof(double));
  }
if(test_flag==-1000) {
// Open Socket and initial IO
  if (verbose>0) printf("Opening Socket %s %d\n",hostip,port);
  sock=opentcpsock(hostip, port);
  if (sock < 0) {
    if (verbose>0) printf("Socket failure %d\n",sock);
  } else if (verbose>0) printf("Socket %d\n",sock);
  rval=read(sock, &output, sizeof(char)*10);
  if (verbose>0) fprintf(stdout,"Initial Output Length: %d\n",rval);
  strcpy(strout,"");
  strncat(strout,output,rval);
  if (verbose>0) fprintf(stdout,"Initial Output String: %s\n",strout);

  button_command(":SYST:PRES\r\n");
  sprintf(command,":SENS1:FREQ:STAR %s\r\n",freq_start);
  button_command(command);
  sprintf(command,":SENS1:FREQ:STOP %s\r\n",freq_stop);
  button_command(command);
  sprintf(command,":SENS1:SWE:POIN %s\r\n",freq_steps);
  button_command(command);
  button_command(":CALC1:PAR:COUN 2\r\n");
  button_command(":CALC1:PAR1:SEL\r\n");
  button_command(":CALC1:PAR1:DEF S21\r\n");
  button_command(":CALC1:FORM UPH\r\n");
  button_command(":CALC1:PAR2:SEL\r\n");
  button_command(":CALC1:PAR2:DEF S21\r\n");
  button_command(":CALC1:FORM MLOG\r\n");
  button_command(":SENS1:AVER OFF\r\n");
  button_command(":SENS1:AVER:COUN 4\r\n");
  button_command(":SENS1:AVER:CLE\r\n");
  button_command(":INIT1:CONT OFF\r\n");

  printf("\n\n\7\7Calibrate Network Analyzer for S12,S21\n");
  mypause();
  button_command(":SENS1:CORR:COLL:METH:THRU 1,2\r\n");
  sleep(1);
  button_command(":SENS1:CORR:COLL:THRU 1,2\r\n");
  printf("  Doing S1,2 Calibration..wait 4 seconds\n");
  sleep(4);

  button_command(":SENS1:CORR:COLL:METH:THRU 2,1\r\n");
  sleep(1);
  button_command(":SENS1:CORR:COLL:THRU 2,1\r\n");
  printf("  Doing S2,1 Calibration..wait 4 seconds\n");
  sleep(4);
  button_command(":SENS1:CORR:COLL:SAVE\r\n");

  button_command(":INIT1:IMM\r\n");
  printf("\n\nCalibration Complete\nReconfigure for Phasing Card Measurements");
  mypause();
  c=-1; 
  printf("\n\nEnter Radar Name: ");
  fflush(stdin);
  scanf("%s", &radar_name);
  fflush(stdout);
  fflush(stdin);
  printf("\n\nEnter Phasing Card Number: ");
  fflush(stdin);
  fflush(stdout);
  scanf("%d", &c);
  printf("Radar: <%s>  Card: %d\n",radar_name,c);
  fflush(stdout);
}
  while((c<CARDS) && (c >=0)) {
  if(test_flag==-1000) {
    sprintf(filename,"%s%s_%s_%d%s",dir,file_prefix,radar_name,c,file_ext);
    if (verbose>0) fprintf(stdout,"Using file: %s\n",filename);
    fflush(stdout);
    gettimeofday(&t0,NULL);
    calfile=fopen(filename,"w");
    count=PHASECODES;
    fwrite(&count,sizeof(int),1,calfile);
    count=CARDS;
    fwrite(&count,sizeof(int),1,calfile);
    count=fnum;
    fwrite(&count,sizeof(int),1,calfile);
    count=0;
    fwrite(freq,sizeof(double),fnum,calfile);
    if (verbose>0) {
      fprintf(stdout,"Writing beamcodes to phasing card\n");
      gettimeofday(&t2,NULL);
    }
    delay(10);
  } //test flag if
  if(stupid_flag) {
    while(1) {  //JDS  
      if (b==0) b=8; //JDS
      else b=0;//JDS
//JDS    for (b=0;b<PHASECODES;b++) {
      data=b;
      beamcode=b;
      if(NEW_PMAT) {
        printf("B: %d data: %d BC: %d\n",b,data,beamcode); 
        sleep(2); //JDS 
      } else {
      }
    }
}
   for (b=0;b<PHASECODES;b++) {
      data=b;
      beamcode=b;
      if(NEW_PMAT) {
        printf("B: %d data: %d BC: %d\n",b,data,beamcode); 
      } else {
      }
    }
    if(verbose> 0 ) {
      gettimeofday(&t3,NULL);
      elapsed=(t3.tv_sec-t2.tv_sec)*1E6;
      elapsed+=(t3.tv_usec-t2.tv_usec);
      printf("  Program Card Elapsed Seconds: %lf\n",(float)elapsed/1E6);
    }
    if(test_flag==-1) {
      exit(0);
    }
    sleep(10);
    if (verbose>0) fprintf(stdout,"Measuring phases\n");
    for (b=0;b<PHASECODES;b++) {
//jds    for (index=-1;index<=13;index++) {
//      if(index==-1) b=0;
//      else if(index==13) b=8191;
//      else b=(int)pow(2,index); 
// Start of Command Block
      beamcode=b;
      if(verbose> 0 ) {
        printf(":::Card %d::: BEAMCode %d\n",c,beamcode);
        gettimeofday(&t4,NULL);
      }
//select beamcode
      gettimeofday(&t10,NULL);
      //button_command(":SENS1:AVER:CLE\r\n");
      button_command(":INIT1:IMM\r\n");
#ifdef __QNX__
      delay(10); //NO AVERAGE
#else
      usleep(5);
#endif
    if(verbose> 0 ) {
      gettimeofday(&t11,NULL);
      elapsed=(t11.tv_sec-t10.tv_sec)*1E6;
      elapsed+=(t11.tv_usec-t10.tv_usec);
      printf("  Measure Delay Total Elapsed Seconds: %lf\n",(float)elapsed/1E6);
    }
      button_command(":CALC1:PAR1:SEL\r\n");
      mlog_data_command(":CALC1:DATA:FDAT?\r\n",phase,b) ;
//      printf("End\n");
      printf("Phase 0: %lf Phase %d: %lf\n",phase[0][b],fnum-1,phase[fnum-1][b]);
//      exit(0);
      button_command(":CALC1:PAR2:SEL\r\n");
      mlog_data_command(":CALC1:DATA:FDAT?\r\n",pwr_mag,b) ;
      if(verbose> 0 ) {
        gettimeofday(&t5,NULL);
        elapsed=(t5.tv_sec-t4.tv_sec)*1E6;
        elapsed+=(t5.tv_usec-t4.tv_usec);
        printf("  Measure Phasecode: %d Elapsed Seconds: %lf\n",b,(float)elapsed/1E6);
      }
//      for(i=0;i<fnum;i++) {
//        if (verbose > 1) printf("Freq %lf:  Phase %d:%lf \n",freq[i],b,phase[i][b]);
//      }
    } // end of phasecode loop
    if(verbose> 0 ) {
      gettimeofday(&t6,NULL);
      elapsed=(t6.tv_sec-t2.tv_sec)*1E6;
      elapsed+=(t6.tv_usec-t2.tv_usec);
      printf("  All Measurements Total Elapsed Seconds: %lf\n",(float)elapsed/1E6);
    }
//    if(verbose> 0 ) {
//      gettimeofday(&t6,NULL);
//      elapsed=(t6.tv_sec-t2.tv_sec)*1E6;
//      elapsed+=(t6.tv_usec-t2.tv_usec);
//      printf("  Card Calc Total Elapsed Seconds: %lf\n",(float)elapsed/1E6);
//    }
    for(i=0;i<fnum;i++) {
      if (verbose > 1) printf("Freq %lf:  Phase 0:%lf Phase 8191: %lf\n",freq[i],phase[i][0],phase[i][PHASECODES-1]);
      fwrite(&i,sizeof(int),1,calfile);
      count=fwrite(phase[i],sizeof(double),PHASECODES,calfile);
      count=fwrite(pwr_mag[i],sizeof(double),PHASECODES,calfile);
      //printf("Freq index: %d Count: %d\n",i,count);
    }
    printf("Closing File\n");
    fclose(calfile);
    if(verbose> 0 ) {
      gettimeofday(&t1,NULL);
      elapsed=(t1.tv_sec-t0.tv_sec)*1E6;
      elapsed+=(t1.tv_usec-t0.tv_usec);
      printf("%d Cards %d PhaseCodes, Elapsed Seconds: %lf\n",CARDS,PHASECODES,(float)elapsed/1E6);
    }
    c=-1;
//    if (verbose>0) fprintf(stdout,"Asking for another card:\n");
//    printf("Enter Next Card Number (CTRL-C to exit): ");
//    scanf("%d", &c);
  } // end of Card loop
}

