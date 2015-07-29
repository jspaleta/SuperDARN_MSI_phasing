optimize_beam_programming

./optimize_beam_programming -h
Required:
  -r radarname
  -n dio radar number (1 or 2)
  -c card number
Optional:
  -a vna ipaddress
  -p vna tcp port
  -i to run vna init and cal process
  -v number to set verbose output level
  -s user@host to enable ssh based write/verify


Frequency optimized phasing replaces the write_final_beamcodes step and relies on the normal card characterization procedure and beam programming procedure as a starting point and then adjusts the phasing and attenuation in a frequency band of interest using the VNA measurements as a guide.

Env variables:
MSI_CALDIR: sets the directory with calibration data.
For example when working aze radar calibrations using this filesystem directory: /data/calibrations/aze
MSI_CALDIR=/data/calibrations

Inputs:
optimize_beam_programming uses as inputs:
beamcodes_cal_<rad>_<slot>.dat files produced by beamcode_generator 
<rad> is 3 letter code
<slot> is zero padded 2 digit number

Outputs:
optcodes_cal_<rad>_<slot>.dat
<rad> is 3 letter code
<slot> is zero padded 2 digit number


Note:
optimize_beam_programming must be run on all cards in an array with self consistent memory layout configuration (MSI_functions.c) to ensure correct beam/frequency lookup. MSI_functions.c is site specific!


optimize_beam_verify:  
Can be used to verify and re-write card memory using the optcodes_cal files as inputs.
Can be used to write and verify lookup table to be used by dio driver to select best card memory location for bmnum,frequency pair.

./optimize_beam_verify -h
Required:
  -r <radarname> : 3-letter radarcode 
  -n <number> :dio radar number (1 or 2)
  -c <number> :card number (0-19)
Optional:
  -W :flag  to enable write to card memory
  -V :flag to enable verify card memory
  -m  <memloc> : specify single memory location
  -F :flag  to enable write of lookup table
  -v <number> :to set verbose output level
  -s <user@host> :to enable ssh based write/verify


Examples:
to verify aze card 15 is programmed as expected compared to optcodes_cal_aze_15.dat
optimize_beam_verify -r aze -n 1 -c 15 -V

to write aze card 15 memory using optcodes_cal_aze_15.dat
optimize_beam_verify -r aze -n 1 -c 15 -W

To write lookup table for aze radar (opt_lookup_aze.dat) based on information in optcodes_cal_aze_00.dat
optimize_beam_verify -r aze -n 1 -c 0 -F

To verify lookup table for aze radar (opt_lookup_aze.dat) is self-consistent with information in optcodes_cal_aze_10.dat
optimize_beam_verify -r aze -n 1 -c 10 

