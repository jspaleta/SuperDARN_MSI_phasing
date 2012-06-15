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
#define ATTENCODES 64 
//#define PHASECODES  64 
#define FREQS 1500
#define _QUICK_

int stupid_flag=0;
int setup_flag=1;
int test_flag=-1000;
int sock=-1;
int verbose=2;
//char *hostip="192.168.1.2";
//char *hostip="209.114.113.119";
char *hostip="137.229.27.122";
//char *hostip="67.59.83.38";
char *file_prefix="phasing_cal";
char *file_ext=".dat";
char filename[120];
char *dir="/data/calibrations/";
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
  int32 count,rval,sample_count;
  char output[10]="";
  char command2[80];
  char cmd_str[80],prompt_str[10],data_str[1000];
  int32 cr,lf;
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
  int32 count,rval;
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
/*-SET WRITE ENABLE BIT-------------------------------------------------------*/
int set_WE(int base,int onoff,int radar){
        int temp;
        int portA,portB,portC;
        switch(radar) {
          case 1:
            portC=PC_GRP_0;
            portB=PB_GRP_0;
            portA=PA_GRP_0;
            break;
          case 2:
            portC=PC_GRP_2;
            portB=PB_GRP_2;
            portA=PA_GRP_2;
            break;
          case 3:
            portC=PC_GRP_4;
            portB=PB_GRP_4;
            portA=PA_GRP_4;
            break;
        }
        if(onoff==OFF){
                temp=in8(base+portC);
                out8(base+portC,temp & 0xfe);
        }
        if(onoff==ON){
                temp=in8(base+portC);
                out8(base+portC,temp | 0x01);
        }
}
/*-SET READ/WRITE BIT-------------------------------------------------------*/
int set_RW(int base,int rw,int radar){
        int temp;
        int portA,portB,portC;
        switch(radar) {
          case 1:
            portC=PC_GRP_0;
            portB=PB_GRP_0;
            portA=PA_GRP_0;
            break;
          case 2:
            portC=PC_GRP_2;
            portB=PB_GRP_2;
            portA=PA_GRP_2;
            break;
          case 3:
            portC=PC_GRP_4;
            portB=PB_GRP_4;
            portA=PA_GRP_4;
            break;
        }
        if(rw==READ){
                temp=in8(base+portC);
                out8(base+portC,temp & 0xbf);
        }
        if(rw==WRITE){
                temp=in8(base+portC);
                out8(base+portC,temp | 0x40);
        }
}
/*-SET SWITCHED/ATTEN BIT-------------------------------------------------------*/
int set_SA(int base,int sa,int radar){
        int temp;
        int portA,portB,portC;
        switch(radar) {
          case 1:
            portC=PC_GRP_0;
            portB=PB_GRP_0;
            portA=PA_GRP_0;
            break;
          case 2:
            portC=PC_GRP_2;
            portB=PB_GRP_2;
            portA=PA_GRP_2;
            break;
          case 3:
            portC=PC_GRP_4;
            portB=PB_GRP_4;
            portA=PA_GRP_4;
            break;
        }
        if(sa==SWITCHES){
                temp=in8(base+portC);
                out8(base+portC,temp & 0x7f);
        }
        if(sa==ATTEN){
                temp=in8(base+portC);
                out8(base+portC,temp | 0x80);
        }
}


