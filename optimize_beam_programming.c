/* 
 * This program is designed to optimize the final timedelay beam programming
 * for MSI phasing cards to correct for frequency variance of expected timedelay
 *
 * This is an iterative procedure for time delay and attenuator switches as they
 * interact with each other (most likely due to small impendenance mismathes in the electronics layout).
 *
 * This program is written to work with the HP vector network analyzer.
 * But it can be rewritten to make use of another automated test equipment rig.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* helper functions  for vna */
#include "vna_functions.h"
/* Useful defines for MSI phasing cards */
#include "MSI_functions.h"

/* variables defined elsewhere */
extern int32_t    MSI_max_angles;
extern int32_t    MSI_phasecodes;
extern int32_t    MSI_num_angles;
extern int32_t    MSI_num_cards;
extern double     MSI_bm_sep_degrees;
extern double     MSI_spacing_meters;
extern double     MSI_max_freq;
extern double     MSI_min_freq;
extern double     MSI_lo_freq;
extern double     MSI_hi_freq;
extern int32_t    MSI_max_freq_steps;
extern double     MSI_freq_window;
extern double     MSI_target_pwr_dB;
extern double     MSI_target_tdelay0_nsecs;
extern double     MSI_tdelay_tolerance_nsec;
extern double     MSI_pwr_tolerance_dB;

extern int32_t    VNA_triggers;
extern int32_t    VNA_wait_delay_ms;
extern int32_t    VNA_min_nave;

