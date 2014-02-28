#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "_prog_conventions.h"

/*-WRITE8------------------------------------------------------------*/
void write08(unsigned long BASE, unsigned long reg1, signed char val){
	unsigned long val_to_write, val32, temp;
	unsigned long bitmask[4]={0xffffff00, 0xffff00ff, 0xff00ffff, 0x00ffffff};
	int byte;

	byte=reg1%4;
	reg1=reg1-byte;	
	val32=val<<(8*byte);
	temp=*((uint32*)(BASE+reg1));
	val_to_write=temp & bitmask[byte];	
	val_to_write=val_to_write | val32;
	*((uint32*)(BASE+reg1))=val_to_write;
	
}
/*-WRITE16-----------------------------------------------------------*/
void write16(unsigned long BASE, unsigned long reg1, signed short val){
	unsigned long val_to_write, val32, temp;
	unsigned long bitmask[3]={0xffff0000, 0xff0000ff, 0x0000ffff};
	int byte;

	byte=reg1%4;
	reg1=reg1-byte;	
	val32=val<<(8*byte);
	temp=*((uint32*)(BASE+reg1));
	val_to_write=temp & bitmask[byte];	
	val_to_write=val_to_write | val32;
	*((uint32*)(BASE+reg1))=val_to_write;

}
/*-WRITE32-----------------------------------------------------------*/
void write32(unsigned long BASE, unsigned long reg1, signed long val){
	unsigned long val_to_write, val32, temp;
	unsigned long bitmask[1]={0x00000000};
	int byte;
	
	*((uint32*)(BASE+reg1))=val;
}
/*-READ8-------------------------------------------------------------*/
char read08(unsigned long reg1){
	unsigned long *reg;
	signed char val;
	reg=(unsigned long *)reg1;
	val=(char)(*reg);
	return val;
}
/*-READ16------------------------------------------------------------*/
signed short read16(unsigned long reg1){
	unsigned long *reg;
	signed short val;
	reg=(unsigned long *)reg1;
	val=(short)*(reg);
	return val;
}
/*-READ32------------------------------------------------------------*/
long read32(unsigned long reg1){
	unsigned long *reg;
	signed long val;
	reg=(unsigned long *)reg1;
	val=(long)*(reg);
	return val;
}