/*-REVERSE_BITS-------------------------------------------------------*/
int reverse_bits(int data){
	
	int temp=0;
	
	temp=temp + ((data & 1)  << 12);
	temp=temp + ((data & 2)  << 10);
	temp=temp + ((data & 4)  << 8);
	temp=temp + ((data & 8)  << 6);
	temp=temp + ((data & 16)  << 4);
	temp=temp + ((data & 32)  << 2);
	temp=temp + ((data & 64)  << 0);
	temp=temp + ((data & 128)  >> 2);
	temp=temp + ((data & 256)  >> 4);
	temp=temp + ((data & 512)  >> 6);
	temp=temp + ((data & 1024)  >> 8);
	temp=temp + ((data & 2048)  >> 10);
	temp=temp + ((data & 4096)  >> 12);

	return temp;
}
/*-GET_DEALY---------------------------------------------------------*/
float get_delay(int code){
	
	int	i;
	float	delay;
	float	delaylist[]={0.25, 0.45, 0.8, 1.5, 2.7, 5, 8, 15, 25, 45, 80, 140, 250};

	delay=0;
	for(i=0;i<13;i++){
		delay+=( (code & (int)pow(2,i)) >> i)*delaylist[i];
	}	
	return delay;
}
/*-BEAM_CODE---------------------------------------------------------*/
int beam_code(unsigned int base, int code,int radar){
	/* the beam code is 13 bits, pAD0 thru pAD12.  This code
	   uses bits 0-7 of CH0, PortA, and bits 0-4 of CH0, PortB
	   to output the beam code. Note: The beam code is an address
	   of the EEPROMs in the phasing cards.  This code is broadcast
	   to ALL phasing cards.  If you are witing the EEPROM, then this
	   be the beam code you are writing
	*/
	

	int temp;
        int portA,portB,portC;
        switch(radar) {
          case 1:
            portC=PC_GRP_0;
            portB=PB_GRP_0;
            portA=PA_GRP_0;
            break;
          case 2:
            portC=PC_GRP_2;
            portB=PB_GRP_2;
            portA=PA_GRP_2;
            break;
          case 3:
            portC=PC_GRP_4;
            portB=PB_GRP_4;
            portA=PA_GRP_4;
            break;
        }
#ifdef __QNX__
        //printf("Selecting Beam Code: %d\n",code);

    // check if beam code is reasonable
	if ( (code>8192) | (code<0) ){
		fprintf(stderr,"INVALID BEAM CODE - must be between 0 and 8192\n");
                fflush(stderr);
		return -1;
	}
    // bit reverse the code
	code=reverse_bits(code);
        //printf(" Output  : Reversed Code: 0x%x\n",code);
    // set CH0, Port A to lowest 8 bits of beam code and output on PortA
	temp=code & 0xff;
	out8(base+portA,temp);
    // set CH0, Port B to upper 5 bits of beam code and output on PortB
	temp=code & 0x1f00;
	temp=temp >> 8;
	out8(base+portB,temp);
    // verify that proper beam code was sent out
	temp=in8(base+portB);
	temp=(temp & 0x1f) << 8;
	temp=temp+in8(base+portA);
        //printf(" Readback: Reversed BeamCode: 0x%x\n",temp);
	if (temp==code) return 0;
	else{
		fprintf(stderr,"BEAM CODE OUTPUT ERROR - requested code not sent\n");
                fflush(stderr);
		return -1;
	}
#else
  return 0; 
#endif
}
/*-SELECT_CARD------------------------------------------------------*/
int select_card(unsigned int base, int address,int radar){

	/* This code selects a card to address.  This can be used for
	   writing data to the EEPROM, or to verify the output of the
	   EEPROM. There are 20 cards in the phasing matrix, addresses
	   0-19.  A card is addressed when this address corresponds to
	   the switches on the phasing card.  Card address 31 is reserved for
	   programming purposes.
	*/
        int portA,portB,portC;
        switch(radar) {
          case 1:
            portC=PC_GRP_0;
            portB=PB_GRP_0;
            portA=PA_GRP_0;
            break;
          case 2:
            portC=PC_GRP_2;
            portB=PB_GRP_2;
            portA=PA_GRP_2;
            break;
          case 3:
            portC=PC_GRP_4;
            portB=PB_GRP_4;
            portA=PA_GRP_4;
            break;
        }
#ifdef __QNX__	
	int temp;
	struct 	timespec nsleep;
	nsleep.tv_sec=0;
	nsleep.tv_nsec=5000;


    // check if card address is reasonable
	if ( (address>31) | (address<0) ){
		fprintf(stderr,"INVALID CARD ADDRESS - must be between 0 and 32\n");
                fflush(stderr);
		return -1;
	}
    // shift address left 1 bit (write enable is the lowest bit)
	address=address << 1;
    // mask out bits not used for addressing the cards
	address=address & 0x3e;
    // check for other bits in CH0, PortC that may be on
	temp=in8(base+portC);
	temp=temp & 0xc1;
    // add other bit of PortC to the address bits
	address=address+temp;
    // output the address and original other bits to PortC
	out8(base+portC,address);
	nanosleep(&nsleep,NULL);
    // verify the output
	temp=in8(base+portC);
	if (temp==address) return 0;
	else{
		fprintf(stderr,"CARD SELECT OUTPUT ERROR - requested code not sent\n");
		fprintf(stderr," code=%d\n", temp);
		return -1;
	}
#else
  return 0;
#endif	
}

