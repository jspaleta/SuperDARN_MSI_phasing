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
/* helper functions  for vna */
#include "vna_functions.h"
/* Useful defines for MSI phasing cards */
#include "MSI_defines.h"

void main() {

  fprintf(stdout,"Hello Word\n");

}
