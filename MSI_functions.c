/* 
 * Calculate the timedelay needed for a given angle relative to boresite
 *   angle_degress: angle in degrees
 *   spacing_meters: antenna spacing in meters
 *   card: phasing card number numbered from 0. Card 0 is west most phasing card
 * Note:
 *   Assumes 16 cards in main array, numbered 0-15
 *   Assumes 4 cards in interf array, numbered 16-19
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>

double MSI_timedelay_needed(double angle_degrees,double spacing_meters,int32_t card) {

/*
 *  * *  angle from broadside (degrees)  spacing in meters
 *   * */
  double deltat=0;
  double needed=0;
  double c=0.299792458; // meters per nanosecond
  int32_t antenna=-1;
  double radians=0.0;
  if (card > 15) antenna=card-10;
  else antenna=card;
  deltat=(spacing_meters/c)*sin((fabs(angle_degrees)*3.14159)/180.0); //nanoseconds
  if (angle_degrees > 0) needed=antenna*deltat;
  if (angle_degrees < 0) needed=(15-antenna)*deltat;
  if (needed < 0) {
    fprintf(stderr,"Error in Time Needed Calc: %lf %lf\n",needed,deltat);
  }
  return needed;
}