int write_attenuators(unsigned int base, int card, int code, int data,int radar){

        int temp;
        struct  timespec nsleep;
        nsleep.tv_sec=0;
        nsleep.tv_nsec=5000;
        int portA,portB,portC,cntrl1;
        switch(radar) {
          case 1:
            portC=PC_GRP_1;
            portB=PB_GRP_1;
            portA=PA_GRP_1;
            cntrl1=CNTRL_GRP_1;
            break;
          case 2:
            portC=PC_GRP_3;
            portB=PB_GRP_3;
            portA=PA_GRP_3;
            cntrl1=CNTRL_GRP_3;
            break;
          case 3:
            portC=PC_GRP_3;
            portB=PB_GRP_3;
            portA=PA_GRP_3;
            cntrl1=CNTRL_GRP_3;
        }
    // check that the data to write is valid
        if ( (data>63) | (data<0) ){
                fprintf(stderr,"INVALID DATA TO WRITE - must be between 0 and 63\n");
                return -1;
        }
        data=data ^ 0x3f;
    // select card to write
        temp=select_card(base,card,radar);
    // choose the beam code to write (output appropriate EEPROM address
        temp=beam_code(base,code,radar);
        set_SA(base,ATTEN,radar);
    // enable writing
        set_RW(base,WRITE,radar);
    // set CH1, PortA and Port B to output for writing
        out8(base+cntrl1,0x81);
    // bit reverse the data
        data=reverse_bits(data);
    // apply the data to be written to PortA and PortB on CH1
    // set CH1, Port A to lowest 8 bits of data and output on PortA
        temp=data & 0xff;
        out8(base+portA,temp);
    // set CH0, Port B to upper 5 bits of data and output on PortB
        temp=data & 0x1f00;
        temp=(temp >> 8);
        out8(base+portB,temp);
        out8(base+cntrl1,0x01);
    // toggle write enable bit
        set_WE(base,ON,radar);
        set_WE(base,OFF,radar);
    // reset CH1, PortA and PortB to inputs
        out8(base+cntrl1,0x93);
        out8(base+cntrl1,0x13);
    // disable writing
        set_RW(base,READ,radar);
        delay(3);
    // verify written data
    // read PortA and PortB to see if EEPROM output is same as progammed
        temp=in8(base+portB);
        temp=temp & 0x1f;
        temp=temp << 8;
        temp=temp + in8(base+portA);
        temp=temp & 0x1f80;
        if (temp == data){
                //printf("    Code read after writing is %d\n", reverse_bits(temp));
         //       printf("       - DATA WAS WRITTEN: data: %x = readback: %x\n", reverse_bits(data), reverse_bits(temp));
                return 0;
        }
        else {
                printf(" ERROR - ATTEN DATA NOT WRITTEN: data: %x != readback: %x :: Code: %d Card: %d\n", reverse_bits(data), reverse_bits(temp),code,card);
                return -1;
        }
}
/*-VERIFY_CODE--------------------------------------------------------*/
int verify_attenuators(unsigned int base, int card, int code, int data,int radar){

        int temp;
        struct  timespec nsleep;
        nsleep.tv_sec=0;
        nsleep.tv_nsec=5000;
        int portA,portB,portC,cntrl1;
        switch(radar) {
          case 1:
            portC=PC_GRP_1;
            portB=PB_GRP_1;
            portA=PA_GRP_1;
            cntrl1=CNTRL_GRP_1;
            break;
          case 2:
            portC=PC_GRP_3;
            portB=PB_GRP_3;
            portA=PA_GRP_3;
            cntrl1=CNTRL_GRP_3;
            break;
          case 3:
            portC=PC_GRP_3;
            portB=PB_GRP_3;
            portA=PA_GRP_3;
            cntrl1=CNTRL_GRP_3;
        }
    // check that the data to write is valid
        if ( (data>63) | (data<0) ){
                fprintf(stderr,"INVALID ATTEN DATA TO VERIFY - must be between 0 and 63\n");
                return -1;
        }
        data=data ^ 0x3f;
    // bit reverse the data
        data=reverse_bits(data);
    // select card to write
        temp=select_card(base,card,radar);
    // choose the beam code to write (output appropriate EEPROM address
        temp=beam_code(base,code,radar);
        set_SA(base,ATTEN,radar);
    // disable writing
        set_RW(base,READ,radar);
        delay(10);
    // verify written data
    // read PortA and PortB to see if EEPROM output is same as progammed
        temp=in8(base+portB);
        temp=temp & 0x1f;
        temp=temp << 8;
        temp=temp + in8(base+portA);
        temp=temp & 0x1f80;
        if (temp == data){
                //printf("    Code read after writing is %d\n", reverse_bits(temp));
         //       printf("       - DATA WAS WRITTEN: data: %x = readback: %x\n", reverse_bits(data), reverse_bits(temp));
                return 0;
        }
        else {
                printf(" ERROR - ATTEN DATA NOT VERIFIED: data: %x != readback: %x :: Code: %d Card: %d\n", reverse_bits(data), reverse_bits
(temp),code,card);
                return -1;
        }
}

int verify_data_new(unsigned int base, int card, int code, int data,int radar,int print){

        int temp;
        struct  timespec nsleep;
        nsleep.tv_sec=0;
        nsleep.tv_nsec=5000;
        int portA,portB,portC,cntrl1;
        switch(radar) {
          case 1:
            portC=PC_GRP_1;
            portB=PB_GRP_1;
            portA=PA_GRP_1;
            cntrl1=CNTRL_GRP_1;
            break;
          case 2:
            portC=PC_GRP_3;
            portB=PB_GRP_3;
            portA=PA_GRP_3;
            cntrl1=CNTRL_GRP_3;
            break;
          case 3:
            portC=PC_GRP_3;
            portB=PB_GRP_3;
            portA=PA_GRP_3;
            cntrl1=CNTRL_GRP_3;
        }

    // check that the data to write is valid
        if ( (data>8192) | (data<0) ){
                fprintf(stderr,"INVALID DATA TO WRITE - must be between 0 and 8192\n");
                return -1;
        }
        data=data ^ 0x1fff;
    // select card to write
        temp=select_card(base,card,radar);
    // choose the beam code to write (output appropriate EEPROM address
        temp=beam_code(base,code,radar);
        set_SA(base,SWITCHES,radar);
    // bit reverse the data
        data=reverse_bits(data);
        if( print ) printf("    Code to write is %d\n", data);
    // reset CH1, PortA and PortB to inputs
        out8(base+cntrl1,0x93);
        out8(base+cntrl1,0x13);
    // verify written data
    // read PortA and PortB to see if EEPROM output is same as progammed
        temp=in8(base+portB);
        temp=temp & 0x1f;
        temp=temp << 8;
        temp=temp + in8(base+portA);
        temp=temp & 0x1fff;
        if ((temp == data) ){
                if(print)
                  printf("    data expected: %d data read: %d\n", data,temp);
                return 0;
        }
        else {
                printf(" ERROR - New Card DATA NOT Verified: data: %x != readback: %x :: Code: %d Card: %d\n", reverse_bits(data), reverse_bits(temp),code,card);
                return -1;
        }
        
}