int main(int argc, char **argv ) {
     
     int verbose=0;
     int sock=-1;
     int rval;
     int i,c,b,a,f,t,bf,ba,pc,ac;
     struct timespec begin_card, end_card;
     double  time_spent_card;
     struct timespec begin_angle, end_angle;
     double  time_spent_angle;
     struct timespec begin_step, end_step;
     double  time_spent_step;
     int32_t loops_total, loops_done;

     int32_t best_beam_freq_index,best_beam_angle_index,best_phasecode,best_attencode; 
     double best_tdelay,best_pwr;
     int first_card=0,last_card=19;

     double *phase[VNA_FREQS],*pwr_mag[VNA_FREQS], *tdelay[VNA_FREQS];
     double freq[VNA_FREQS];
 
     double middle=(float)(MSI_num_angles-1)/2.0;
     double angles_degrees[MSI_num_angles];
     double freq_center[MSI_max_freq_steps];
     double freq_lo[MSI_max_freq_steps];
     double freq_hi[MSI_max_freq_steps];
     int32_t freq_steps=0;
     int32_t wait_ms=50;

     double timedelay_nsecs,needed_tdelay;
     double td_sum,td_ave,pwr_sum,pwr_ave;
     double td_min,td_max,td_pp;
     double pwr_min,pwr_max,pwr_pp;
     double adelta,tdelta;
     double test_adelta,test_tdelta;
     double asign,tsign;
     int32_t fast_loop,count,wflag,acode_step,pcode_step;
     double fdiff,tdiff;

     int32_t nave,pcode_range,pcode_min,pcode_max;
     int32_t acode_range,acode_min,acode_max;

 
     FILE *beamcodefile=NULL;
     FILE *optbeamcodefile=NULL;
     char *caldir=NULL;
     char dirstub[256]="";
     char filename[512]="";
     char radar_name[16]="ade";
     char vna_host[24]="137.229.27.122";
     char strout[128]="";
     char output[128]="";
     char command[128]="";
     char diocmd[256]="";
     int32_t sshflag=0,iflag=0,rflag=0,rnum=0,port=23;

     double beam_highest_time0_nsec,beam_lowest_pwr_dB,beam_middle;
     int32_t    num_beam_freqs,num_beam_angles,num_beam_steps;
     double     *beam_freqs=NULL;
     double     *beam_angles=NULL;
     double     *beam_requested_delay=NULL;
     double     *beam_needed_delay=NULL;
     double     *beam_freq_lo=NULL;
     double     *beam_freq_hi=NULL;
     double     *beam_freq_center=NULL;
     double     *beam_pwr_dB=NULL;
     double     *beam_tdelay_nsec=NULL;
     int32_t     *beam_attencode=NULL;
     int32_t     *beam_phasecode=NULL;
     int32_t    beam_freq_index;

/* 
*  The optimized arrays are length MSI_phasecodes 
*  and represent the final programming state of 
*  the card
*/ 
     int32_t    opt_mem_offset=MSI_max_angles;
/* code arrays are the final values */
     int32_t     *opt_pcode=NULL;
     int32_t     *opt_acode=NULL;
     int32_t     *opt_qual=NULL;
/* Freq and bm arrays used for self-checking and searching lookup table */ 
     double     *opt_freq_lo=NULL;
     double     *opt_freq_hi=NULL;
     double     *opt_freq_center=NULL;
     double     *opt_bmangle_deg=NULL;
     int32_t     *opt_bmnum=NULL;
/* tdelay and gain arrays used for self-checking of final programming */
     double     *opt_tdelay_ave=NULL;
     double     *opt_tdelay_target=NULL;
     double     *opt_tdelay_min=NULL;
     double     *opt_tdelay_max=NULL;
     double     *opt_gain_ave=NULL;
     double     *opt_gain_target=NULL;
     double     *opt_gain_min=NULL;
     double     *opt_gain_max=NULL;


     while ((rval = getopt (argc, argv, "+r:n:c:a:p:v:ish")) != -1) {
         switch (rval) {
           case 'a':
             snprintf(vna_host,24,"%s",optarg);
             break;
           case 'v':
             verbose=atoi(optarg);
             break;
           case 'n':
             rnum=atoi(optarg);
             break;
           case 'c':
             first_card=atoi(optarg);
             last_card=atoi(optarg);
             break;
           case 'p':
             port=atoi(optarg);
             break;
           case 'r':
             snprintf(radar_name,16,"%s",optarg);
             rflag=1;
             break;
           case 'i':
             iflag=1;
             break;
           case 's':
             sshflag=1;
             break;
           case '?':
             if (optopt == 'r')
               fprintf (stderr, "Option -%c requires an argument.\n", optopt);
             else if (isprint (optopt))
               fprintf (stderr, "Unknown option `-%c'.\n", optopt);
             else
               fprintf (stderr,
                        "Unknown option character `\\x%x'.\n",
                        optopt);
             return 1;
           case 'h':
           default:
               fprintf (stderr,"-r radarname -n dio radar number -a 'ipaddress' -c to run vna cal\n");
               return 1;
         }
     }
     if (argc == 1 || rnum==0 || rflag==0) {
               fprintf (stderr,"Required arguments -r radarname -n dio radar number\n");
               return 1;
     }
     caldir=getenv("MSI_CALDIR");
     if (caldir==NULL) {
          caldir=strdup("/data/calibrations/");
     }
     fprintf(stdout,"CALDIR: %s\n",caldir);
     printf("Radar: <%s>\n",radar_name);
     sprintf(dirstub,"/%s/%s/",caldir,radar_name);
     fprintf(stdout,"RADARDIR: %s\n",dirstub);
     fflush(stdout);
     struct stat s;
     int err = stat(dirstub, &s);
     if(-1 == err) {
        perror("stat");
        exit(1);
     } else {
       if(S_ISDIR(s.st_mode)) {
        /* it's a dir */
       } else {
        /* exists but is no dir */
        perror("stat");
        exit(1);
       }
     }
/* code arrays are the final values */
     opt_pcode          =calloc(MSI_phasecodes,sizeof(int32_t));
     opt_acode          =calloc(MSI_phasecodes,sizeof(int32_t));
     opt_qual           =calloc(MSI_phasecodes,sizeof(int32_t));
/* Freq and bm arrays used for self-checking and searching lookup table */ 
     opt_freq_lo        =calloc(MSI_phasecodes,sizeof(double));
     opt_freq_hi        =calloc(MSI_phasecodes,sizeof(double));
     opt_freq_center    =calloc(MSI_phasecodes,sizeof(double));
     opt_bmnum          =calloc(MSI_phasecodes,sizeof(int32_t));
     opt_bmangle_deg    =calloc(MSI_phasecodes,sizeof(double));
/* tdelay and atten arrays used for self-checking of final programming */
     opt_tdelay_ave     =calloc(MSI_phasecodes,sizeof(double));
     opt_tdelay_target  =calloc(MSI_phasecodes,sizeof(double));
     opt_tdelay_min     =calloc(MSI_phasecodes,sizeof(double));
     opt_tdelay_max     =calloc(MSI_phasecodes,sizeof(double));
     opt_gain_ave       =calloc(MSI_phasecodes,sizeof(double));
     opt_gain_target    =calloc(MSI_phasecodes,sizeof(double));
     opt_gain_min       =calloc(MSI_phasecodes,sizeof(double));
     opt_gain_max       =calloc(MSI_phasecodes,sizeof(double));
/* Initialize the opt arrays with obviously bogus data */
     for(b=0;b<MSI_phasecodes;b++) {
       opt_pcode[b]=MSI_phasecodes-1;
       opt_acode[b]=63;
       opt_qual[b]=-1;
       opt_bmnum[b]=-1;
       opt_bmangle_deg[b]=180.;
       opt_freq_lo[b]=0.0;
       opt_freq_hi[b]=0.0;
       opt_freq_center[b]=0.0;
       opt_tdelay_ave[b]=-1E13;
       opt_tdelay_target[b]=1E13;
       opt_tdelay_min[b]=-1E13;
       opt_tdelay_max[b]=-1E13;
       opt_gain_ave[b]=1E13;
       opt_gain_target[b]=-1E13;
       opt_gain_min[b]=1E13;
       opt_gain_max[b]=1E13;
     }
     for(i=0;i<VNA_FREQS;i++) {
       freq[i]=MSI_min_freq+i*((MSI_max_freq-MSI_min_freq)/(double)(VNA_FREQS-1));
       phase[i]=calloc(MSI_phasecodes,sizeof(double));
       tdelay[i]=calloc(MSI_phasecodes,sizeof(double));
       pwr_mag[i]=calloc(MSI_phasecodes,sizeof(double));
     }


     /* Fill the angles array */
     for(a=0;a<MSI_num_angles;a++) {
               angles_degrees[a]=(a-middle)*MSI_bm_sep_degrees;
     }
     /* Determine the freq steps */
     freq_steps=(MSI_max_freq-MSI_min_freq)/MSI_freq_window+1;
     if (freq_steps > MSI_max_freq_steps) {
       fprintf(stderr,"Number of frequency steps too large for MSI phase card memory to support! Check your MSI frequency windowing settings: min/max_freq and freq_window\n");
       exit(-1);
     }
     freq_lo[0]=MSI_lo_freq;
     freq_hi[0]=MSI_hi_freq;
     freq_center[0]=(freq_hi[0]+freq_lo[0])/2.0;
     if (verbose > 2 ) fprintf(stdout,"%d %8.3lf %8.3lf %8.3lf\n",0,freq_lo[0],freq_center[0],freq_hi[0]);
     for(f=1;f<freq_steps;f++) {
       freq_lo[f]=MSI_min_freq+(f-1)*MSI_freq_window;
       freq_hi[f]=MSI_min_freq+(f)*MSI_freq_window;
       freq_center[f]=(freq_hi[f]+freq_lo[f])/2.0;
       if (verbose > 2 ) fprintf(stdout,"%d %8.3lf %8.3lf %8.3lf\n",f,freq_lo[f],freq_center[f],freq_hi[f]);
     } 
     if(opt_mem_offset+(freq_steps*opt_mem_offset+MSI_num_angles) > MSI_phasecodes) {
            fprintf(stderr,"Overrun of Phasecode space: f: %d a: %d\n",freq_steps,MSI_num_angles);
            exit(1);
     } else {
            fprintf(stdout,"Info:: Angles: %d\n",MSI_num_angles);
            fprintf(stdout,"Info:: Freq Steps: %d\n",freq_steps);
            loops_total=freq_steps*MSI_num_angles;
            fprintf(stdout,"Info:: Optimizations per card: %d\n",loops_total);
            fprintf(stdout,"Info:: Max Card Memory Location: %d\n",opt_mem_offset+(freq_steps*opt_mem_offset+MSI_num_angles));
     } 
     mypause();
     /* Initialize VNA */
     if (verbose>0) fprintf(stdout,"Opening Socket %s %d\n",vna_host,port);
     sock=opentcpsock(vna_host, port);
     if (sock < 0) {
       fprintf(stderr,"Socket failure %d\n",sock);
       exit(-1);
     } else if (verbose>0) fprintf(stdout,"Socket %d\n",sock);
     rval=read(sock, &output, sizeof(char)*10);
     if (verbose>0) fprintf(stdout,"Initial Output Length: %d\n",rval);
     strcpy(strout,"");
     strncat(strout,output,rval);
     if (verbose>0) fprintf(stdout,"Initial Output String: %s\n",strout);
     /* Calibrate the VNA */
     if(iflag) {
       button_command(sock,":SYST:PRES\r\n",10,verbose);
       button_command(sock,":INIT1:CONT ON\r\n",10,verbose);
       button_command(sock,":TRIG:SOUR BUS\r\n",10,verbose);
       sprintf(command,":SENS1:FREQ:STAR %E\r\n",VNA_MIN);
       button_command(sock,command,10,verbose);
       sprintf(command,":SENS1:FREQ:STOP %E\r\n",VNA_MAX);
       button_command(sock,command,10,verbose);
       sprintf(command,":SENS1:SWE:POIN %d\r\n",VNA_FREQS);
       button_command(sock,command,10,verbose);
       button_command(sock,":CALC1:PAR:COUN 3\r\n",10,verbose);

       button_command(sock,":CALC1:PAR1:SEL\r\n",10,verbose);
       button_command(sock,":CALC1:PAR1:DEF S12\r\n",10,verbose);
       button_command(sock,":CALC1:FORM UPH\r\n",10,verbose);

       button_command(sock,":CALC1:PAR2:SEL\r\n",10,verbose);
       button_command(sock,":CALC1:PAR2:DEF S12\r\n",10,verbose);
       button_command(sock,":CALC1:FORM MLOG\r\n",10,verbose);

       button_command(sock,":CALC1:PAR3:SEL\r\n",10,verbose);
       button_command(sock,":CALC1:PAR3:DEF S12\r\n",10,verbose);
       button_command(sock,":CALC1:FORM GDEL\r\n",10,verbose);
       button_command(sock,":CALC1:SMO:APER 5.0\r\n",10,verbose);
       button_command(sock,":CALC1:SMO:STAT ON\r\n",10,verbose);

       button_command(sock,":SENS1:AVER ON\r\n",10,verbose);
       sprintf(command,":SENS1:AVER:COUN 32\r\n");
       button_command(sock,command,10,verbose);
       button_command(sock,":TRIG:SOUR INTERNAL\r\n",10,verbose);


       printf("\n\n\7\7Calibrate Network Analyzer for S21,S12\n");
       mypause();
       button_command(sock,":SENS1:CORR:COLL:METH:THRU 1,2\r\n",10,verbose);
       sleep(1);
       button_command(sock,":SENS1:CORR:COLL:THRU 1,2\r\n",10,verbose);
       printf("  Doing S1,2 Calibration..wait 4 seconds\n");
       sleep(4);

       button_command(sock,":SENS1:CORR:COLL:METH:THRU 2,1\r\n",10,verbose);
       sleep(1);
       button_command(sock,":SENS1:CORR:COLL:THRU 2,1\r\n",10,verbose);
       printf("  Doing S2,1 Calibration..wait 4 seconds\n");
       sleep(4);
       button_command(sock,":SENS1:CORR:COLL:SAVE\r\n",10,verbose);

       button_command(sock,":SENS1:AVER ON\r\n",10,verbose);
       sprintf(command,":SENS1:AVER:COUN %d\r\n",VNA_triggers);
       button_command(sock,command,10,verbose);
       button_command(sock,":TRIG:SOUR BUS\r\n",10,verbose);
       button_command(sock,":SENS1:AVER:CLE\r\n",10,verbose);
       for(t=0;t<VNA_triggers;t++) {
         button_command(sock,":TRIG:SING\r\n",0,verbose);
         button_command(sock,"*OPC?\r\n",0,verbose);
       }


     }
     button_command(sock,":SENS1:AVER:CLE\r\n",10,verbose);
     for(t=0;t<VNA_triggers;t++) {
        button_command(sock,":TRIG:SING\r\n",0 ,verbose );
        button_command(sock,"*OPC?\r\n",0,verbose);
     }
     fprintf(stdout,"\n\nVNA Init Complete\nConfigure for Phasing Card Measurements\n");

     
     for(c=first_card;c<=last_card;c++) {
          fprintf(stdout,"\nPrepare Card : %02d\n",c);
          loops_done=0;
          //mypause();
          /* Inside the card loop */
          if (verbose > 0 ) fprintf(stdout, "  Starting optimization for Card: %d\n",c);
          clock_gettime(CLOCK_MONOTONIC,&begin_card);
          /* Load the beamtable file for this card*/
          sprintf(filename,"%s/beamcodes_cal_%s_%02d.dat",dirstub,radar_name,c);
          beamcodefile=fopen(filename,"r");
          if (beamcodefile!=NULL) {
               if (verbose > -1 ) fprintf(stdout,"    Opened: %s\n",filename);
               fread(&beam_highest_time0_nsec,sizeof(double),1,beamcodefile);
               fread(&beam_lowest_pwr_dB,sizeof(double),1,beamcodefile);
               if(beam_highest_time0_nsec != MSI_target_tdelay0_nsecs) {
                 fprintf(stderr,"Error:: Card %d Target time0 mismatch: %lf  %lf\n",c,beam_highest_time0_nsec,MSI_target_tdelay0_nsecs);
                 exit(-1); 
               }
               if(beam_lowest_pwr_dB != MSI_target_pwr_dB) {
                 fprintf(stderr,"Error:: Card %d Target pwr  mismatch: %lf  %lf\n",c,beam_lowest_pwr_dB, MSI_target_pwr_dB);
                 exit(-1); 
               }
               
               fread(&num_beam_freqs,sizeof(int),1,beamcodefile);
               if(beam_freqs!=NULL) free(beam_freqs);
               beam_freqs=calloc(num_beam_freqs,sizeof(double));
               fread(beam_freqs,sizeof(double),num_beam_freqs,beamcodefile);
               fread(&num_beam_angles,sizeof(int32_t),1,beamcodefile);
               fread(&beam_middle,sizeof(double),1,beamcodefile);
               if(beam_angles!=NULL) free(beam_angles);
               beam_angles=calloc(num_beam_angles,sizeof(double));
               fread(beam_angles,sizeof(double),num_beam_angles,beamcodefile);
               if(beam_requested_delay!=NULL) free(beam_requested_delay);
               beam_requested_delay=calloc(num_beam_angles,sizeof(double));
               fread(beam_requested_delay,sizeof(double),num_beam_angles,beamcodefile);
               if(beam_needed_delay!=NULL) free(beam_needed_delay);
               beam_needed_delay=calloc(num_beam_angles,sizeof(double));
               fread(beam_needed_delay,sizeof(double),num_beam_angles,beamcodefile);
               fread(&num_beam_steps,sizeof(int32_t),1,beamcodefile);
               if(beam_freq_lo!=NULL) free(beam_freq_lo);
               beam_freq_lo=calloc(num_beam_steps+1,sizeof(double));
               if(beam_freq_hi!=NULL) free(beam_freq_hi);
               beam_freq_hi=calloc(num_beam_steps+1,sizeof(double));
               if(beam_freq_center!=NULL) free(beam_freq_center);
               beam_freq_center=calloc(num_beam_steps+1,sizeof(double));

               if(beam_pwr_dB!=NULL) free(beam_pwr_dB);
               beam_pwr_dB=calloc(num_beam_angles*(num_beam_steps+1),sizeof(double));

               if(beam_tdelay_nsec!=NULL) free(beam_tdelay_nsec);
               beam_tdelay_nsec=calloc(num_beam_angles*(num_beam_steps+1),sizeof(double));

               if(beam_phasecode!=NULL) free(beam_phasecode);
               beam_phasecode=calloc(num_beam_angles*(num_beam_steps+1),sizeof(int32_t));

               if(beam_attencode!=NULL) free(beam_attencode);
               beam_attencode=calloc(num_beam_angles*(num_beam_steps+1),sizeof(int32_t));

               for(f=0;f<=num_beam_steps;f++) {
                 fread(&beam_freq_index,sizeof(int32_t),1,beamcodefile);
                 if (f!=beam_freq_index) {
                   fprintf(stderr,"Error:: Read file error! %d %d\n",f,beam_freq_index);
                   exit(-1);
                 } 
                 fread(&beam_freq_lo[f],sizeof(double),1,beamcodefile);
                 fread(&beam_freq_hi[f],sizeof(double),1,beamcodefile);
                 beam_freq_center[f]=(beam_freq_lo[f]+beam_freq_hi[f])/2.0;

               
                 for(a=0;a<num_beam_angles;a++) 
                      fread(&beam_pwr_dB[(f*num_beam_angles)+a],sizeof(double),1,beamcodefile);
                 for(a=0;a<num_beam_angles;a++) 
                      fread(&beam_tdelay_nsec[(f*num_beam_angles)+a],sizeof(double),1,beamcodefile);
                 for(a=0;a<num_beam_angles;a++) 
                      fread(&beam_attencode[(f*num_beam_angles)+a],sizeof(int32_t),1,beamcodefile);
                 for(a=0;a<num_beam_angles;a++)
                      fread(&beam_phasecode[(f*num_beam_angles)+a],sizeof(int32_t),1,beamcodefile);
                  
                 if (verbose > 1 ) fprintf(stdout,"    Findex: %5d :: %-8.5e  %-8.5e Hz\n", f,beam_freq_lo[f],beam_freq_hi[f]);
               } 

               fclose(beamcodefile); 
          } else {
               fprintf(stdout,"    Warning::  Failed to Open: %s\n",filename);
               num_beam_freqs=0;
               num_beam_steps=0;
               num_beam_angles=0;
          }
          for(a=0;a<MSI_num_angles;a++) {
               clock_gettime(CLOCK_MONOTONIC,&begin_angle);
               timedelay_nsecs=MSI_timedelay_needed(angles_degrees[a],MSI_spacing_meters,c);
               needed_tdelay=timedelay_nsecs+MSI_target_tdelay0_nsecs;
               tdiff=1E13;
               best_beam_angle_index=-1;
               for(ba=0;ba<num_beam_angles;ba++) {
                    if ( fabs(beam_needed_delay[ba]-timedelay_nsecs) < tdiff ) {
                         best_beam_angle_index=ba;
                         tdiff=fabs(beam_needed_delay[ba]-timedelay_nsecs);
                    }  
               } 
               if (verbose > -1 ) fprintf(stdout,"    -------\n");  
               if (verbose > -1 ) fprintf(stdout, "    Angle Index: %5d : %lf [deg] :: %8.4lf [nsec]\n",a,angles_degrees[a],timedelay_nsecs);
               for(f=0;f<freq_steps;f++) {
                    clock_gettime(CLOCK_MONOTONIC,&begin_step);
                    if(f==0) {
                      best_beam_freq_index=0;
                    } else {
                      fdiff=1E13;
                      best_beam_freq_index=-1;
                      for(bf=1;bf<=num_beam_steps;bf++) {
                        if ( fabs(beam_freq_center[bf]-freq_center[f]) < fdiff ) {
                          best_beam_freq_index=bf;
                          fdiff=fabs(beam_freq_center[bf]-freq_center[f]);
                        }  
                      }
                    } 
                    if((best_beam_angle_index < 0) || (best_beam_freq_index < 0)) {
                      best_tdelay=needed_tdelay;
                      best_pwr=10.0-MSI_target_pwr_dB;
                      best_phasecode=MSI_phasecode(needed_tdelay);
                      best_attencode=MSI_attencode(10.0-MSI_target_pwr_dB);
                    } else {
                      best_tdelay=beam_tdelay_nsec[(best_beam_freq_index*num_beam_angles)+best_beam_angle_index];
                      best_pwr=beam_pwr_dB[(best_beam_freq_index*num_beam_angles)+best_beam_angle_index];
                      best_phasecode=beam_phasecode[(best_beam_freq_index*num_beam_angles)+best_beam_angle_index];
                      best_attencode=beam_attencode[(best_beam_freq_index*num_beam_angles)+best_beam_angle_index];
                    }
                    b=f*opt_mem_offset+a;
                    if (f>0) b+=opt_mem_offset;
                    if (verbose > -1 ){ 
                      fprintf(stdout,"      >>>>>>>\n");  
                      fprintf(stdout,"      Optimize Start:: MemLoc: %5d Q: %5d Freq Step: %5d : %-08.5e - %-08.5e [Hz]\n", b,opt_qual[b],f,freq_lo[f],freq_hi[f]);
                      fprintf(stdout,"        Initial phasecode: %5d  acode: %5d\n",best_phasecode,best_attencode);
                      fprintf(stdout,"        Initial tdelay: %lf [ns] atten: %lf [dB]\n",best_tdelay,best_pwr);
                      fflush(stdout);
                    } 

                    /* Take a measurement at best phasecode and acode */
                    rval=take_data(sock,b,rnum,c,best_phasecode,best_attencode,pwr_mag,phase,tdelay,wait_ms,wait_ms,sshflag,verbose);
                    if(rval!=0) exit(rval);
                    td_sum=0.0;
                    pwr_sum=0.0;
                    nave=0; 
                    for(i=0;i<VNA_FREQS;i++) {
                      if((freq[i] >= freq_lo[f]) && (freq[i] <=freq_hi[f])){
                        td_sum+=tdelay[i][b];
                        pwr_sum+=pwr_mag[i][b];
                        nave++; 
                      }
                    }
                    if(nave>=VNA_min_nave) {
                      td_ave=td_sum/nave;
                      pwr_ave=pwr_sum/nave;
                    } else {
                      td_ave=-1E13;
                      pwr_ave=-1E13;
                      fprintf(stdout,"        Warning:: low nave: %d < desired: %d >. Consider reconfiguring VNA or freq windows\n",nave,VNA_min_nave);
                    }
                    if (verbose > 1 ){ 
                      fprintf(stdout,"        Needed tdelay: %13.4lf (ns) Ave tdelay: %13.4lf (ns)\n",needed_tdelay,td_ave*1E9); 
                      fprintf(stdout,"        Target Gain: %13.4lf (dB)   Ave Gain: %13.4lf (dB)\n",MSI_target_pwr_dB,pwr_ave); 
                    }
                    fast_loop=0; 
                    while(fast_loop < 3 ) {
                      adelta=fabs(MSI_target_pwr_dB-pwr_ave);
                      if (adelta > 0) asign=(MSI_target_pwr_dB-pwr_ave)/adelta;
                      else asign=1;
                      /* First lets do an attempt at quick optimization */ 
                      if(adelta <= MSI_pwr_tolerance_dB) {  
                        fprintf(stdout,"        Optimizing attencode skipped\n"); 
                      } else {
                        fprintf(stdout,"        Try fast attencode optimization....\n"); 
                        test_adelta=adelta;
                        count=0;
                        wflag=1;
                        ac=best_attencode;
                        while(wflag==1) {
                          count++;
                          acode_range=MSI_attencode(adelta);
                          acode_step=asign*acode_range;
                          ac=ac-acode_step;
                          if (ac < 0 ) ac=0;
                          if (ac > 63 ) ac=63;
                          rval=take_data(sock,b,rnum,c,best_phasecode,ac,pwr_mag,phase,tdelay,wait_ms,sshflag,verbose);
                          if(rval!=0) exit(rval);
                          pwr_sum=0.0;
                          nave=0; 
                          for(i=0;i<VNA_FREQS;i++) {
                            if((freq[i] >= freq_lo[f]) && (freq[i] <=freq_hi[f])){
                              pwr_sum+=pwr_mag[i][b];
                              nave++; 
                            }
                          }
                          if(nave>0) {
                            pwr_ave=pwr_sum/nave;
                          } else {
                            pwr_ave=-1E13;
                          } 
                          adelta=fabs(MSI_target_pwr_dB-pwr_ave);
                          if (adelta > 0) asign=(MSI_target_pwr_dB-pwr_ave)/adelta;
                          else asign=1;
                          /* While Loop Test conditions */
                          if (adelta <= test_adelta) {
                            test_adelta=adelta;
                            best_attencode=ac;
                            best_pwr=pwr_ave;
                          } else {
                            fprintf(stderr," Info:: Gain error got worse! %d :: best: %13.4lf current: %13.4lf\n",count,test_adelta,adelta);
                          }
                          if (count > 8 ) {
                            fprintf(stderr," Info:: Count limit reached %d. Move on to brute force. \n",count);
                            wflag=0;
                          }
                          if(adelta <= MSI_pwr_tolerance_dB) {  
                            fprintf(stderr," Info:: Reached Gain tolerance: %d. Skipping brute force.\n",count);
                            wflag=0;
                          }
                        }
                        adelta=test_adelta;
                      }   
                      fprintf(stdout,"       Gain [dB] :: Target: %13.4lf Measured: %13.4lf :: Tol: %13.4lf\n",MSI_target_pwr_dB,pwr_ave,MSI_pwr_tolerance_dB); 

                      /* Brute force Optimization of attenuation */
                      if(adelta > MSI_pwr_tolerance_dB) {  
                        fprintf(stdout,"        Optimizing attencode.... %d measurements needed\n",acode_range); 
                        acode_range=MSI_attencode(2*adelta);
                        acode_min=best_attencode-acode_range;
                        acode_max=best_attencode+acode_range;
		        if(acode_min < 0) acode_min=0; 
                        if(acode_max > 63) acode_max=63; 
                        for(ac=acode_min;ac<=acode_max;ac++) {
                          rval=take_data(sock,b,rnum,c,best_phasecode,ac,pwr_mag,phase,tdelay,wait_ms,sshflag,verbose);
                          if(rval!=0) exit(rval);
                          pwr_sum=0.0;
                          nave=0; 
                          for(i=0;i<VNA_FREQS;i++) {
                            if((freq[i] >= freq_lo[f]) && (freq[i] <=freq_hi[f])){
                              pwr_sum+=pwr_mag[i][b];
                              nave++; 
                            }
                          }
                          if(nave>0) {
                            pwr_ave=pwr_sum/nave;
                          } else {
                            pwr_ave=-1E13;
                          }
                          if(fabs(MSI_target_pwr_dB-pwr_ave) < adelta) {
                            adelta=fabs(MSI_target_pwr_dB-pwr_ave);
                            best_attencode=ac;
                            best_pwr=pwr_ave;
                          } 
                        } 
                      } 
                      if (verbose > 1 ){ 
                        fprintf(stdout,"        Optimized acode: %d %lf %lf\n",best_attencode,best_pwr,MSI_target_pwr_dB); 
                      }

                      rval=take_data(sock,b,rnum,c,best_phasecode,best_attencode,pwr_mag,phase,tdelay,wait_ms,sshflag,verbose);
                      if(rval!=0) exit(rval);
                      td_sum=0.0;
                      pwr_sum=0.0;
                      nave=0; 
                      for(i=0;i<VNA_FREQS;i++) {
                        if((freq[i] >= freq_lo[f]) && (freq[i] <=freq_hi[f])){
                          td_sum+=tdelay[i][b];
                          pwr_sum+=pwr_mag[i][b];
                          nave++; 
                        }
                      }
                      if(nave>0) {
                        td_ave=td_sum/nave;
                        pwr_ave=pwr_sum/nave;
                      } else {
                        td_ave=-1E13;
                        pwr_ave=-1E13;
                      }
                      if (verbose > 1 ){ 
                        fprintf(stdout,"        Needed tdelay: %13.4lf (ns) Ave tdelay: %13.4lf (ns)\n",needed_tdelay,td_ave*1E9); 
                        fprintf(stdout,"        Target Gain: %13.4lf (dB)   Ave Gain: %13.4lf (dB)\n",MSI_target_pwr_dB,pwr_ave); 
                      }

                      tdelta=fabs(needed_tdelay-td_ave*1E9);
                      if (tdelta > 0) tsign=(needed_tdelay-td_ave*1E9)/tdelta;
                      else tsign=1;
                      fprintf(stdout,"       Delay [ns] :: Target: %13.4lf Measured: %13.4lf :: Tol: %13.4lf\n",needed_tdelay,td_ave*1E9,MSI_tdelay_tolerance_nsec); 

                      /* First lets do an attempt at quick optimization */ 
                      if(tdelta <= MSI_tdelay_tolerance_nsec) {  
                        fprintf(stdout,"        Optimizing phasecode skipped\n",pcode_range); 
                      } else {
                        fprintf(stdout,"        Try fast phasecode optimization....\n"); 
                        test_tdelta=tdelta;
                        test_adelta=adelta;
                        count=0;
                        wflag=1;
                        pc=best_phasecode;
                        while(wflag==1) {
                          count++;
                          pcode_range=MSI_phasecode(tdelta);
                          pcode_step=tsign*pcode_range;
                          pc=pc+pcode_step;
                          if (pc < 0 ) pc=0;
                          if (pc >= MSI_phasecodes  ) pc=MSI_phasecodes-1;
                          rval=take_data(sock,b,rnum,c,pc,best_attencode,pwr_mag,phase,tdelay,wait_ms,sshflag,verbose);
                          if(rval!=0) exit(rval);
                          td_sum=0.0;
                          pwr_sum=0.0;
                          nave=0; 
                          for(i=0;i<VNA_FREQS;i++) {
                            if((freq[i] >= freq_lo[f]) && (freq[i] <=freq_hi[f])){
                              td_sum+=tdelay[i][b];
                              pwr_sum+=pwr_mag[i][b];
                              nave++; 
                            }
                          }
                          if(nave>0) {
                            td_ave=td_sum/nave;
                            pwr_ave=pwr_sum/nave;
                          } else {
                            td_ave=-1E13;
                          } 
                          tdelta=fabs(needed_tdelay-td_ave*1E9);
                          if (tdelta > 0) tsign=(needed_tdelay-td_ave*1E9)/tdelta;
                          else tsign=1;
                          /* While Loop Test conditions */
                          if (tdelta < test_tdelta) {
                            test_tdelta=tdelta;
                            test_adelta=fabs(MSI_target_pwr_dB-pwr_ave);
                            best_pwr=pwr_ave;
                            best_phasecode=pc;
                            best_tdelay=td_ave*1E9;
                            fprintf(stderr," Info:: New best Tdelay! %d :: best: %13.4lf code: %8d delta: %13.4E\n",count,best_tdelay,best_phasecode,test_tdelta);
                          } else {
                            fprintf(stderr," Info:: Tdelay error got worse! %d :: best: %13.4lf current: %13.4lf\n",count,test_tdelta,tdelta);
                          }
                          if (count > 8 ) {
                            fprintf(stderr," Info:: Count limit reached %d. Move on to brute force. \n",count);
                            wflag=0;
                          }
                          if(test_tdelta <= MSI_tdelay_tolerance_nsec) {  
                            fprintf(stderr," Info:: Reached Tdelay tolerance: %d. Skipping brute force.\n",count);
                            wflag=0;
                          }
                        }
                        tdelta=test_tdelta;
                        adelta=test_adelta;
                      }  
                      fprintf(stdout,"          Fast Optimized pcode: %5d :: tdelay [ns]:: Measured: %13.4lf Needed: %13.4lf tdelta: %13.4lf\n",best_phasecode,best_tdelay,needed_tdelay,tdelta); 
                      fprintf(stdout,"          Fast Optimized acode: %5d :: Gain   [dB]:: Measured: %13.4lf Needed: %13.4lf adelta: %13.4lf\n",best_attencode,best_pwr,MSI_target_pwr_dB,adelta,test_adelta); 
                      if(adelta > MSI_pwr_tolerance_dB) {
                        fprintf(stdout,"          Redo Fast Optimization: %d\n",fast_loop); 
                        fast_loop++;
                        continue;
                      } else {
                        break;
                      }
                    }
                    if(adelta > MSI_pwr_tolerance_dB) {
                      fprintf(stderr,"Warning:: Fast optimization of gain failed to reach tolerance\n");
                      fprintf(stderr,"  acode: %5d :: Gain   [dB]:: Measured: %13.4lf Needed: %13.4lf adelta: %13.4lf\n",best_attencode,best_pwr,MSI_target_pwr_dB,adelta,test_adelta); 
                    }
                    /* Okay Brute Force optimization now */
                    if(tdelta > MSI_tdelay_tolerance_nsec) {  
                      pcode_range=MSI_phasecode(2*tdelta);
                      if(needed_tdelay < td_ave*1E9) {
                        pcode_min=best_phasecode-pcode_range;
                        pcode_max=best_phasecode+1;
                      } else {
                        pcode_min=best_phasecode-1;
                        pcode_max=best_phasecode+pcode_range;
                      }

		      if(pcode_min < 0) pcode_min=0; 
                      if(pcode_max >= MSI_phasecodes) pcode_max=MSI_phasecodes-1; 
                      fprintf(stdout,"        Optimizing phasecode.... %d measurements needed\n",pcode_range); 

                      for(pc=pcode_min;pc<=pcode_max;pc++) {
                        rval=take_data(sock,b,rnum,c,pc,best_attencode,pwr_mag,phase,tdelay,wait_ms,sshflag,verbose);
                        if(rval!=0) exit(rval);
                        td_sum=0.0;
                        nave=0; 
                        for(i=0;i<VNA_FREQS;i++) {
                          if((freq[i] >= freq_lo[f]) && (freq[i] <=freq_hi[f])){
                            td_sum+=tdelay[i][b];
                            nave++; 
                          }
                        }
                        if(nave>0) {
                          td_ave=td_sum/nave;
                        } else {
                          td_ave=-1E13;
                        }
                        if(fabs(needed_tdelay-td_ave*1E9) < tdelta) {
                          tdelta=fabs(needed_tdelay-td_ave*1E9);
                          best_phasecode=pc;
                          best_tdelay=td_ave*1E9;
                        } 
                      } 
                    }

                    rval=take_data(sock,b,rnum,c,best_phasecode,best_attencode,pwr_mag,phase,wait_ms,tdelay,sshflag,verbose);
                    if(rval!=0) exit(rval);
                    td_sum=0.0;
                    td_min=1E13;
                    td_max=-1E13;
                    td_pp=-2E13;
                    pwr_sum=0.0;
                    pwr_min=1E13;
                    pwr_max=-1E13;
                    pwr_pp=-2E13;
                    nave=0; 
                    for(i=0;i<VNA_FREQS;i++) {
                      if((freq[i] >= freq_lo[f]) && (freq[i] <=freq_hi[f])){
                        if(tdelay[i][b] < td_min) td_min=tdelay[i][b]; 
                        if(tdelay[i][b] > td_max) td_max=tdelay[i][b]; 
                        if(pwr_mag[i][b] < pwr_min) pwr_min=pwr_mag[i][b]; 
                        if(pwr_mag[i][b] > pwr_max) pwr_max=pwr_mag[i][b]; 
                        td_sum+=tdelay[i][b];
                        pwr_sum+=pwr_mag[i][b];
                        nave++; 
                      }
                    }
                    td_pp=td_max-td_min; 
                    pwr_pp=pwr_max-pwr_min; 
                    if(nave>0) {
                      td_ave=td_sum/nave;
                      pwr_ave=pwr_sum/nave;
                    } else {
                      td_ave=-1E13;
                      pwr_ave=-1E13;
                    }
                    best_tdelay=td_ave*1E9;
                    best_pwr=pwr_ave;
                    tdelta=fabs(needed_tdelay-best_tdelay);
                    adelta=fabs(MSI_target_pwr_dB-best_pwr);

                    loops_done++;
                    clock_gettime(CLOCK_MONOTONIC,&end_step);
                    time_spent_step=(end_step.tv_sec-begin_step.tv_sec)+1E-9*(end_step.tv_nsec-begin_step.tv_nsec);
                    time_spent_card=(end_step.tv_sec-begin_card.tv_sec)+1E-9*(end_step.tv_nsec-begin_card.tv_nsec);
                    fprintf(stdout,"          Optimized pcode: %5d :: tdelay [ns]:: Measured: %13.4lf Needed: %13.4lf\n",best_phasecode,best_tdelay,needed_tdelay); 
                    fprintf(stdout,"          Optimized acode: %5d :: Gain   [dB]:: Measured: %13.4lf Needed: %13.4lf\n",best_attencode,best_pwr,MSI_target_pwr_dB); 

                    if(opt_qual[b]!=-1) {
                      fprintf(stdout,"Warning::: We are overwriting a memory location already used\n");
                      fprintf(stdout,"           MemLoc: %5d Q: %5d Angle: %5d Freq Step: %5d : %-08.5e - %-08.5e [Hz]\n", b, opt_qual[b],a, f,opt_freq_lo[b],opt_freq_hi[b]);
                      fprintf(stdout,"           tdelay [ns]:: Min: %13.4lf Max: %13.4lf Ave: %13.4lf\n",opt_tdelay_min[b],opt_tdelay_max[b],opt_tdelay_ave[b]);
                      fprintf(stdout,"           gain   [dB]:: Min: %13.4lf Max: %13.4lf Ave: %13.4lf\n",opt_gain_min[b],opt_gain_max[b],opt_gain_ave[b]);
                      //mypause();
                    }         

                    opt_pcode[b]=best_phasecode;
                    opt_acode[b]=best_attencode;
                    opt_qual[b]=1;
                    opt_freq_lo[b]=freq_lo[f];
                    opt_freq_hi[b]=freq_hi[f];
                    opt_freq_center[b]=freq_center[f];
                    opt_tdelay_ave[b]=best_tdelay;
                    opt_gain_ave[b]=best_pwr;
                    opt_tdelay_max[b]=td_max*1E9;
                    opt_gain_max[b]=pwr_max;
                    opt_tdelay_min[b]=td_min*1E9;
                    opt_gain_min[b]=pwr_min;
                    opt_tdelay_target[b]=needed_tdelay;
                    opt_gain_target[b]=MSI_target_pwr_dB;
                    opt_bmnum[b]=a;
                    opt_bmangle_deg[b]=angles_degrees[a];


                    fprintf(stdout,"      -------\n");  
                    fprintf(stdout,"      Optimize End:: MemLoc: %5d Q: %5d Angle: %5d Freq Step: %5d : %-08.5e - %-08.5e [Hz]\n", b,opt_qual[b],a, f,opt_freq_lo[b],opt_freq_hi[b]);
                    fprintf(stdout,"          pcode: %d  acode %d\n",opt_pcode[b],opt_acode[b]); 
                    fprintf(stdout,"          Needed tdelay: %13.4lf (ns) Opt tdelay: %13.4lf (ns) Delta: %13.4lf (ns)\n",opt_tdelay_target[b],opt_tdelay_ave[b],tdelta); 
                    fprintf(stdout,"            tdelay [ns]:: Min: %13.4lf Max: %13.4lf P2P: %13.4lf\n",opt_tdelay_min[b],opt_tdelay_max[b],td_pp*1E9); 
                    fprintf(stdout,"          Target   Gain: %13.4lf (dB)   Opt Gain: %13.4lf (dB) Delta: %13.4lf (dB)\n",opt_gain_target[b],opt_gain_ave[b],adelta); 
                    fprintf(stdout,"            gain   [dB]:: Min: %13.4lf Max: %13.4lf P2P: %13.4lf\n",opt_gain_min[b],opt_gain_max[b],pwr_pp); 
                    fprintf(stdout,"      <<<<<<<\n");  
                    fprintf(stdout,"      Step Elapsed  :: %13.3lf\n", time_spent_step);
                    fprintf(stdout,"      Card Elapsed  :: %13.3lf\n", time_spent_card);
                    fprintf(stdout,"      Card Estimate :: %13.3lf\n", time_spent_card*loops_total/loops_done);
                    if((adelta > MSI_pwr_tolerance_dB*1.5) || (tdelta > MSI_tdelay_tolerance_nsec*1.5)) {  
                      opt_qual[b]=2;
                      fprintf(stdout,"Warning::: tolerance exceeded\n");
                      //mypause();
                    } 
               }
               clock_gettime(CLOCK_MONOTONIC,&end_angle);
               time_spent_step=(end_angle.tv_sec-begin_angle.tv_sec)+1E-9*(end_angle.tv_nsec-begin_angle.tv_nsec);
               time_spent_card=(end_angle.tv_sec-begin_card.tv_sec)+1E-9*(end_angle.tv_nsec-begin_card.tv_nsec);
               fprintf(stdout,"    -------\n");  
               fprintf(stdout, "    Angle Elapsed  :: %13.3lf\n", time_spent_angle);
               fprintf(stdout, "    Card Elapsed   :: %13.3lf\n", time_spent_card);
               fprintf(stdout, "    Card Estimate  :: %13.3lf\n", time_spent_card*loops_total/loops_done);
          }
          clock_gettime(CLOCK_MONOTONIC,&end_card);
          time_spent_card=(end_card.tv_sec-begin_card.tv_sec)+1E-9*(end_card.tv_nsec-begin_card.tv_nsec);
          fprintf(stdout, "  Card Elapsed   :: %13.3lf\n", time_spent_card);
          /* Let's tag some upper memory values with diagnostic settings */
          b=8191;
          if (b >=MSI_phasecodes) {
            fprintf(stderr,"Error! Selected bad MemLoc: %d (max: %d)\n",b,MSI_phasecodes-1);
          } else {
           
            if(opt_qual[b]!=-1) {
                      fprintf(stderr,"Warning::: We are overwriting a memory location already used\n");
                      fprintf(stderr,"           Card: %5d MemLoc: %5d Q: %5d Angle: %5d Freq Step: %5d : %-08.5e - %-08.5e [Hz]\n", c, b, opt_qual[b],a, f,opt_freq_lo[b],opt_freq_hi[b]);
                      fprintf(stderr,"           tdelay [ns]:: Min: %13.4lf Max: %13.4lf Ave: %13.4lf\n",opt_tdelay_min[b],opt_tdelay_max[b],opt_tdelay_ave[b]);
                      fprintf(stderr,"           gain   [dB]:: Min: %13.4lf Max: %13.4lf Ave: %13.4lf\n",opt_gain_min[b],opt_gain_max[b],opt_gain_ave[b]);
            }
            opt_pcode[b]=8191;
            opt_acode[b]=63;
            opt_qual[b]=0;
            b=8190;
            if(opt_qual[b]!=-1) {
                      fprintf(stderr,"Warning::: We are overwriting a memory location already used\n");
                      fprintf(stderr,"           Card: %5d MemLoc: %5d Q: %5d Angle: %5d Freq Step: %5d : %-08.5e - %-08.5e [Hz]\n", c, b, opt_qual[b],a, f,opt_freq_lo[b],opt_freq_hi[b]);
                      fprintf(stderr,"           tdelay [ns]:: Min: %13.4lf Max: %13.4lf Ave: %13.4lf\n",opt_tdelay_min[b],opt_tdelay_max[b],opt_tdelay_ave[b]);
                      fprintf(stderr,"           gain   [dB]:: Min: %13.4lf Max: %13.4lf Ave: %13.4lf\n",opt_gain_min[b],opt_gain_max[b],opt_gain_ave[b]);
            }
            opt_pcode[b]=0;
            opt_acode[b]=0;
            opt_qual[b]=0;
            b=8189;
            if(opt_qual[b]!=-1) {
                      fprintf(stderr,"Warning::: We are overwriting a memory location already used\n");
                      fprintf(stderr,"           Card: %5d MemLoc: %5d Q: %5d Angle: %5d Freq Step: %5d : %-08.5e - %-08.5e [Hz]\n", c, b, opt_qual[b],a, f,opt_freq_lo[b],opt_freq_hi[b]);
                      fprintf(stderr,"           tdelay [ns]:: Min: %13.4lf Max: %13.4lf Ave: %13.4lf\n",opt_tdelay_min[b],opt_tdelay_max[b],opt_tdelay_ave[b]);
                      fprintf(stderr,"           gain   [dB]:: Min: %13.4lf Max: %13.4lf Ave: %13.4lf\n",opt_gain_min[b],opt_gain_max[b],opt_gain_ave[b]);
            }
            opt_pcode[b]=0;
            opt_acode[b]=63;
            opt_qual[b]=0;
            b=8188;
            if(opt_qual[b]!=-1) {
                      fprintf(stderr,"Warning::: We are overwriting a memory location already used\n");
                      fprintf(stderr,"           Card: %5d MemLoc: %5d Q: %5d Angle: %5d Freq Step: %5d : %-08.5e - %-08.5e [Hz]\n", c, b, opt_qual[b],a, f,opt_freq_lo[b],opt_freq_hi[b]);
                      fprintf(stderr,"           tdelay [ns]:: Min: %13.4lf Max: %13.4lf Ave: %13.4lf\n",opt_tdelay_min[b],opt_tdelay_max[b],opt_tdelay_ave[b]);
                      fprintf(stderr,"           gain   [dB]:: Min: %13.4lf Max: %13.4lf Ave: %13.4lf\n",opt_gain_min[b],opt_gain_max[b],opt_gain_ave[b]);
            }
            opt_pcode[b]=8191;
            opt_acode[b]=0;
            opt_qual[b]=0;
          } 
          /* Let's save these optimized settings to a file so we can reuse them */
          sprintf(filename,"%s/optcodes_cal_%s_%02d.dat",dirstub,radar_name,c);
          optbeamcodefile=fopen(filename,"w");
          if (optbeamcodefile!=NULL) {
            if (verbose > -1 ) fprintf(stdout,"    Writing Optimized values to file for card: %d\n",c);
            if (verbose > -1 ) fprintf(stdout,"      Opened: %s\n",filename);
            /* Length of the arrays */
            fwrite(&MSI_phasecodes,  sizeof(int32_t),1,optbeamcodefile);
            /* memory offset to start of narrow freq band */
            fwrite(&opt_mem_offset,  sizeof(int32_t),1,optbeamcodefile);
            /* Number of angles programmed*/
            fwrite(&MSI_num_angles,  sizeof(int32_t),1,optbeamcodefile);
            /* Number of freq steps programmed
            *    freq_steps=(MSI_max_freq-MSI_min_freq)/MSI_freq_window+1;
            *    first step is wide band average, 
            *    sequent steps are narrow MSI_freq_window ave
            */ 
            fwrite(&freq_steps,      sizeof(int32_t),1,optbeamcodefile);
            /* Min frequency considered */
            fwrite(&MSI_min_freq,    sizeof(double),1,optbeamcodefile);
            /* Min frequency considered */
            fwrite(&MSI_max_freq,    sizeof(double),1,optbeamcodefile);
            /* Narrow frequency window considered */
            fwrite(&MSI_freq_window, sizeof(double),1,optbeamcodefile);

            /* Now the arrays */
            fwrite(opt_pcode,        sizeof(int32_t),MSI_phasecodes,optbeamcodefile);
            fwrite(opt_acode,        sizeof(int32_t),MSI_phasecodes,optbeamcodefile);
            fwrite(opt_qual,         sizeof(int32_t),MSI_phasecodes,optbeamcodefile);
            fwrite(opt_bmnum,        sizeof(int32_t),MSI_phasecodes,optbeamcodefile);
            fwrite(opt_bmangle_deg,  sizeof(double), MSI_phasecodes,optbeamcodefile);
            fwrite(opt_freq_lo,      sizeof(double), MSI_phasecodes,optbeamcodefile);
            fwrite(opt_freq_hi,      sizeof(double), MSI_phasecodes,optbeamcodefile);
            fwrite(opt_freq_center,  sizeof(double), MSI_phasecodes,optbeamcodefile);
            fwrite(opt_tdelay_ave,   sizeof(double), MSI_phasecodes,optbeamcodefile);
            fwrite(opt_tdelay_target,sizeof(double), MSI_phasecodes,optbeamcodefile);
            fwrite(opt_tdelay_min,   sizeof(double), MSI_phasecodes,optbeamcodefile);
            fwrite(opt_tdelay_max,   sizeof(double), MSI_phasecodes,optbeamcodefile);
            fwrite(opt_gain_ave,     sizeof(double), MSI_phasecodes,optbeamcodefile);
            fwrite(opt_gain_target,  sizeof(double), MSI_phasecodes,optbeamcodefile);
            fwrite(opt_gain_min,     sizeof(double), MSI_phasecodes,optbeamcodefile);
            fwrite(opt_gain_max,     sizeof(double), MSI_phasecodes,optbeamcodefile);

            fclose(optbeamcodefile);
            optbeamcodefile=NULL;
          } else {
               fprintf(stdout,"    Warning::  Failed to Open: %s\n",filename);
               //mypause();
          }
          if (verbose > -1 ){ 
            fprintf(stdout,"    ++++++++\n");  
            fprintf(stdout,"    Writing Optimized values to card memory: %d\n",c);
          }
          for(b=0;b<MSI_phasecodes;b++) {
            if(opt_qual[b]>=0) {
              if (verbose > -1 ) {
                      fprintf(stdout,"           Card: %5d MemLoc: %5d Q: %5d Bmnum: %5d Angle: %13.4lf [deg] Freq Range: %-08.5e - %-08.5e [Hz]\n", c, b, opt_qual[b],
                               opt_bmnum[b],   opt_bmangle_deg[b],
                               opt_freq_lo[b], opt_freq_hi[b]
                     );
                      fprintf(stdout,"             pcode: %5d :: tdelay [ns]:: Min: %-08.5e Max: %-08.5e Ave: %-08.5e\n",opt_pcode[b],opt_tdelay_min[b],opt_tdelay_max[b],opt_tdelay_ave[b]);
                      fprintf(stdout,"             acode: %5d :: gain   [dB]:: Min: %-08.5e Max: %-08.5e Ave: %-08.5e\n",opt_acode[b],opt_gain_min[b],opt_gain_max[b],opt_gain_ave[b]);
              }
              rval=MSI_dio_write_memory(b,rnum,c,opt_pcode[b],opt_acode[b],sshflag,verbose);
            }
          }
          if (verbose > -1 ){ 
            fprintf(stdout,"    ########\n");  
          }
     }
     return 0;
}
