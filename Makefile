OS:=$(shell uname -s)

CC=gcc
CFLAGS=-B /usr/ -c
common_libs=-lm
#linux_libs=-lgsl -lgslcblas
linux_libs=
qnx_libs=-lsocket
INCLUDES=-I"/usr/include/" -I"../include/" -I"include/" -I"../tsg/include"   

SOURCES=main.c  _open_PLX9050.c common_functions.c _prog_conventions.c utils.c 
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=phasing_calibration

ATTEN_SOURCES=atten_calibration.c  _open_PLX9050.c _prog_conventions.c utils.c 
ATTEN_OBJECTS=$(ATTEN_SOURCES:.c=.o)
ATTEN_EXECUTABLE=atten_calibration

#RF_SOURCES=rf_cal.c  _open_PLX9050.c _prog_conventions.c utils.c 
#RF_OBJECTS=$(RF_SOURCES:.c=.o)
#RF_EXECUTABLE=rf_calibration

CONVERT_SOURCES=time_delay.c 
CONVERT_OBJECTS=$(CONVERT_SOURCES:.c=.o)
CONVERT_EXECUTABLE=time_delay

VERIFY_SOURCES=verifier.c 
VERIFY_OBJECTS=$(VERIFY_SOURCES:.c=.o)
VERIFY_EXECUTABLE=verifier

ATTEN_VERIFY_SOURCES=atten_verifier.c 
ATTEN_VERIFY_OBJECTS=$(ATTEN_VERIFY_SOURCES:.c=.o)
ATTEN_VERIFY_EXECUTABLE=atten_verifier

VERIFY_PROGRAMMED_SOURCES=verify_programmed_beamcodes.c common_functions.c _open_PLX9050.c _prog_conventions.c utils.c 
VERIFY_PROGRAMMED_OBJECTS=$(VERIFY_PROGRAMMED_SOURCES:.c=.o)
VERIFY_PROGRAMMED_EXECUTABLE=verify_programmed_beamcodes

CHECK_PROGRAMMED_SOURCES=check_programmed_beamcodes.c _open_PLX9050.c _prog_conventions.c utils.c 
CHECK_PROGRAMMED_OBJECTS=$(CHECK_PROGRAMMED_SOURCES:.c=.o)
CHECK_PROGRAMMED_EXECUTABLE=check_programmed_beamcodes

CHECK_RADAR_SOURCES=check_radar_settings.c _open_PLX9050.c _prog_conventions.c utils.c 
CHECK_RADAR_OBJECTS=$(CHECK_RADAR_SOURCES:.c=.o)
CHECK_RADAR_EXECUTABLE=check_radar_settings

SELECT_BEAM_SOURCES=select_beam.c _open_PLX9050.c _prog_conventions.c utils.c 
SELECT_BEAM_OBJECTS=$(SELECT_BEAM_SOURCES:.c=.o)
SELECT_BEAM_EXECUTABLE=select_beam

COMPARE_SOURCES=time_compare.c 
COMPARE_OBJECTS=$(COMPARE_SOURCES:.c=.o)
COMPARE_EXECUTABLE=time_delay_compare

BEAM_SOURCES=beamcode_generator.c 
BEAM_OBJECTS=$(BEAM_SOURCES:.c=.o)
BEAM_EXECUTABLE=beamcode_generator

SUMMARY_SOURCES=summary_generator.c 
SUMMARY_OBJECTS=$(SUMMARY_SOURCES:.c=.o)
SUMMARY_EXECUTABLE=summary_generator

FINAL_SOURCES=write_final_beamcodes.c  common_functions.c _open_PLX9050.c _prog_conventions.c utils.c 
FINAL_OBJECTS=$(FINAL_SOURCES:.c=.o)
FINAL_EXECUTABLE=write_final_beamcodes

SELECT_SOURCES=select_final_beamcodes.c    
SELECT_OBJECTS=$(SELECT_SOURCES:.c=.o)
SELECT_EXECUTABLE=select_final_beamcodes

DIO_OUTPUT_SOURCES=dio_output_test.c _open_PLX9050.c _prog_conventions.c utils.c 
DIO_OUTPUT_OBJECTS=$(DIO_OUTPUT_SOURCES:.c=.o)
DIO_OUTPUT_EXECUTABLE=dio_output_test

ATTEN_TEST_SOURCES=attenuator_test.c _open_PLX9050.c _prog_conventions.c utils.c 
ATTEN_TEST_OBJECTS=$(ATTEN_TEST_SOURCES:.c=.o)
ATTEN_TEST_EXECUTABLE=attenuator_test