/*-WRITE_CODE--------------------------------------------------------*/
int write_data_new(unsigned int base, int card, int code, int data,int radar,int print){

        int temp;
        struct  timespec nsleep;
        nsleep.tv_sec=0;
        nsleep.tv_nsec=5000;
        int portA,portB,portC,cntrl1;
        switch(radar) {
          case 1:
            portC=PC_GRP_1;
            portB=PB_GRP_1;
            portA=PA_GRP_1;
            cntrl1=CNTRL_GRP_1;
            break;
          case 2:
            portC=PC_GRP_3;
            portB=PB_GRP_3;
            portA=PA_GRP_3;
            cntrl1=CNTRL_GRP_3;
            break;
          case 3:
            portC=PC_GRP_3;
            portB=PB_GRP_3;
            portA=PA_GRP_3;
            cntrl1=CNTRL_GRP_3;
        }

    // check that the data to write is valid
        if ( (data>8192) | (data<0) ){
                fprintf(stderr,"INVALID DATA TO WRITE - must be between 0 and 8192\n");
                return -1;
        }
        data=data ^ 0x1fff;
        if( print ) printf("    Code to write is %d\n", data);
    // select card to write
        temp=select_card(base,card,radar);
    // choose the beam code to write (output appropriate EEPROM address
        temp=beam_code(base,code,radar);
        set_SA(base,SWITCHES,radar);
    // enable writing
        set_RW(base,WRITE,radar);
    // set CH1, PortA and Port B to output for writing
        out8(base+cntrl1,0x81);
    // bit reverse the data
        data=reverse_bits(data);
    // apply the data to be written to PortA and PortB on CH1
    // set CH1, Port A to lowest 8 bits of data and output on PortA
        temp=data & 0xff;
        out8(base+portA,temp);
    // set CH0, Port B to upper 5 bits of data and output on PortB
        temp=data & 0x1f00;
        temp=(temp >> 8);
        out8(base+portB,temp);
        out8(base+cntrl1,0x01);
        
    // toggle write enable bit
        set_WE(base,ON,radar);
        if (stupid_flag) {
          printf("Exiting write with WE enabled");
          exit(2); //JDS
        } 
        set_WE(base,OFF,radar);
    // reset CH1, PortA and PortB to inputs
        out8(base+cntrl1,0x93);
        out8(base+cntrl1,0x13);
    // disable writing
        set_RW(base,READ,radar);
        delay(3);
    // reset CH1, PortA and PortB to inputs
        out8(base+cntrl1,0x93);
        out8(base+cntrl1,0x13);
    // verify written data
    // read PortA and PortB to see if EEPROM output is same as progammed
        temp=in8(base+portB);
        temp=temp & 0x1f;
        temp=temp << 8;
        temp=temp + in8(base+portA);
        temp=temp & 0x1fff;
        if ((temp == data) ){
                if(print)
                  printf("    Code read after writing is %d\n", reverse_bits(temp));
                return 0;
        }
        else {
                printf(" ERROR - New Card DATA NOT WRITTEN: data: %x != readback: %x :: Code: %d Card: %d\n", reverse_bits(data), reverse_bits(temp),code,card);
                return -1;
        }
}

/*-WRITE_CODE--------------------------------------------------------*/
int write_data_old(unsigned int base, int card, int code, int data,int radar){

        int temp;
        struct  timespec nsleep;
        nsleep.tv_sec=0;
        nsleep.tv_nsec=5000;
        int portA0,portB0,portC0,cntrl0;
        int portA1,portB1,portC1,cntrl1;
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
            portC0=PC_GRP_4;
            portC1=PC_GRP_3;
            portB0=PB_GRP_4;
            portB1=PB_GRP_3;
            portA0=PA_GRP_4;
            portA1=PA_GRP_3;
            cntrl0=CNTRL_GRP_4;
            cntrl1=CNTRL_GRP_3;
            break;
        }
    // check that the data to write is valid
        if ( (data>8192) | (data<0) ){
                fprintf(stderr,"INVALID DATA TO WRITE - must be between 0 and 8192\n");
                return -1;
        }
    // select card 31 so that no real card is selected
        temp=select_card(base,31,radar);
    // choose the beam code to write (output appropriate EEPROM address
        temp=beam_code(base,code,radar);
        usleep(1000);
    // enable writing (turn on WRITE_ENABLE);
        temp=in8(base+portC0);
        temp=temp | 0x01;
        out8(base+portC0,temp);
        usleep(1000);
    // set CH1, PortA and Port B to output for writing
        out8(base+cntrl1,0x81);
        usleep(1000);
    // bit reverse the data
        data=reverse_bits(data);
    // apply the data to be written to PortA and PortB on CH1
    // set CH1, Port A to lowest 8 bits of data and output on PortA
        temp=data & 0xff;
        out8(base+portA1,temp);
        usleep(1000);
    // set CH0, Port B to upper 5 bits of data and output on PortB
        temp=data & 0x1f00;
        temp=(temp >> 8);
        out8(base+portB1,temp);
        usleep(1000);
        out8(base+cntrl1,0x01);
        usleep(1000);
    // select card to write
        temp=select_card(base,card,radar);
        usleep(1000);
    // select card 31 so that no real card is selected
        temp=select_card(base,31,radar);
        usleep(1000);
    // reset CH1, PortA and PortB to inputs
        out8(base+cntrl1,0x93);
        usleep(1000);
        out8(base+cntrl1,0x13);
        usleep(1000);
    // disable writing (turn off WRITE_ENABLE);
        temp=in8(base+portC0);
        temp=temp & 0xfe;
        out8(base+portC0,temp);
        usleep(1000);
    // verify written data
    // select card to read
        temp=select_card(base,card,radar);
        usleep(1000);
    // read PortA and PortB to see if EEPROM output is same as progammed
        temp=in8(base+portB1);
        usleep(1000);
        temp=temp & 0x1f;
        temp=temp << 8;
        temp=temp + in8(base+portA1);
        usleep(1);
        if (temp == data){
                //printf("    Code read after writing is %d\n", reverse_bits(temp));
                temp=select_card(base,31,radar);
                return 0;
        }
        else {
                printf(" ERROR - Old Card DATA NOT WRITTEN: data: %x != readback: %x :: Code: %d Card: %d\n", reverse_bits(data), reverse_bits(temp),code,card);
                temp=select_card(base,31,radar);
                return -1;
        }
}


