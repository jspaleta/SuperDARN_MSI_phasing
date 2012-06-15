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
#define read_lookup_table 1 
#define CARDS 32 
#define PHASECODES 8192 
#define BEAMCODES 8192 
#define MAX_FREQS 300
#define MAX_ANGLES 200
#define SIMPLE_BEAMS 32
int sock=-1;
int verbose=2;
char summaryfile_prefix[20]="beamcodes";
char beamfile_prefix[20]="beamcodes_cal";
char file_ext[20]=".dat";
char filename[120];
char dir[80];
FILE *beamcodefile=NULL;
FILE *beamtablefile=NULL;
FILE *summaryfile=NULL;
char radar_name[80];

struct timeval t0,t1,t2,t3;
unsigned long elapsed;
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
	float	delaylist[]={0.25, 0.45, 0.8, 1.5, 2.75, 5, 8, 15, 25, 45, 80, 140, 250};

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
                fprintf(stderr,"INVALID ATTEN DATA TO WRITE - must be between 0 and 63\n");
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
                printf(" ERROR - ATTEN DATA NOT VERIFIED: data: %x != readback: %x :: Code: %d Card: %d\n", reverse_bits(data), reverse_bits(temp),code,card);
                return -1;
        }
}

/*-VERIFY_CODE--------------------------------------------------------*/
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
                fprintf(stderr,"INVALID DATA TO VERIFY - must be between 0 and 8192\n");
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
    // disable writing
        set_RW(base,READ,radar);
        delay(10);

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
                printf(" ERROR - New Card DATA NOT VERIFIED: data: %x != readback: %x :: Code: %d Card: %d\n", reverse_bits(data), reverse_bits(temp),code,card);
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
        set_WE(base,OFF,radar);
    // reset CH1, PortA and PortB to inputs
        out8(base+cntrl1,0x93);
        out8(base+cntrl1,0x13);
    // disable writing
        set_RW(base,READ,radar);
        delay(10);
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
int write_data_old(unsigned int base, int card, int code, int data,int radar,int print){

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
    // bit reverse the data
        if(print) printf("Data to write %d  ",data);
        data=reverse_bits(data);
        if(print) printf("  reversed: %d\n",data);
    // apply the data to be written to PortA and PortB on CH1
       usleep(1000);
    // set CH1, Port A to lowest 8 bits of data and output on PortA
        temp=data & 0xff;
        out8(base+portA1,temp);
    // set CH0, Port B to upper 5 bits of data and output on PortB
        temp=data & 0x1f00;
        temp=(temp >> 8);
        out8(base+portB1,temp);
        out8(base+cntrl1,0x01);
    // select card to write
        temp=select_card(base,card,radar);
    // select card 31 so that no real card is selected
        temp=select_card(base,31,radar);
    // reset CH1, PortA and PortB to inputs
       usleep(1000);
        out8(base+cntrl1,0x93);
       usleep(1000);
        out8(base+cntrl1,0x13);
       usleep(1000);
    // disable writing (turn off WRITE_ENABLE);
        temp=in8(base+portC0);
        temp=temp & 0xfe;
        out8(base+portC0,temp);
    // verify written data
    // select card to read
        temp=select_card(base,card,radar);
    // read PortA and PortB to see if EEPROM output is same as progammed
        temp=in8(base+portB1);
        temp=temp & 0x1f;
        temp=temp << 8;
        temp=temp + in8(base+portA1);
        if (temp == data){
                if(print)
                  printf("    Code read after writing is %d\n", reverse_bits(temp));
                temp=select_card(base,31,radar);
                return 0;
        }
        else {
                printf(" ERROR - Old Card DATA NOT WRITTEN: data: %x != readback: %x :: Code: %d Card: %d\n", reverse_bits(data), reverse_bits(temp),code,card);
                temp=select_card(base,31,radar);
                return -1;
        }
}
int verify_data_old(unsigned int base, int card, int code, int data,int radar, int print){

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
/*
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
*/
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
        data=reverse_bits(data);
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
        if (temp == data){
                if(print)
                  printf("    data expected: %d data read: %d\n", data,temp);
                temp=select_card(base,31,radar);
                return 0;
        }
        else {
                printf(" ERROR - Old Card DATA NOT VERIFIED: requested data: %d != readback: %d :: Code: %d Card: %d\n", reverse_bits(data), reverse_bits(temp),code,card);
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
  double *pwr_mag[MAX_FREQS];
  double freqs[MAX_FREQS];
  double angles[MAX_ANGLES],std_angles[MAX_ANGLES];
  int lowest_pwr_mag_index[3]={-1,-1,-1}; // freq,card,phasecode
  int *best_phasecode[MAX_FREQS], *best_std_phasecode[MAX_FREQS];
  int *best_attencode[MAX_FREQS], *best_std_attencode[MAX_FREQS];
  int *final_beamcodes[CARDS], *final_attencodes[CARDS];
  double *final_angles[CARDS];
  double *final_freqs[CARDS];
  int a=0,i=0,card=0,c=0,b=0,rval=0,count=0,attempt=0; 
  int num_freqs,num_angles,num_std_angles,num_beamcodes,num_cards;
  int std_angle_index_offset=0,angle_index_offset=192;
  double df=0.0,angle,freq;
  int requested_phasecode=0, requested_attencode=0,beamcode=0;
  int radar,loop=0,read=0;
  unsigned int portA0,portB0,portC0,cntrl0 ;
  unsigned int portA1,portB1,portC1,cntrl1 ;
	int		 temp, pci_handle, j,  IRQ  ;
	unsigned char	 *BASE0, *BASE1;
	unsigned int	 mmap_io_ptr,IOBASE, CLOCK_RES;
	float		 time;
#ifdef __QNX__
	struct		 _clockperiod new, old;
	struct		 timespec start_p, stop_p, start, stop, nsleep;
#endif
    printf("Hmm\n");
    if(argc <2 ) {

      fprintf(stderr,"%s: invoke with radar number (1 or 2)\n",argv[0]);
      exit(0);
    }
    radar=atoi(argv[1]);
    printf("Radar: %d\n",radar);
    if (argc==3) loop=atoi(argv[2]);
    printf("Loop: %d\n",loop);

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
//            sprintf(radar_name,"cve");
//            sprintf(dir,"/root/site_data/site.oregon/calibrations/cve/");
            sprintf(radar_name,"cvw");
            sprintf(dir,"/root/site_data/site.oregon/calibrations/cvw/");
//            sprintf(radar_name,"mcm");
//            sprintf(dir,"/root/site_data/site.mcm/calibrations/mcm/");
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
            sprintf(radar_name,"cvw");
            sprintf(dir,"/root/site_data/site.oregon/calibrations/cvw/");
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
            sprintf(radar_name,"cvw");
            sprintf(dir,"/root/site_data/site.oregon/calibrations/cvw/");
        break;
      default:
        fprintf(stderr,"Invalid radar number %d",radar);
        exit(-1);
    } 
#ifdef __QNX__
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
	temp=in8(IOBASE+portC1);
	temp=temp & 0x0f;
	printf("input on group 1, port c is %x\n", temp);
#endif

  for (c=0;c<CARDS;c++) {
    final_beamcodes[c]=calloc(BEAMCODES,sizeof(int));
    final_attencodes[c]=calloc(BEAMCODES,sizeof(int));
    final_angles[c]=calloc(BEAMCODES,sizeof(double));
    final_freqs[c]=calloc(BEAMCODES,sizeof(double));
    for (b=0;b<BEAMCODES;b++) {
      final_beamcodes[c][b]=-1;
      final_attencodes[c][b]=-1;
      final_angles[c][b]=-1;
      final_freqs[c][b]=-1;
    }
  }
  fflush(stdout);
  fflush(stdin);

  if(loop==0) {
    c=-1; 
    printf("\n\nEnter Beam Number: ");
    fflush(stdin);
    fflush(stdout);
    scanf("%d", &b);
    fflush(stdout);
  } else {
    b=0;
  }
  if(loop) {
    for(b=0;b<=24;b++) {
      printf("Selected Radar: <%s>  Beam: %d\n",radar_name,b);
      printf("Card Select LED (CS) will illuminate for Radar: %s Card: %d\n",radar_name,b % 20);
      beam_code(IOBASE,b,radar);
      select_card(IOBASE,b % 20,radar);
      sleep(5);
    }
  } else {
      printf("Selected Radar: <%s>  Beam: %d\n",radar_name,b);
      printf("Card Select LED (CS) will illuminate for Radar: %s Card: %d\n",radar_name,b % 20);
      beam_code(IOBASE,b,radar);
      select_card(IOBASE,b % 20,radar);
  }
  return 0;
}

