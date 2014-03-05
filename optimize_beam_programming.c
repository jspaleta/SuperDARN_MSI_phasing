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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* helper functions  for vna */
#include "vna_functions.h"
/* Useful defines for MSI phasing cards */
#include "MSI_functions.h"

/* variables defined elsewhere */
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

int main(int argc, char **argv ) {
     
     int verbose=0;
     int sock=-1;
     int rval;
     int i,c,b,a,f,t,bf,ba,pc,ac;
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

     double timedelay_nsecs,needed_tdelay;
     double td_sum,td_ave,pwr_sum,pwr_ave;
     int32_t nave,pcode_range,pcode_min,pcode_max;
     int32_t acode_range,acode_min,acode_max;
     double adelta,tdelta;
     double fdiff,tdiff;
 
     FILE *beamcodefile=NULL;
     char *caldir=NULL;
     char dirstub[256]="";
     char filename[256]="";
     char radar_name[16]="ade";
     char vna_host[24]="137.229.27.122";
     char strout[128]="";
     char output[128]="";
     char command[128]="";
     char diocmd[256]="";
     int32_t iflag=0,rflag=0,rnum=0,port=23;

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

     while ((rval = getopt (argc, argv, "+r:n:c:a:p:v:ih")) != -1) {
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
     fflush(stdout);
     sprintf(dirstub,"/%s/%s/",caldir,radar_name);

     for(i=0;i<VNA_FREQS;i++) {
       freq[i]=MSI_min_freq+i*((MSI_max_freq-MSI_min_freq)/(double)(VNA_FREQS-1));
       phase[i]=calloc(VNA_PHASECODES,sizeof(double));
       tdelay[i]=calloc(VNA_PHASECODES,sizeof(double));
       pwr_mag[i]=calloc(VNA_PHASECODES,sizeof(double));
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

     /* Initialize VNA */
     if (verbose>0) printf("Opening Socket %s %d\n",vna_host,port);
     sock=opentcpsock(vna_host, port);
     if (sock < 0) {
       if (verbose>0) printf("Socket failure %d\n",sock);
     } else if (verbose>0) printf("Socket %d\n",sock);
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
       sprintf(command,":SENS1:FREQ:STAR %E\r\n",MSI_min_freq);
       button_command(sock,command,10,verbose);
       sprintf(command,":SENS1:FREQ:STOP %E\r\n",MSI_max_freq);
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
       usleep(VNA_wait_delay_ms*1000); 

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
     printf("\n\nVNA Init Complete\nReconfigure for Phasing Card Measurements");
     mypause();

     
     for(c=first_card;c<=last_card;c++) {
          /* Inside the card loop */
          if (verbose > 0 ) fprintf(stdout, "Starting optimization for Card: %d\n",c);
          /* Load the beamtable file for this card*/
          sprintf(filename,"%s/beamcodes_cal_%s_%02d.dat",dirstub,radar_name,c);
          beamcodefile=fopen(filename,"r");
          if (beamcodefile!=NULL) {
               if (verbose > -1 ) fprintf(stdout,"Opened: %s\n",filename);
               fread(&beam_highest_time0_nsec,sizeof(double),1,beamcodefile);
               fread(&beam_lowest_pwr_dB,sizeof(double),1,beamcodefile);
               if(beam_highest_time0_nsec != MSI_target_tdelay0_nsecs) {
                 fprintf(stderr,"Card %d Target time0 mismatch: %lf  %lf\n",c,beam_highest_time0_nsec,MSI_target_tdelay0_nsecs);
                 exit(-1); 
               }
               if(beam_lowest_pwr_dB != MSI_target_pwr_dB) {
                 fprintf(stderr,"Card %d Target pwr  mismatch: %lf  %lf\n",c,beam_lowest_pwr_dB, MSI_target_pwr_dB);
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
                   fprintf(stderr,"Read file error! %d %d\n",f,beam_freq_index);
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
                  
                 if (verbose > 1 ) fprintf(stdout,"  %5d :: %-8.5e  %-8.5e\n", f,beam_freq_lo[f],beam_freq_hi[f]);
               } 

               fclose(beamcodefile); 
          } else {
               fprintf(stdout,"Failed to Open: %s\n",filename);
               num_beam_freqs=0;
               num_beam_steps=0;
               num_beam_angles=0;
          }
          for(a=0;a<MSI_num_angles;a++) {
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
               if (verbose > -1 ) fprintf(stdout, "  Beam Angle: %5d : %lf [deg] : %8.4lf \n",a,angles_degrees[a],timedelay_nsecs);
               for(f=0;f<freq_steps;f++) {
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
                    if (verbose > -1 ){ 
                      fprintf(stdout, "    Optimizating Freq Step: %5d : %-08.5e - %-08.5e [Hz]\n", f,freq_lo[f],freq_hi[f]);
                      fprintf(stdout,"       Best findex: %d aindex: %d\n",best_beam_freq_index,best_beam_angle_index);
                      fprintf(stdout,"       Initial Guess phasecode: %d acode: %d\n",best_phasecode,best_attencode);
                      fprintf(stdout,"       Initial Guess   tdelay: %lf pwr: %lf\n",best_tdelay,best_pwr);
                      fflush(stdout);
                    } 
                    /* Take a measurement at best phasecode and acode */
                    b=f*32+a;
                    take_data(sock,b,rnum,c,best_phasecode,best_attencode,pwr_mag,phase,tdelay,verbose);
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
                      fprintf(stdout,"Needed tdelay: %13.4lf (ns) Ave tdelay: %13.4lf (ns)\n",needed_tdelay,td_ave*1E9); 
                      fprintf(stdout,"  Target Gain: %13.4lf (dB)   Ave Gain: %13.4lf (dB)\n",MSI_target_pwr_dB,pwr_ave); 
                    }
                    adelta=fabs(MSI_target_pwr_dB-pwr_ave);
                    acode_range=2*MSI_attencode(adelta);
                    fprintf(stdout,"  Optimizing attencode.... %d measurements needed\n",acode_range); 
                    if (verbose > 1 ) fprintf(stdout,"  acode range: %d\n",acode_range); 
                    acode_min=best_attencode-acode_range;
                    acode_max=best_attencode+acode_range;
		    if(acode_min < 0) acode_min=0; 
                    if(acode_max > 63) acode_max=63; 
                    for(ac=acode_min;ac<=acode_max;ac++) {
                      take_data(sock,b,rnum,c,best_phasecode,ac,pwr_mag,phase,tdelay,verbose);
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
                    if (verbose > 1 ){ 
                      fprintf(stdout,"  Optimized acode: %d %lf %lf\n",best_attencode,best_pwr,MSI_target_pwr_dB); 
                      fprintf(stdout,"    Needed tdelay: %13.4lf (ns) Ave tdelay: %13.4lf (ns)\n",needed_tdelay,td_ave*1E9); 
                      fprintf(stdout,"    Target Gain: %13.4lf (dB)   Ave Gain: %13.4lf (dB)\n",MSI_target_pwr_dB,best_pwr); 
                    }
                    take_data(sock,b,rnum,c,best_phasecode,best_attencode,pwr_mag,phase,tdelay,verbose);
                    td_sum=0.0;
                    pwr_sum=0.0;
                    nave=0; 
                    for(i=0;i<VNA_FREQS;i++) {
                      if((freq[i] >= freq_lo[f]) && (freq[i] <=freq_hi[f])){
                        //fprintf(stdout,"%lf :: %lf %e %lf\n",freq[i],phase[i][b],tdelay[i][b],pwr_mag[i][b]); 
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
                      fprintf(stdout,"Needed tdelay: %13.4lf (ns) Ave tdelay: %13.4lf (ns)\n",needed_tdelay,td_ave*1E9); 
                      fprintf(stdout,"  Target Gain: %13.4lf (dB)   Ave Gain: %13.4lf (dB)\n",MSI_target_pwr_dB,pwr_ave); 
                    }
                    tdelta=fabs(needed_tdelay-td_ave*1E9);
                    pcode_range=2*MSI_phasecode(tdelta);
                    fprintf(stdout,"  Optimizing phasecode.... %d measurements needed\n",pcode_range); 
/*
                    if(tdelta > MSI_tdelay_tolerance_nsec) {
                    } else {
                      pcode_range=0; 
                    }
*/
                    if (verbose > 1 ) fprintf(stdout,"  pcode range: %d\n",pcode_range); 

                    for(pc=pcode_min;pc<=pcode_max;pc++) {
                      take_data(sock,b,rnum,c,pc,best_attencode,pwr_mag,phase,tdelay,verbose);
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
                    fprintf(stdout,"  Optimized pcode: %d :: %lf %lf\n",best_phasecode,best_tdelay,needed_tdelay); 
                    fprintf(stdout,"  Optimized acode: %d :: %lf %lf\n",best_attencode,best_pwr,MSI_target_pwr_dB); 
                    take_data(sock,b,rnum,c,best_phasecode,best_attencode,pwr_mag,phase,tdelay,verbose);
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
                    fprintf(stdout,"nave:%d\n",nave);
                    if(nave>0) {
                      td_ave=td_sum/nave;
                      pwr_ave=pwr_sum/nave;
                    } else {
                      td_ave=-1E13;
                      pwr_ave=-1E13;
                    }
                    fprintf(stdout, "    Final Freq Step: %5d : %-08.5e - %-08.5e [Hz]\n", f,freq_lo[f],freq_hi[f]);
                    fprintf(stdout, "      pcode: %d  acode %d\n",best_phasecode,best_attencode); 
                    fprintf(stdout, "      Needed tdelay: %13.4lf (ns) Opt tdelay: %13.4lf (ns)\n",needed_tdelay,td_ave*1E9); 
                    fprintf(stdout, "      Target Gain: %13.4lf (dB)   Opt Gain: %13.4lf (dB)\n",MSI_target_pwr_dB,pwr_ave); 

               }
               mypause();
               /* fprintf(stdout,"%4d %4d %8.3lf %8.3lf\n",c,a,angles_degrees[a],timedelay_nsecs);*/
          }
     }



  fprintf(stdout,"Hello Word\n");
}