/*-VERIFY_CODE--------------------------------------------------------*/
int verify_data_old(unsigned int base, int card, int code, int data,int radar, int print){

        int temp,rdata;
        struct  timespec nsleep;
        nsleep.tv_sec=0;
        nsleep.tv_nsec=5000;
        int portA0,portB0,portC0,cntrl0;
        int portA1,portB1,portC1,cntrl1;
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
            portC0=PC_GRP_4;
            portC1=PC_GRP_3;
            portB0=PB_GRP_4;
            portB1=PB_GRP_3;
            portA0=PA_GRP_4;
            portA1=PA_GRP_3;
            cntrl0=CNTRL_GRP_4;
            cntrl1=CNTRL_GRP_3;
            break;
        }
    // check that the data to write is valid
        if ( (data>8192) | (data<0) ){
                fprintf(stderr,"INVALID DATA TO WRITE - must be between 0 and 8192\n");
                return -1;
        }
    // choose the beam code to write (output appropriate EEPROM address
        temp=beam_code(base,code,radar);

        out8(base+portC0,temp);
    // bit reverse the data
        rdata=reverse_bits(data);
    // apply the data to be written to PortA and PortB on CH1
    // select card to write
        temp=select_card(base,card,radar);
    // reset CH1, PortA and PortB to inputs
        out8(base+cntrl1,0x93);
        usleep(1000);
        out8(base+cntrl1,0x13);
        usleep(1000);
    // verify written data
    // select card to read
        temp=select_card(base,card,radar);
    // read PortA and PortB to see if EEPROM output is same as progammed
        temp=in8(base+portB1);
        temp=temp & 0x1f;
        temp=temp << 8;
        temp=temp + in8(base+portA1);
        if (temp == rdata){
                if(print)
                  printf("    rdata expected: %d rdata read: %d\n", rdata,temp);
                temp=select_card(base,31,radar);
                return 0;
        }
        else {
                printf(" ERROR - Old Card DATA NOT VERIFIED: rdata: %8x != readback: %8x :: Code: %8d Card: %8d\n", rdata, temp,code,card);
                printf(" ERROR - Old Card DATA NOT VERIFIED:  data: %8d != expected: %8d :: Code: %8d Card: %8d\n", reverse_bits(data), reverse_bits(temp),code,card);
                temp=select_card(base,31,radar);
                return -1;
        }
}
/*-READ_DATA---------------------------------------------------------*/
int read_data(unsigned int base,int radar){
#ifdef __QNX__	
	int temp;
        int portA1,portB1,portC1;
        switch(radar) {
          case 1:
            portC1=PC_GRP_1;
            portB1=PB_GRP_1;
            portA1=PA_GRP_1;
            break;
          case 2:
            portC1=PC_GRP_3;
            portB1=PB_GRP_3;
            portA1=PA_GRP_3;
            break;
          case 3:
            portC1=PC_GRP_3;
            portB1=PB_GRP_3;
            portA1=PA_GRP_3;
            break;
        }
    // read PortA and PortB to see if EEPROM output is same as progammed
	temp=in8(base+portB1);
	temp=temp & 0x1f;
	temp=temp << 8;
	temp=temp + in8(base+portA1);

    // bit reverse data
	temp=reverse_bits(temp);

	return temp;
#else
  return 0;
#endif	
}


int main(int argc, char **argv)
{
  char output[40],strout[40];
  char cmd_str[80],prompt_str[10],data_str[1000];
  double *phase[FREQS],*pwr_mag[FREQS];
  double freq[FREQS];
  double pd_old,pd_new,phase_diff=0.0;
  int32 rval,count,sample_count,fail,cr,lf;
  int32 ii,i=0,c=31,data=0,index=0,wait_delay=30; 
  unsigned int b=0;
  int last_collect,current_collect,collect=0,beamcode=0,take_data=0,attempt=0,max_attempts=20;
  double fstart;
  double fstop;
  double fstep;
  int fnum;
  int radar;
  char serial_number[80];
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
      if(atoi(argv[2])==0) setup_flag=0;
      else setup_flag=1;
    } else {
      test_flag=-1000;
    }
    if(argc ==4 ) {
      test_flag=atoi(argv[3]);
      c=atoi(argv[2]); 
    } else {
      test_flag=-1000;
    } 
    radar=atoi(argv[1]);
    printf("Radar: %d Card: %d\n",radar,c);
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
            portC0=PC_GRP_4;
            portC1=PC_GRP_3;
            portB0=PB_GRP_4;
            portB1=PB_GRP_3;
            portA0=PA_GRP_4;
            portA1=PA_GRP_3;
            cntrl0=CNTRL_GRP_4;
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
    /* OPEN THE PLX9656 AND GET LOCAL BASE ADDRESSES */
	fprintf(stderr,"PLX9052 CONFIGURATION ********************\n");
	clock_gettime(CLOCK_REALTIME, &start);
	temp=_open_PLX9052(&pci_handle, &mmap_io_ptr, &IRQ, 1);
	IOBASE=mmap_io_ptr;
	if(temp==-1){
		fprintf(stderr, "	PLX9052 configuration failed");
	}
	else{
		fprintf(stderr, "	PLX9052 configuration successful!\n");
	}
	printf("IOBASE=%x\n",IOBASE);
    /* INITIALIZE THE CARD FOR PROPER IO */
	// GROUP 0 - PortA=output, PortB=output, PortClo=output, PortChi=output
	out8(IOBASE+cntrl0,0x80);
	// GROUP 1 - PortAinput, PortB=input, PortClo=input, PortChi=output
	out8(IOBASE+cntrl1,0x93);
	out8(IOBASE+portA0,0x00);
	out8(IOBASE+portB0,0x00);
	out8(IOBASE+portC0,0x00);
	out8(IOBASE+portA1,0x00);
	out8(IOBASE+portB1,0x00);
	out8(IOBASE+cntrl0,0x00);
	out8(IOBASE+cntrl1,0x13);
	temp=in8(IOBASE+portC1);
	temp=temp & 0x0f;
	printf("input on group 1, port c is %x\n", temp);
        select_card(IOBASE,c,radar);
