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
    printf("HERRRRRRRRRRRRRRRRRRRR\n");
    keepRunning = 0;
};

int main(int argc, char **argv ) {
     
     int verbose=0;
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
     double *ophase[VNA_FREQS],*opwr_mag[VNA_FREQS], *otdelay[VNA_FREQS];
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
     int32_t fast_loop,count,acode_step,pcode_step;
     double fdiff,tdiff;

     int32_t nave,pcode_range,pcode_min,pcode_max;
     int32_t acode_range,acode_min,acode_max;

 
     FILE *beamcodefile=NULL;
     FILE *optbeamcodefile=NULL;
     char *caldir=NULL;
     char dirstub[256]="";
     char filename[512]="";
     char radar_name[16]="";
     int32_t sshflag=0,vflag=0,wflag=0,nflag=0,cflag=0,rflag=0,rnum=0,port=23;

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

     signal(SIGINT, intHandler);

     while ((rval = getopt (argc, argv, "+r:n:c:a:p:v:s:iWVh")) != -1) {
         switch (rval) {
           case 'v':
             verbose=atoi(optarg);
             break;
           case 'W':
             wflag=1; 
             vflag=1; 
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
             if (optopt == 'r' || optopt =='c' || optopt =='n' || optopt=='s'|| optopt=='v')
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
               fprintf(stderr,"Required:\n  -r radarname\n  -n dio radar number (1 or 2)\n  -c card number\nOptional:\n  -W to write to card memory\n  -V to verify card memory\n");
               fprintf(stderr,"  -v number to set verbose output level\n  -s user@host to enable ssh based write/verify\n");
               return 1;
         }
     }
     if (argc == 1 || rnum==0 || rflag==0 || nflag==0||cflag==0) {
               fprintf (stderr,"Required arguments -r radarname, -n dio radar number and -c card number\n Consult the help using the -h option\n");
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
       ophase[i]=calloc(MSI_phasecodes,sizeof(double));
       otdelay[i]=calloc(MSI_phasecodes,sizeof(double));
       opwr_mag[i]=calloc(MSI_phasecodes,sizeof(double));
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
     } 
     
     for(c=first_card;c<=last_card;c++) {
          /* Let's save these optimized settings to a file so we can reuse them */
          sprintf(filename,"%s/optcodes_cal_%s_%02d.dat",dirstub,radar_name,c);
          optbeamcodefile=fopen(filename,"r");
          if (optbeamcodefile!=NULL) {
            if (verbose > -1 ) fprintf(stdout,"    Reading Optimized values to file for card: %d\n",c);
            if (verbose > -1 ) fprintf(stdout,"      Opened: %s\n",filename);

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

            fprintf(stdout,"Info:: Angles: %d\n",MSI_num_angles);
            fprintf(stdout,"Info:: Freq Steps: %d\n",freq_steps);
            loops_total=freq_steps*MSI_num_angles;
            fprintf(stdout,"Info:: Optimizations per card: %d\n",loops_total);
            fprintf(stdout,"Info:: Max Card Memory Location: %d\n",opt_mem_offset+(freq_steps*opt_mem_offset+MSI_num_angles));
            if(sshflag==1) fprintf(stdout,"Info:: SSH: %s\n",ssh_userhost);
            if(wflag==1) fprintf(stdout,"Ready to begin writing saved programmming to card memory\n");
            else fprintf(stdout,"Ready to readback saved programmming from file\n");
            mypause();

 
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
            chmod(filename, S_IRUSR|S_IRGRP|S_IROTH);
          } else {
               fprintf(stdout,"    Warning::  Failed to Open: %s\n",filename);
               //mypause();
          }
          if(keepRunning==0) return 0; 
          for(b=0;b<MSI_phasecodes;b++) {
            if(keepRunning==0) return 0; 
            if(opt_qual[b]>-1) {
              if (verbose > -1 ) {
                      fprintf(stdout,"           Card: %5d MemLoc: %5d Q: %5d Bmnum: %5d Angle: %13.4lf [deg] Freq Range: %-08.5e - %-08.5e [Hz]\n", c, b, opt_qual[b],
                               opt_bmnum[b],   opt_bmangle_deg[b],
                               opt_freq_lo[b], opt_freq_hi[b]
                     );
                      fprintf(stdout,"             pcode: %5d :: tdelay [ns]:: Min: %-08.5e Max: %-08.5e Ave: %-08.5e Target: %-08.5e Delta: %-08.5e\n",
                        opt_pcode[b],opt_tdelay_min[b],opt_tdelay_max[b],opt_tdelay_ave[b],opt_tdelay_target[b],fabs(opt_tdelay_ave[b]-opt_tdelay_target[b]));
                      fprintf(stdout,"             acode: %5d :: gain   [dB]:: Min: %-08.5e Max: %-08.5e Ave: %-08.5e Target: %-08.5e Delta %-08.5e\n",
                        opt_acode[b],opt_gain_min[b],opt_gain_max[b],opt_gain_ave[b],opt_gain_target[b],fabs(opt_gain_ave[b]-opt_gain_target[b]));
              }
              if(wflag==1) {
                rval=MSI_dio_write_memory(b,rnum,c,opt_pcode[b],opt_acode[b],sshflag,verbose);
                if (WIFSIGNALED(rval) && (WTERMSIG(rval) == SIGINT || WTERMSIG(rval) == SIGQUIT)) return rval;
              } else {

              }
            }
          }
          if(vflag==1) {
            fprintf(stdout,"  Verifying card memory programming: Start\n");
            for(b=0;b<MSI_phasecodes;b++) {
              if(keepRunning==0) return 0; 
              if(opt_qual[b]>-1) {
                rval=MSI_dio_verify_memory(b,rnum,c,opt_pcode[b],opt_acode[b],sshflag,verbose);
                if (WIFSIGNALED(rval) && (WTERMSIG(rval) == SIGINT || WTERMSIG(rval) == SIGQUIT)) return rval;
                if (rval!=0) {
                      fprintf(stdout,"  ERROR:  Card: %5d MemLoc: %5d Q: %5d Bmnum: %5d Angle: %13.4lf [deg] Freq Range: %-08.5e - %-08.5e [Hz]\n", c, b, opt_qual[b],
                               opt_bmnum[b],   opt_bmangle_deg[b],
                               opt_freq_lo[b], opt_freq_hi[b]
                     );
                }
              }
            }
            fprintf(stdout,"  Verifying card memory programming: End\n");
          }
          continue;

/* JDS: Review below */
     }
     return 0;
}
