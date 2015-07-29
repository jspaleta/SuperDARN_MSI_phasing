/* 
 * This program is designed to verify/(re)write the optimized final timedelay beam programming
 * for MSI phasing cards to correct for frequency variance of expected timedelay
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
#include <signal.h>
#include <ctype.h>

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

extern char      ssh_userhost[128];

static int keepRunning = 1;

void intHandler(int dummy) {
    printf("HERE\n");
    keepRunning = 0;
};

int main(int argc, char **argv ) {
     
     int verbose=0;
     int rval;
     int c,b;
     int32_t loops_total;

     int first_card=0,last_card=19;

 
     int32_t freq_steps=0;


     int32_t    memloc;
 
     FILE *optbeamcodefile=NULL;
     FILE *optlookupfile=NULL;
     char *caldir=NULL;
     char dirstub[256]="";
     char filename[512]="";
     char radar_name[16]="";
     int32_t sshflag=0,vflag=0,Fflag=0,wflag=0,mflag=0,nflag=0,cflag=0,rflag=0,rnum=0;


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
/* Final lookup table variables to be used to sleep tuned beams during operations */
     int        lookup_seen=0;
     int        lookup_mismatch=0;
     int32_t    lookup_num_codes=0;
     int32_t    lookup_mem_offset;
     int32_t    lookup_freq_steps;
     double     *lookup_freq_lo=NULL;
     double     *lookup_freq_hi=NULL;
     double     *lookup_freq_center=NULL;
     int32_t    *lookup_bmnum=NULL;
     int32_t    *lookup_qual=NULL;
     signal(SIGINT, intHandler);

     while ((rval = getopt (argc, argv, "+r:m:n:c:a:p:v:s:iWVFh")) != -1) {
         switch (rval) {
           case 'v':
             verbose=atoi(optarg);
             break;
           case 'W':
             wflag=1; 
             vflag=1; 
             break;
           case 'F':
             Fflag=1; 
             break;
           case 'm':
             mflag=1; 
             memloc=atoi(optarg);
             break;
           case 'V':
             vflag=1; 
             break;
           case 'n':
             rnum=atoi(optarg);
             nflag=1; 
             break;
           case 'c':
             first_card=atoi(optarg);
             last_card=atoi(optarg);
             cflag=1;
             break;
           case 'r':
             snprintf(radar_name,16,"%s",optarg);
             rflag=1;
             break;
           case 's':
             sshflag=1;
             snprintf(ssh_userhost,128,"%s",optarg);
             break;
           case '?':
             if (optopt =='m' || optopt == 'r' || optopt =='c' || optopt =='n' || optopt=='s'|| optopt=='v')
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
               fprintf(stderr,"Required:\n  -r <radarname> : 3-letter radarcode \n  -n <number> :dio radar number (1 or 2)\n  -c <number> :card number (0-19)\n");
               fprintf(stderr,"Optional:\n  -W :flag  to enable write to card memory\n  -V :flag to enable verify card memory\n  -m  <memloc> : specify single memory location\n");
               fprintf(stderr,"  -F :flag  to enable write of lookup table\n");
               fprintf(stderr,"  -v <number> :to set verbose output level\n  -s <user@host> :to enable ssh based write/verify\n");
               return 1;
         }
     }
     if (argc == 1 || rnum==0 || rflag==0 || nflag==0) {
               fprintf (stderr,"Required arguments -r radarname, -n dio radar number\n Consult the help using the -h option\n");
               return 1;
     }
     caldir=getenv("MSI_CALDIR");
     if (caldir==NULL) {
          caldir=strdup("/data/calibrations/");
     }
     sprintf(dirstub,"/%s/%s/",caldir,radar_name);
     fprintf(stdout,"MSI_CALDIR: %s\n",caldir);
     printf("Radar: <%s>\n",radar_name);
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

     lookup_qual         =calloc(MSI_phasecodes,sizeof(int32_t));
     lookup_bmnum        =calloc(MSI_phasecodes,sizeof(int32_t));
     lookup_freq_lo      =calloc(MSI_phasecodes,sizeof(double));
     lookup_freq_hi      =calloc(MSI_phasecodes,sizeof(double));
     lookup_freq_center  =calloc(MSI_phasecodes,sizeof(double));

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

     /* Determine the freq steps */
     freq_steps=(MSI_max_freq-MSI_min_freq)/MSI_freq_window+1;
     if (freq_steps > MSI_max_freq_steps) {
       fprintf(stderr,"Number of frequency steps too large for MSI phase card memory to support! Check your MSI frequency windowing settings: min/max_freq and freq_window\n");
       exit(-1);
     }
     if(opt_mem_offset+(freq_steps*opt_mem_offset+MSI_num_angles) > MSI_phasecodes) {
            fprintf(stderr,"Overrun of Phasecode space: f: %d a: %d\n",freq_steps,MSI_num_angles);
            exit(1);
     } else {
     } 
     sprintf(filename,"%s/opt_lookup_%s.dat",dirstub,radar_name);
     if (Fflag) {
       if (verbose > -1 ) fprintf(stdout,"    New Optimized lookup table will be written:\n");
       if (verbose > -1 ) fprintf(stdout,"      File to write: %s\n",filename);
       lookup_seen=0;
       lookup_mismatch=0;
     } else {
       optlookupfile=fopen(filename,"r");
       if (optlookupfile!=NULL) {
         if (verbose > -1 ) fprintf(stdout,"  Reading Optimized lookup table:\n");
         if (verbose > -1 ) fprintf(stdout,"    Opened: %s\n",filename);
         fread(&lookup_num_codes, sizeof(int32_t),1,optlookupfile);
         fread(&lookup_mem_offset, sizeof(int32_t),1,optlookupfile);
         fread(&lookup_freq_steps, sizeof(int32_t),1,optlookupfile);
         fread(lookup_bmnum,       sizeof(int32_t),MSI_phasecodes,optlookupfile);
         fread(lookup_freq_center, sizeof(double) ,MSI_phasecodes,optlookupfile);
         fread(lookup_freq_lo,     sizeof(double) ,MSI_phasecodes,optlookupfile);
         fread(lookup_freq_hi,     sizeof(double) ,MSI_phasecodes,optlookupfile);
         fread(lookup_qual,        sizeof(int32_t),MSI_phasecodes,optlookupfile);
         if (verbose > -1 ) fprintf(stdout,"  Lookup table:\n");
         if (verbose > -1 ) fprintf(stdout,"    Mem Offset: %d\n",lookup_mem_offset);
         if (verbose > -1 ) fprintf(stdout,"    Freq Steps: %d\n",lookup_freq_steps);
         for(b=0;b<MSI_phasecodes;b++) {
           if(opt_qual[b]>-1) {
             if (verbose > -1 ) fprintf(stdout,"  %5d Bmnum: %3d " ,b,lookup_bmnum[b]);
             if (verbose > -1 ) fprintf(stdout,"  Qual: %3d ",lookup_qual[b]);
             if (verbose > -1 ) fprintf(stdout,"  Freq_lo: %8.3e ",lookup_freq_lo[b]);
             if (verbose > -1 ) fprintf(stdout,"  Freq_hi: %8.3e ",lookup_freq_hi[b]);
             if (verbose > -1 ) fprintf(stdout,"  Freq_c : %8.3e\n",lookup_freq_center[b]);
           }
         }
         lookup_seen=1;
         lookup_mismatch=0;
         fclose(optlookupfile);
       } else {
         if (verbose > -1 ) fprintf(stdout,"    Warning: Optimized lookup table:\n");
         if (verbose > -1 ) fprintf(stdout,"      Failed to Open: %s\n",filename);
         lookup_seen=0;
         lookup_mismatch=0;
       }
     }
     for(c=first_card;c<=last_card;c++) {
          /* Saved optimized settings in optcodes_cal files*/
          sprintf(filename,"%s/optcodes_cal_%s_%02d.dat",dirstub,radar_name,c);
          optbeamcodefile=fopen(filename,"r");
          if (optbeamcodefile!=NULL) {
            if (verbose > -1 ) fprintf(stdout,"  Reading Optimized values from file for card: %d\n",c);
            if (verbose > -1 ) fprintf(stdout,"    Opened: %s\n",filename);
            /* Length of the arrays */
            fread(&MSI_phasecodes,  sizeof(int32_t),1,optbeamcodefile);
            /* memory offset to start of narrow freq band */
            fread(&opt_mem_offset,  sizeof(int32_t),1,optbeamcodefile);
            /* Number of angles programmed*/
            fread(&MSI_num_angles,  sizeof(int32_t),1,optbeamcodefile);

            /* Number of freq steps programmed
            *    freq_steps=(MSI_max_freq-MSI_min_freq)/MSI_freq_window+1;
            *    first step is wide band average, 
            *    sequent steps are narrow MSI_freq_window ave
            */ 
            fread(&freq_steps,      sizeof(int32_t),1,optbeamcodefile);
            /* Min frequency considered */
            fread(&MSI_min_freq,    sizeof(double),1,optbeamcodefile);
            /* Min frequency considered */
            fread(&MSI_max_freq,    sizeof(double),1,optbeamcodefile);
            /* Narrow frequency window considered */
            fread(&MSI_freq_window, sizeof(double),1,optbeamcodefile);
            loops_total=freq_steps*MSI_num_angles;
            fprintf(stdout,"Info:: Optcodes File Info for Card: %d \n",c);
            fprintf(stdout,"Info:: Angles: %d\n",MSI_num_angles);
            fprintf(stdout,"Info:: Freq Steps: %d\n",freq_steps);
            fprintf(stdout,"Info:: Optimizations per card: %d\n",loops_total);
            fprintf(stdout,"Info:: Max Card Memory Location: %d\n",opt_mem_offset+(freq_steps*opt_mem_offset+MSI_num_angles));
            if(sshflag==1) fprintf(stdout,"Info:: SSH: %s\n",ssh_userhost);
            if(wflag==1) fprintf(stdout,"Ready to begin writing saved programmming to card memory\n");
            else fprintf(stdout,"Ready to readback saved programmming from file\n");
            mypause();
            if(keepRunning==0) return 0; 

 
            /* reset array lengths */
            /* code arrays for final memory values*/
            free(opt_pcode);
            opt_pcode          =calloc(MSI_phasecodes,sizeof(int32_t));
            free(opt_acode);
            opt_acode          =calloc(MSI_phasecodes,sizeof(int32_t));
            free(opt_qual);
            opt_qual           =calloc(MSI_phasecodes,sizeof(int32_t));
            /* Freq and bm arrays used for self-checking and searching lookup table */ 
            free(opt_freq_lo);
            opt_freq_lo        =calloc(MSI_phasecodes,sizeof(double));
            free(opt_freq_hi);
            opt_freq_hi        =calloc(MSI_phasecodes,sizeof(double));
            free(opt_freq_center);
            opt_freq_center    =calloc(MSI_phasecodes,sizeof(double));
            free(opt_bmnum);
            opt_bmnum          =calloc(MSI_phasecodes,sizeof(int32_t));
            free(opt_bmangle_deg);
            opt_bmangle_deg    =calloc(MSI_phasecodes,sizeof(double));
            /* tdelay and atten arrays used for self-checking of final programming */
            free(opt_tdelay_ave);
            opt_tdelay_ave     =calloc(MSI_phasecodes,sizeof(double));
            free(opt_tdelay_target);
            opt_tdelay_target  =calloc(MSI_phasecodes,sizeof(double));
            free(opt_tdelay_min);
            opt_tdelay_min     =calloc(MSI_phasecodes,sizeof(double));
            free(opt_tdelay_max);
            opt_tdelay_max     =calloc(MSI_phasecodes,sizeof(double));
            free(opt_gain_ave);
            opt_gain_ave       =calloc(MSI_phasecodes,sizeof(double));
            free(opt_gain_target);
            opt_gain_target    =calloc(MSI_phasecodes,sizeof(double));
            free(opt_gain_min);
            opt_gain_min       =calloc(MSI_phasecodes,sizeof(double));
            free(opt_gain_max);
            opt_gain_max       =calloc(MSI_phasecodes,sizeof(double));

            /* Now read the arrays */
            fread(opt_pcode,        sizeof(int32_t),MSI_phasecodes,optbeamcodefile);
            fread(opt_acode,        sizeof(int32_t),MSI_phasecodes,optbeamcodefile);
            fread(opt_qual,         sizeof(int32_t),MSI_phasecodes,optbeamcodefile);
            fread(opt_bmnum,        sizeof(int32_t),MSI_phasecodes,optbeamcodefile);
            fread(opt_bmangle_deg,  sizeof(double), MSI_phasecodes,optbeamcodefile);
            fread(opt_freq_lo,      sizeof(double), MSI_phasecodes,optbeamcodefile);
            fread(opt_freq_hi,      sizeof(double), MSI_phasecodes,optbeamcodefile);
            fread(opt_freq_center,  sizeof(double), MSI_phasecodes,optbeamcodefile);
            fread(opt_tdelay_ave,   sizeof(double), MSI_phasecodes,optbeamcodefile);
            fread(opt_tdelay_target,sizeof(double), MSI_phasecodes,optbeamcodefile);
            fread(opt_tdelay_min,   sizeof(double), MSI_phasecodes,optbeamcodefile);
            fread(opt_tdelay_max,   sizeof(double), MSI_phasecodes,optbeamcodefile);
            fread(opt_gain_ave,     sizeof(double), MSI_phasecodes,optbeamcodefile);
            fread(opt_gain_target,  sizeof(double), MSI_phasecodes,optbeamcodefile);
            fread(opt_gain_min,     sizeof(double), MSI_phasecodes,optbeamcodefile);
            fread(opt_gain_max,     sizeof(double), MSI_phasecodes,optbeamcodefile);
            fclose(optbeamcodefile);
            optbeamcodefile=NULL;

            if(lookup_seen==0) {
              lookup_mem_offset=opt_mem_offset;
              lookup_freq_steps=freq_steps;
              for(b=0;b<MSI_phasecodes;b++) {
                lookup_bmnum[b]=opt_bmnum[b];
                lookup_qual[b]=opt_qual[b];
                lookup_freq_lo[b]=opt_freq_lo[b];
                lookup_freq_hi[b]=opt_freq_hi[b];
                lookup_freq_center[b]=opt_freq_center[b];
              }
            } else {
              if (lookup_mem_offset!=opt_mem_offset) lookup_mismatch=1;
              if (lookup_freq_steps!=freq_steps) lookup_mismatch=1;
              for(b=0;b<MSI_phasecodes;b++) {
                if(lookup_qual[b]==1) lookup_qual[b]=opt_qual[b];
                if(lookup_bmnum[b]!=opt_bmnum[b]) lookup_mismatch=1;
                if(lookup_freq_lo[b]!=opt_freq_lo[b]) lookup_mismatch=1;
                if(lookup_freq_hi[b]!=opt_freq_hi[b]) lookup_mismatch=1;
                if(lookup_freq_center[b]!=opt_freq_center[b]) lookup_mismatch=1;
                if(lookup_mismatch) {
                  if (verbose > -1 ) fprintf(stdout,"Mismatch between Lookup table and optcode file:\n");
                  if (verbose > -1 ) fprintf(stdout,"Lookup table:\n");
                  if (verbose > -1 ) fprintf(stdout,"  Mem Offset: %d\n",lookup_mem_offset);
                  if (verbose > -1 ) fprintf(stdout,"  Freq Steps: %d\n",lookup_freq_steps);
                  if (verbose > -1 ) fprintf(stdout,"  %5d Bmnum: %3d " ,b,lookup_bmnum[b]);
                  if (verbose > -1 ) fprintf(stdout,"  Qual: %3d ",lookup_qual[b]);
                  if (verbose > -1 ) fprintf(stdout,"  Freq_lo: %8.3e ",lookup_freq_lo[b]);
                  if (verbose > -1 ) fprintf(stdout,"  Freq_hi: %8.3e ",lookup_freq_hi[b]);
                  if (verbose > -1 ) fprintf(stdout,"  Freq_c : %8.3e\n",lookup_freq_center[b]);
                  if (verbose > -1 ) fprintf(stdout,"Optcode:\n");
                  if (verbose > -1 ) fprintf(stdout,"  Mem Offset: %d\n",opt_mem_offset);
                  if (verbose > -1 ) fprintf(stdout,"  Freq Steps: %d\n",freq_steps);
                  if (verbose > -1 ) fprintf(stdout,"  %5d Bmnum: %3d " ,b,opt_bmnum[b]);
                  if (verbose > -1 ) fprintf(stdout,"  Qual: %3d ",opt_qual[b]);
                  if (verbose > -1 ) fprintf(stdout,"  Freq_lo: %8.3e ",opt_freq_lo[b]);
                  if (verbose > -1 ) fprintf(stdout,"  Freq_hi: %8.3e ",opt_freq_hi[b]);
                  if (verbose > -1 ) fprintf(stdout,"  Freq_c : %8.3e\n",opt_freq_center[b]);
                  exit(0);
                }
              }
            }
          } else {
               fprintf(stdout,"    Warning::  Failed to Open: %s\n",filename);
               //mypause();
          }
          if(keepRunning==0) return 0; 
          for(b=0;b<MSI_phasecodes;b++) {
            if(keepRunning==0) return 0; 
            if(mflag==1) {
              if (b!=memloc) continue;
              if(opt_qual[b]>-1) {
                if (verbose > -1 ) {
                      fprintf(stdout,"           Card: %5d MemLoc: %5d Q: %5d Bmnum: %5d Angle: %13.4lf [deg] Freq Range: %-8.5e - %-8.5e [Hz]\n", c, b, opt_qual[b],
                               opt_bmnum[b],   opt_bmangle_deg[b],
                               opt_freq_lo[b], opt_freq_hi[b]
                     );
                      fprintf(stdout,"             pcode: %5d :: tdelay [ns]:: Min: %-8.5e Max: %-8.5e Ave: %-8.5e Target: %-8.5e Delta: %-8.5e\n",
                        opt_pcode[b],opt_tdelay_min[b],opt_tdelay_max[b],opt_tdelay_ave[b],opt_tdelay_target[b],fabs(opt_tdelay_ave[b]-opt_tdelay_target[b]));
                      fprintf(stdout,"             acode: %5d :: gain   [dB]:: Min: %-8.5e Max: %-8.5e Ave: %-8.5e Target: %-8.5e Delta %-8.5e\n",
                        opt_acode[b],opt_gain_min[b],opt_gain_max[b],opt_gain_ave[b],opt_gain_target[b],fabs(opt_gain_ave[b]-opt_gain_target[b]));
                }
                if(wflag==1) {
                  rval=MSI_dio_write_memory(b,rnum,c,opt_pcode[b],opt_acode[b],sshflag,verbose);
                  if (WIFSIGNALED(rval) && (WTERMSIG(rval) == SIGINT || WTERMSIG(rval) == SIGQUIT)) return rval;
                } else {

                }
              }
            }
          }
          if(vflag==1) {
            fprintf(stdout,"  Verifying card memory programming: Start\n");
            for(b=0;b<MSI_phasecodes;b++) {
              if(keepRunning==0) return 0; 
              if(mflag==1) {
                if (b!=memloc) continue;
              }
              if(opt_qual[b]>-1) {
                rval=MSI_dio_verify_memory(b,rnum,c,opt_pcode[b],opt_acode[b],sshflag,verbose);
                if (WIFSIGNALED(rval) && (WTERMSIG(rval) == SIGINT || WTERMSIG(rval) == SIGQUIT)) return rval;
                if (rval!=0) {
                      fprintf(stdout,"  ERROR:  Card: %5d MemLoc: %5d Q: %5d Bmnum: %5d Angle: %13.4lf [deg] Freq Range: %-8.5e - %-8.5e [Hz]\n", c, b, opt_qual[b],
                               opt_bmnum[b],   opt_bmangle_deg[b],
                               opt_freq_lo[b], opt_freq_hi[b]
                     );
                }
              }
            }
            fprintf(stdout,"  Verifying card memory programming: End\n");
          }
          if (lookup_mismatch) {
            fprintf(stdout,"memory map mismatch!\nPlease check cards individually to make sure optimized settings are consistent!\n");
            if (verbose > -1 ) fprintf(stdout,"Expected Values:\n");
            if (verbose > -1 ) fprintf(stdout,"  Mem Offset: %d\n",lookup_mem_offset);
            if (verbose > -1 ) fprintf(stdout,"  Freq Steps: %d\n",lookup_freq_steps);
            for(b=0;b<MSI_phasecodes;b++) {
                //if (verbose > -1 ) fprintf(stdout,"  %5d Bmnum: %3d " ,b,lookup_bmnum[b]);
                //if (verbose > -1 ) fprintf(stdout,"Qual: %3d ",lookup_qual[b]);
                //if (verbose > -1 ) fprintf(stdout,"Freq_lo: %8.3e ",lookup_freq_lo[b]);
                //if (verbose > -1 ) fprintf(stdout,"Freq_hi: %8.3e ",lookup_freq_hi[b]);
                //if (verbose > -1 ) fprintf(stdout,"Freq_c : %8.3e ",lookup_freq_center[b]);
            }
          }
          continue;

     } // end card loop
     if (lookup_mismatch) {
            fprintf(stdout,"memory map mismatch!\nPlease check cards individually to make sure optimized settings are consistent!\n");
       
     } else {
       if (Fflag) {
         if (verbose > -1 ) fprintf(stdout,"Lookup table:\n");
         if (verbose > -1 ) fprintf(stdout,"  Mem Offset: %d\n",lookup_mem_offset);
         if (verbose > -1 ) fprintf(stdout,"  Freq Steps: %d\n",lookup_freq_steps);
         for(b=0;b<MSI_phasecodes;b++) {
           if(opt_qual[b]>-1) {
             if (verbose > -1 ) fprintf(stdout,"  %5d Bmnum: %3d " ,b,lookup_bmnum[b]);
             if (verbose > -1 ) fprintf(stdout,"Qual: %3d ",lookup_qual[b]);
             if (verbose > -1 ) fprintf(stdout,"Freq_lo: %8.3e ",lookup_freq_lo[b]);
             if (verbose > -1 ) fprintf(stdout,"Freq_hi: %8.3e ",lookup_freq_hi[b]);
             if (verbose > -1 ) fprintf(stdout,"Freq_c : %8.3e\n",lookup_freq_center[b]);
           }
         }
         sprintf(filename,"%s/opt_lookup_%s.dat",dirstub,radar_name);
         if (verbose > -1 ) fprintf(stdout,"Writing lookup table values to file: %s\n",filename);
         optlookupfile=fopen(filename,"w");
         if (optlookupfile!=NULL) {
           fwrite(&MSI_phasecodes,    sizeof(int32_t),1,optlookupfile);
           fwrite(&lookup_mem_offset, sizeof(int32_t),1,optlookupfile);
           fwrite(&lookup_freq_steps, sizeof(int32_t),1,optlookupfile);
           fwrite(lookup_bmnum,       sizeof(int32_t),MSI_phasecodes,optlookupfile);
           fwrite(lookup_freq_center, sizeof(double) ,MSI_phasecodes,optlookupfile);
           fwrite(lookup_freq_lo,     sizeof(double) ,MSI_phasecodes,optlookupfile);
           fwrite(lookup_freq_hi,     sizeof(double) ,MSI_phasecodes,optlookupfile);
           fwrite(lookup_qual,        sizeof(int32_t),MSI_phasecodes,optlookupfile);
           fclose(optlookupfile);
         } else {
           fprintf(stdout,"Error! Writing lookup table to file failed\n");
         }
       }
     }
     return 0;
}