/*
        while(1) { 
        printf("Set Write\n");
          set_RW(IOBASE,WRITE,radar);
          sleep(1);
        printf("Set Read\n");
          set_RW(IOBASE,READ,radar);
          sleep(1);
        printf("Set switches \n");
          set_SA(IOBASE,SWITCHES,radar);
          sleep(1);
        printf("Set atten\n");
          set_SA(IOBASE,ATTEN,radar);
          sleep(1);
        }
        exit(0);
*/
#endif
if(test_flag==-3) {
  while(1) {
    for(i=0;i<=12;i=i+1) {
   //     b=0;
   //     printf("Selecting Beamcode: %d 0x%x\n",b,b);
   //     beam_code(IOBASE,b,1);
   //     sleep(1);
        if (i==-1) b=0;
        if (i==0) b=1;
        if(i > 0 ) b=1 << i;
        b=b | 0x200 ;
//        b= (b |  0x4);
        printf("Selecting Beamcode: %d 0x%x\n",b,b);
        beam_code(IOBASE,b,1);
        sleep(1);
    }
  }
}
if(test_flag>=0) {
  //stupid_flag=1;
  //printf("test flag %d radar %d\n",test_flag,radar);
  beam_code(IOBASE,test_flag,radar);
  //      temp=write_data_new(IOBASE,c,test_flag,test_flag,radar,0);
  if(NEW_PMAT) temp=verify_data_new(IOBASE,c,test_flag,test_flag,radar,1);
  else temp=verify_data_old(IOBASE,c,test_flag,test_flag,radar,1);
  exit(0);
}
if(test_flag==-2) {
  printf("test flag %d radar %d icard %d verify programming\n",test_flag,radar,c);
  for(b=0;b<=8191;b++) {
//    beam_code(IOBASE,b,radar);
    delay(10);
    if(NEW_PMAT) temp=verify_data_new(IOBASE,c,b,b,radar,0);
    else temp=verify_data_old(IOBASE,c,b,b,radar,1);
  }
  exit(0);
}
/*
if(test_flag==1) {
  beam_code(IOBASE,8191,radar);
  exit(0);
}
*/
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

  if(setup_flag!=0) {
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
  }
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
  printf("\n\nEnter Serial Number: ");
  fflush(stdin);
  fflush(stdout);
  scanf("%s", &serial_number);
  printf("Radar: <%s>  Card: %d Serial: %s\n",radar_name,c,serial_number);
  fflush(stdout);
}
  while((c<CARDS) && (c >=0)) {
  if(test_flag==-1000) {
    sprintf(filename,"%s%s_%s_%02d_%s%s",dir,file_prefix,radar_name,c,serial_number,file_ext);
    if (verbose>0) fprintf(stdout,"Using file: %s\n",filename);
    fflush(stdout);
    gettimeofday(&t0,NULL);
    calfile=fopen(filename,"w");
    count=PHASECODES;
    fwrite(&count,sizeof(int32),1,calfile);
    count=CARDS;
    fwrite(&count,sizeof(int32),1,calfile);
    count=fnum;
    fwrite(&count,sizeof(int32),1,calfile);
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
        temp=write_data_new(IOBASE,c,beamcode,data,radar,0);
        sleep(2); //JDS 
        temp=write_attenuators(IOBASE,c,beamcode,0,radar);
      } else {
        temp=write_data_old(IOBASE,c,beamcode,b,radar);
      }
    }
  }
