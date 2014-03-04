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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* helper functions  for vna */
#include "vna_functions.h"
/* Useful defines for MSI phasing cards */
#include "MSI_functions.h"

void main() {

     int verbose=0;
     int c,a,f,bf,ba,best_beam_freq_index,best_beam_angle_index; /* simple indexers for looping */
     int first_card=0,last_card=19;
     
     double middle=(float)(MSI_num_angles-1)/2.0;
     double angles_degrees[MSI_num_angles];
     double freq_center[MSI_max_freq_steps];
     double freq_lo[MSI_max_freq_steps];
     double freq_hi[MSI_max_freq_steps];
     int32_t freq_steps=0;

     double timedelay_nsecs;
     double fdiff,tdiff;
 
     FILE *beamcodefile=NULL;
     char *caldir=NULL;
     char dirstub[256]="";
     char filename[256]="";
     char radar_name[16]="ade";

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
     
     for(c=first_card;c<=last_card;c++) {
          /* Inside the card loop */
          if (verbose > 0 ) fprintf(stdout, "Starting optimization for Card: %d\n",c);
          /* Load the beamtable file for this card*/
          sprintf(filename,"%s/beamcodes_cal_%s_%02d.dat",dirstub,radar_name,c);
          beamcodefile=fopen(filename,"r");
          if (beamcodefile!=NULL) {
               if (verbose > 0 ) fprintf(stdout,"Opened: %s\n",filename);
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

               for(f=0;f<=num_beam_steps;f++) {
                 fread(&beam_freq_index,sizeof(int32_t),1,beamcodefile);
                 if (f!=beam_freq_index) {
                   fprintf(stderr,"Read file error! %d %d\n",f,beam_freq_index);
                   exit(-1);
                 } 
                 fread(&beam_freq_lo[f],sizeof(double),1,beamcodefile);
                 fread(&beam_freq_hi[f],sizeof(double),1,beamcodefile);
                 beam_freq_center[f]=(beam_freq_lo[f]+beam_freq_hi[f])/2.0;

                 if(beam_pwr_dB!=NULL) free(beam_pwr_dB);
                 beam_pwr_dB=calloc(num_beam_angles*(num_beam_steps+1),sizeof(double));
                 if(beam_tdelay_nsec!=NULL) free(beam_tdelay_nsec);
                 beam_tdelay_nsec=calloc(num_beam_angles*(num_beam_steps+1),sizeof(double));
                 if(beam_phasecode!=NULL) free(beam_phasecode);
                 beam_phasecode=calloc(num_beam_angles*(num_beam_steps+1),sizeof(int32_t));
                 if(beam_attencode!=NULL) free(beam_attencode);
                 beam_attencode=calloc(num_beam_angles*(num_beam_steps+1),sizeof(int32_t));
               
                 for(a=0;a<num_beam_angles;a++) {
                      fread(&beam_pwr_dB[(f*num_beam_angles)+a],sizeof(double),1,beamcodefile);
                      fread(&beam_tdelay_nsec[(f*num_beam_angles)+a],sizeof(double),1,beamcodefile);
                      fread(&beam_attencode[(f*num_beam_angles)+a],sizeof(int32_t),1,beamcodefile);
                      fread(&beam_phasecode[(f*num_beam_angles)+a],sizeof(int32_t),1,beamcodefile);
                 } 
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
               tdiff=1E13;
               best_beam_angle_index=-1;
               for(ba=a;ba<num_beam_angles;ba++) {
                    if ( fabs(beam_needed_delay[ba]-timedelay_nsecs) < tdiff ) {
                         best_beam_angle_index=ba;
                         tdiff=fabs(beam_needed_delay[ba]-timedelay_nsecs);
                    }  
               } 
               if (verbose > -1 ) fprintf(stdout, "  Beam Angle: %5d  <%5d>: %lf [deg] : %8.4lf : <%8.4lf>\n",a,best_beam_angle_index,angles_degrees[a],timedelay_nsecs,beam_needed_delay[best_beam_angle_index]);
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
                    if (verbose > -1 ) fprintf(stdout, "    Optimizating Freq Step: %5d : %-08.5e - %-08.5e [Hz] < %-08.5e - %-08.5e> [Hz]\n",f,freq_lo[f],freq_hi[f],
                                               beam_freq_lo[best_beam_freq_index],beam_freq_hi[best_beam_freq_index]);
               }
               /* fprintf(stdout,"%4d %4d %8.3lf %8.3lf\n",c,a,angles_degrees[a],timedelay_nsecs);*/
          }
     }



  fprintf(stdout,"Hello Word\n");
}
