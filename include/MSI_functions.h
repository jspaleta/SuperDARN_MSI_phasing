#include <stdint.h>
/* settings which I could probably move to an ini file */
int32_t    MSI_num_angles=24;
int32_t    MSI_num_cards=20;
double     MSI_bm_sep_degrees=3.24;
double     MSI_spacing_meters=12.8016;
double     MSI_max_freq=20E6;
double     MSI_min_freq=8E6;
double     MSI_lo_freq=10E6;
double     MSI_hi_freq=16E6;
int32_t    MSI_max_freq_steps=128;
double     MSI_freq_window=.25*1E6;
double     MSI_target_pwr_dB=-2.0;
double     MSI_target_tdelay0_nsecs=10.0;


/* Hardwired stuff */

double MSI_timedelay_bits_nsecs[13]={
                         0.25, 
                         0.45, 
                         0.8, 
                         1.5,
                         2.75,
                         5.0,
                         8.0,
                         15.0,
                         25.0,
                         45.0,
                         80.0,
                         140.0,
                         250.0
                       };

double MSI_atten_bits_nsecs[6]={
                         0.5, 
                         1.0, 
                         2.0, 
                         4.0, 
                         8.0, 
                         16.0, 
                       };  
double MSI_timedelay_needed(double angle_degrees,double spacing_meter,int32_t card);