all: $(SOURCES) $(EXECUTABLE) $(SUMMARY_SOURCES) $(SUMMARY_EXECUTABLE) $(CONVERT_SOURCES) $(CONVERT_EXECUTABLE) $(BEAM_SOURCES) $(BEAM_EXECUTABLE) $(FINAL_EXECUTABLE) $(RF_SOURCES) $(RF_EXECUTABLE) $(VERIFY_PROGRAMMED_SOURCES) $(VERIFY_PROGRAMMED_EXECUTABLE) 

$(EXECUTABLE): $(OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(OBJECTS) $(common_libs) $(qnx_libs) 
endif

$(RF_EXECUTABLE): $(RF_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(RF_OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(RF_OBJECTS) $(common_libs) $(qnx_libs) 
endif

$(VERIFY_EXECUTABLE): $(VERIFY_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(VERIFY_OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(VERIFY_OBJECTS) $(common_libs) $(qnx_libs) 
endif

$(VERIFY_PROGRAMMED_EXECUTABLE): $(VERIFY_PROGRAMMED_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(VERIFY_PROGRAMMED_OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(VERIFY_PROGRAMMED_OBJECTS) $(common_libs) $(qnx_libs) 
endif

$(CHECK_PROGRAMMED_EXECUTABLE): $(CHECK_PROGRAMMED_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(CHECK_PROGRAMMED_OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(CHECK_PROGRAMMED_OBJECTS) $(common_libs) $(qnx_libs) 
endif

$(CHECK_RADAR_EXECUTABLE): $(CHECK_RADAR_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(CHECK_RADAR_OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(CHECK_RADAR_OBJECTS) $(common_libs) $(qnx_libs) 
endif

$(SELECT_BEAM_EXECUTABLE): $(SELECT_BEAM_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(SELECT_BEAM_OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(SELECT_BEAM_OBJECTS) $(common_libs) $(qnx_libs) 
endif

$(CONVERT_EXECUTABLE): $(CONVERT_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(CONVERT_OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -L/usr/lib/ -o $@ $(CONVERT_OBJECTS) $(common_libs) $(qnx_libs) -lgsl -lgslcblas 
endif

$(COMPARE_EXECUTABLE): $(COMPARE_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(COMPARE_OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(COMPARE_OBJECTS) $(common_libs) $(qnx_libs) 
endif

$(BEAM_EXECUTABLE): $(BEAM_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(BEAM_OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(BEAM_OBJECTS) $(common_libs) $(qnx_libs) 
endif

$(SUMMARY_EXECUTABLE): $(SUMMARY_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(SUMMARY_OBJECTS) $(common_libs) $(linux_libs)
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(SUMMARY_OBJECTS) $(common_libs) $(qnx_libs)
endif

$(FINAL_EXECUTABLE): $(FINAL_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(FINAL_OBJECTS) $(common_libs) $(linux_libs)
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(FINAL_OBJECTS) $(common_libs) $(qnx_libs)
endif

$(SELECT_EXECUTABLE): $(SELECT_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(SELECT_OBJECTS) $(common_libs) $(linux_libs)
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(SELECT_OBJECTS) $(common_libs) $(qnx_libs)
endif

$(DIO_OUTPUT_EXECUTABLE): $(DIO_OUTPUT_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(DIO_OUTPUT_OBJECTS) $(common_libs) $(linux_libs)
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(DIO_OUTPUT_OBJECTS) $(common_libs) $(qnx_libs)
endif

$(ATTEN_TEST_EXECUTABLE): $(ATTEN_TEST_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(ATTEN_TEST_OBJECTS) $(common_libs) $(linux_libs)
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(ATTEN_TEST_OBJECTS) $(common_libs) $(qnx_libs)
endif

$(ATTEN_EXECUTABLE): $(ATTEN_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(ATTEN_OBJECTS) $(common_libs) $(linux_libs)
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(ATTEN_OBJECTS) $(common_libs) $(qnx_libs)
endif

$(ATTEN_VERIFY_EXECUTABLE): $(ATTEN_VERIFY_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(ATTEN_VERIFY_OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(ATTEN_VERIFY_OBJECTS) $(common_libs) $(qnx_libs) 
endif

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ 

clean:
	rm -rf $(OBJECTS) $(EXECUTABLE) $(SUMMARY_OBJECTS) $(SUMMARY_EXECUTABLE) $(VERIFY_PROGRAMMED_OBJECTS) $(VERIFY_PROGRAMMED_EXECUTABLE) $(VERIFY_OBJECTS) $(VERIFY_EXECUTABLE) $(CONVERT_OBJECTS) $(CONVERT_EXECUTABLE) $(BEAM_OBJECTS) $(BEAM_EXECUTABLE) $(RF_EXECUTABLE) $(RF_OBJECTS) ${FINAL_EXECUTABLE} ${FINAL_OBJECTS}