/*
    printf("Programming all zeros attenuation coding\n");
    for (b=0;b<ATTENCODES;b++) {
      data=0;
      beamcode=b;
      if(NEW_PMAT) {
        //printf("B: %d data: %d BC: %d\n",b,data,beamcode); 
        temp=write_data_new(IOBASE,c,beamcode,data,radar,0);
        temp=write_attenuators(IOBASE,c,beamcode,data,radar);
      } else {
        temp=write_data_old(IOBASE,c,beamcode,data,radar);
      }
    }

    printf("Verifying all zero programming attenuation coding\n");
    for (b=0;b<ATTENCODES;b++) {
      select_card(IOBASE,c,radar);	
      beam_code(IOBASE,b,radar);
      delay(10);
      if(NEW_PMAT) temp=verify_attenuators(IOBASE,c,b,0,radar);
      else temp=verify_data_old(IOBASE,c,b,0,radar,0);
    }
    printf("Programming 1-to-1 attenuation coding no phase\n");
    for (b=0;b<ATTENCODES;b++) {
      data=b;
      beamcode=b;
      if(NEW_PMAT) {
        //printf("B: %d data: %d BC: %d\n",b,data,beamcode); 
        temp=write_data_new(IOBASE,c,beamcode,0,radar,0);
        temp=write_attenuators(IOBASE,c,beamcode,b,radar);
      } else {
        temp=write_data_old(IOBASE,c,beamcode,0,radar);
      }
    }
    printf("Verifying 1-to-1 programming attenuation coding\n");
    for (b=0;b<ATTENCODES;b++) {
      select_card(IOBASE,c,radar);	
      beam_code(IOBASE,b,radar);
      delay(10);
      if(NEW_PMAT) temp=verify_attenuators(IOBASE,c,b,b,radar);
      else temp=verify_data_old(IOBASE,c,b,b,radar,0);
    }

    printf("Programming all ones phase coding\n");
    for (b=0;b<PHASECODES;b++) {
      data=8191;
      beamcode=b;
      if(NEW_PMAT) {
        if(b % 512 == 0 ) printf("B: %d data: %d BC: %d\n",b,data,beamcode); 
        temp=write_data_new(IOBASE,c,beamcode,8191,radar,0);
        temp=write_attenuators(IOBASE,c,beamcode,63,radar);
      } else {
        temp=write_data_old(IOBASE,c,beamcode,8191,radar);
      }
    }
    printf("Verifying all ones programming phase coding\n");
    for (b=0;b<PHASECODES;b++) {
      select_card(IOBASE,c,radar);	
      beam_code(IOBASE,b,radar);
      delay(10);
      if(NEW_PMAT) temp=verify_data_new(IOBASE,c,b,8191,radar,0);
      else temp=verify_data_old(IOBASE,c,b,8191,radar,0);
    }
*/
/*
    printf("Programming 1-to-1 phase coding no attenuation\n");
    for (b=0;b<PHASECODES;b++) {
      data=b;
      beamcode=b;
      if(NEW_PMAT) {
        if(b % 512 == 0 ) printf("B: %d data: %d BC: %d\n",b,data,beamcode); 
        temp=write_data_new(IOBASE,c,beamcode,0,radar,0);
        temp=write_data_new(IOBASE,c,beamcode,data,radar,0);
        temp=write_attenuators(IOBASE,c,beamcode,0,radar);
      } else {
        temp=write_data_old(IOBASE,c,beamcode,b,radar);
      }
    }
    printf("Verifying 1-to-1 programming phase coding\n");
    for (b=0;b<PHASECODES;b++) {
      select_card(IOBASE,c,radar);	
      beam_code(IOBASE,b,radar);
      delay(10);
      if(NEW_PMAT) temp=verify_data_new(IOBASE,c,b,b,radar,0);
      else temp=verify_data_old(IOBASE,c,b,b,radar,0);
    }
*/
/*
    if(verbose> 0 ) {
      gettimeofday(&t3,NULL);
      elapsed=(t3.tv_sec-t2.tv_sec)*1E6;
      elapsed+=(t3.tv_usec-t2.tv_usec);
      printf("  Program Card Elapsed Seconds: %lf\n",(float)elapsed/1E6);
    }
*/
    if(test_flag==-1) {
      exit(0);
    }
    sleep(10);
    if (verbose>0) fprintf(stdout,"Measuring phases\n");
