#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "vna_functions.h"

int32_t VNA_triggers=4;
int32_t VNA_wait_delay_ms=10;
int32_t VNA_min_nave=3;

extern int32_t MSI_phasecodes;

int mlog_data_command(int sock,char *command,double *array[VNA_FREQS],int b,int verbose) {
  int32_t count,rval,sample_count;
  char output[10]="";
  char command2[80];
  char cmd_str[80],prompt_str[10],data_str[1000];
  double base,exp;
  int32_t cr,lf;
      strcpy(command2,command);
      if (verbose>2) printf("%d Command: %s\n",(int) strlen(command2),command2);
      write(sock, &command2, sizeof(char)*strlen(command2));
      cr=0;
      lf=0;
      count=0;
      if (verbose>2) fprintf(stdout,"Command Output String::\n");
      strcpy(cmd_str,"");
      while((cr==0) || (lf==0)){
        rval=read(sock, &output, sizeof(char)*1);
#ifdef __QNX__
        if (rval<1) usleep(1000);
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
             base=0;
             exp=0;
             if((sample_count % 2) == 0) {
               if (sample_count/2 >=VNA_FREQS) {
                 printf("ERROR: too many samples... aborting\n");
                 exit(-1);
               }
               base=atof(strtok(data_str, "E"));
               exp=atof(strtok(NULL, "E"));
               array[sample_count/2][b]=base*pow(10,exp);
               if (verbose>2) fprintf(stdout,"%d ::  %s  ::  %lf , %lf :: %g",sample_count/2,data_str,base,exp,array[sample_count/2][b]);
             }
             sample_count++;
             if (verbose>2) fprintf(stdout,"\n%d: ",sample_count);
             strcpy(data_str,"");
        } else {
             strncat(data_str,output,rval);
        }
      }
      if((sample_count % 2) == 0) {
        if (sample_count/2 >=VNA_FREQS) {
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
        if (rval<1) usleep(1000);
#else
        if (rval<1) usleep(10);
#endif
        strncat(prompt_str,output,rval);
        if (verbose>2) fprintf(stdout,"%c",output[0]);
      }
  return 0;
}

int button_command(int sock, char *command,int wait_ms,int verbose) {
  int32_t count,rval;
  char output[10]="";
  char command2[80];
  char prompt_str[80];
/*
 * *  Process Command String with No feedback 
 * */
      strcpy(command2,command);
      if (verbose>2) fprintf(stdout,"%d Command: %s\n",(int) strlen(command2),command2);
      write(sock, &command2, sizeof(char)*strlen(command2));
      count=0;
      if (verbose>2) fprintf(stdout,"\nPrompt String::\n");
      while(output[0]!='>'){
        rval=read(sock, &output, sizeof(char)*1);
        strncat(prompt_str,output,rval);
        if (verbose>2) fprintf(stdout,"%c",output[0]);
        count++;
      }
      if (verbose>2) fprintf(stdout,"Command is done\n");
      fflush(stdout);
  usleep(wait_ms*1000);
  return 0;
}

int take_data(int sock,int b,int rnum,int c, int p,int a, double *pwr_mag[VNA_FREQS],double *phase[VNA_FREQS],double *tdelay[VNA_FREQS],
              int wait_ms,int ssh_flag,int verbose,double target_tdelay,double target_pwr){
  int t,rval;
  char command[128]="";

  rval=MSI_dio_write_memory(b,rnum,c,p,a,ssh_flag,verbose);
  if(rval!=0) return rval;

  usleep(1000*wait_ms);
  button_command(sock,":SENS1:AVER:CLE\r\n",30,verbose);
  for(t=0;t<VNA_triggers;t++) {
                      button_command(sock,":TRIG:SING\r\n",0,verbose);
                      button_command(sock,"*OPC?\r\n",0,verbose);
  }
  button_command(sock,"DISP:WIND1:TRAC1:Y:AUTO\r\n",10,verbose);
  button_command(sock,"DISP:WIND1:TRAC3:Y:AUTO\r\n",10,verbose);
  sprintf(command,"DISP:WIND1:TRAC3:Y:RLEV %E\r\n",target_tdelay);
  button_command(sock,command,10,verbose);
  sprintf(command,"DISP:WIND1:TRAC2:Y:RLEV %E\r\n",target_pwr);
  button_command(sock,command,10,verbose);

  button_command(sock,":CALC1:PAR1:SEL\r\n",10,verbose);
  mlog_data_command(sock,":CALC1:DATA:FDAT?\r\n",phase,b,verbose) ;
  button_command(sock,":CALC1:PAR2:SEL\r\n",10,verbose);
  mlog_data_command(sock,":CALC1:DATA:FDAT?\r\n",pwr_mag,b,verbose) ;
  button_command(sock,":CALC1:PAR3:SEL\r\n",10,verbose);
  mlog_data_command(sock,":CALC1:DATA:FDAT?\r\n",tdelay,b,verbose) ;
  return 0;
}