#ifdef _QUICK_
    last_collect=0;
    current_collect=0;
    for (b=0;b<PHASECODES;b++) {
      beamcode=b;
      temp=select_card(IOBASE,c,radar);	
      beam_code(IOBASE,beamcode,radar);
      collect=0;
      if(b==0) collect=1;
      if(b==(PHASECODES-1)) collect=1; 
      if(b==1) collect=1;
      if(b==2) collect=1;
      if(b==4) collect=1;
      if((b % 8)==0) collect=1;
      if (!collect) { 
        printf(":::Card %d::: Skipping BEAMCode %d :: %d\n",c,beamcode,b % 8);
        continue;
      } else {
        printf(":::Card %d::: Reading BEAMCode %d :: %d\n",c,beamcode,b % 8);
        last_collect=current_collect;
        current_collect=b; 
      }
/*
    for (ii=-1;ii<14;ii++) {
      if(ii==-1) b=0;
      else if(ii==13) b=PHASECODES-1;
      else  {
        b=pow(2,ii);
      }
      beamcode=b;
      temp=select_card(IOBASE,c,radar);	
      beam_code(IOBASE,beamcode,radar);
*/
#else
    for (b=0;b<PHASECODES;b++) {
      beamcode=b;
      temp=select_card(IOBASE,c,radar);	
      beam_code(IOBASE,beamcode,radar);
      last_collect=current_collect;
      current_collect=b; 
#endif
      if(NEW_PMAT) temp=verify_data_new(IOBASE,c,b,b,radar,0);
      else temp=verify_data_old(IOBASE,c,b,b,radar,0);
      if (temp!=0) {
        data=b;
        if(NEW_PMAT) {
          if(b % 512 == 0 ) printf("B: %d data: %d BC: %d\n",b,data,beamcode); 
          temp=write_data_new(IOBASE,c,beamcode,0,radar,0);
          temp=write_data_new(IOBASE,c,beamcode,data,radar,0);
          temp=write_attenuators(IOBASE,c,beamcode,0,radar);
        } else {
          temp=write_data_old(IOBASE,c,beamcode,b,radar);
        }
        delay(10);
        if(NEW_PMAT) temp=verify_data_new(IOBASE,c,b,b,radar,0);
        else temp=verify_data_old(IOBASE,c,b,b,radar,0);
      }
      if (temp!=0) {
        printf("Failed Verification for beamcode: %d  %x\n",b,b);
        exit(-1);
      }
//jds    for (index=-1;index<=13;index++) {
//      if(index==-1) b=0;
//      else if(index==13) b=8191;
//      else b=(int)pow(2,index); 
// Start of Command Block
/*
      if(verbose> 0 ) {
        printf(":::Card %d::: BEAMCode %d\n",c,beamcode);
        gettimeofday(&t4,NULL);
      }
*/
      gettimeofday(&t10,NULL);
      //button_command(":SENS1:AVER:CLE\r\n");
      button_command(":INIT1:IMM\r\n");
      if(b==0) sleep(1);
#ifdef __QNX__
      delay(wait_delay); //NO AVERAGE
#else
      usleep(wait_delay);
#endif
/*
    if(verbose> 0 ) {
      gettimeofday(&t11,NULL);
      elapsed=(t11.tv_sec-t10.tv_sec)*1E6;
      elapsed+=(t11.tv_usec-t10.tv_usec);
      printf("  Measure Delay Total Elapsed Seconds: %lf\n",(float)elapsed/1E6);
    }
*/
      take_data=1;
      attempt=0;
      while((take_data) && (attempt<max_attempts)) {
        
        attempt++;
        button_command(":CALC1:PAR1:SEL\r\n");
        mlog_data_command(":CALC1:DATA:FDAT?\r\n",phase,b) ;
        button_command(":CALC1:PAR2:SEL\r\n");
        mlog_data_command(":CALC1:DATA:FDAT?\r\n",pwr_mag,b) ;
        pd_new=phase[fnum-1][b]-phase[0][b];
        if(b!=0) {
          pd_old=phase[fnum-1][last_collect]-phase[0][last_collect];
          phase_diff=fabs(pd_new-pd_old);
          if(phase_diff > 1.0E-6) {
            take_data=0;
          }
          else {  //phase_diff too small
            wait_delay=wait_delay+10;
            printf("Data Collection Error: %d. Increasing delay to %d (ms)\n",b,wait_delay);
            printf("  Phase Diff: %lf : %lf %lf\n",phase_diff,pd_old, pd_new);
            usleep(wait_delay);
          }  
        } else {  //b==0
          take_data=0;
        }
      }
      if(attempt==max_attempts) {
        printf("FATAL ERROR: Phasecode %d took Max attempts %d last delay %d (ms)\n",b,attempt,wait_delay);
        exit(0);
      }
//      printf("End\n");
      printf("Phasecode %d: Freq Index: %d Phase: %lf Pd: %lf Wait Delay: %d\n",b,fnum-1,phase[fnum-1][b], pd_new,wait_delay);
//      exit(0);
/*
      if(verbose> 0 ) {
        gettimeofday(&t5,NULL);
        elapsed=(t5.tv_sec-t4.tv_sec)*1E6;
        elapsed+=(t5.tv_usec-t4.tv_usec);
        printf("  Measure Phasecode: %d Elapsed Seconds: %lf\n",b,(float)elapsed/1E6);
      }
*/
//      for(i=0;i<fnum;i++) {
//        if (verbose > 1) printf("Freq %lf:  Phase %d:%lf \n",freq[i],b,phase[i][b]);
//      }
    } // end of phasecode loop
/*
    if(verbose> 0 ) {
      gettimeofday(&t6,NULL);
      elapsed=(t6.tv_sec-t2.tv_sec)*1E6;
      elapsed+=(t6.tv_usec-t2.tv_usec);
      printf("  All Measurements Total Elapsed Seconds: %lf\n",(float)elapsed/1E6);
    }
*/
//    if(verbose> 0 ) {
//      gettimeofday(&t6,NULL);
//      elapsed=(t6.tv_sec-t2.tv_sec)*1E6;
//      elapsed+=(t6.tv_usec-t2.tv_usec);
//      printf("  Card Calc Total Elapsed Seconds: %lf\n",(float)elapsed/1E6);
//    }
    for(i=0;i<fnum;i++) {
      if (verbose > 1) printf("Freq %lf:  Phase 0:%lf Phase 8191: %lf\n",freq[i],phase[i][0],phase[i][PHASECODES-1]);
      if (verbose > 1) printf("Freq %lf:  Pwr 0:%lf Pwr 8191: %lf\n",freq[i],pwr_mag[i][0],pwr_mag[i][PHASECODES-1]);
      fwrite(&i,sizeof(int32),1,calfile);
      count=fwrite(phase[i],sizeof(double),PHASECODES,calfile);
      count=fwrite(pwr_mag[i],sizeof(double),PHASECODES,calfile);
      //printf("Freq index: %d Count: %d\n",i,count);
    }
    printf("Closing File\n");
    fclose(calfile);
/*
    if(verbose> 0 ) {
      gettimeofday(&t1,NULL);
      elapsed=(t1.tv_sec-t0.tv_sec)*1E6;
      elapsed+=(t1.tv_usec-t0.tv_usec);
      printf("%d Cards %d PhaseCodes, Elapsed Seconds: %lf\n",CARDS,PHASECODES,(float)elapsed/1E6);
    }
*/
    c=-1;
//    if (verbose>0) fprintf(stdout,"Asking for another card:\n");
//    printf("Enter Next Card Number (CTRL-C to exit): ");
//    scanf("%d", &c);
  } // end of Card loop
}

