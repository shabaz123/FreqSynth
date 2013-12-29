// dds.c
// DDS control program for AD9954 board
// rev 1 - December 2013 - Shabaz

// Note: The BBB will not boot up with these pins connected. So for
// a permanent solution, either pick different pins or install a
// tri-statable buffer

// DDS board          WireColor			BBB
// ------------       -----------   -----------------
// 5 - VIN            RED           P9_5    VDD_5V
// 6 - IO_UPD         PURPLE        P8_39
// 7 - GND            BLACK         P9_1    DGND
// 8 - SCLK           ORANGE        P8_40
// 10 - SDIO          YELLOW        P8_41
// 14 - *CS           BLUE          P8_42
// 16 - RESET         WHITE         P8_43
// 
//
// Some example syntax:
// ./dds --mode single-freq --freq 10000000
// ./dds --brief --mode single-freq --freq 10000000
// ./dds --mode single-freq --freq 10000000 --rel-level -3.0
// ./dds --mode single-freq --freq 10000000 --dbm-level -11.0
// ./dds --mode single-freq --freq 10000000 --int-level 16383
// ./dds --mode fmtone --freq 10000000 --tone 880
// ./dds --mode fmtone --freq 10000000 --tone 1100 --tone2 2100


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include "BBBiolib.h"

// definitions (these are all GPIO2)
#define IO_UPD_LOW pin_low(8, 39)
#define IO_UPD_HIGH pin_high(8, 39)
#define SCLK_LOW pin_low(8, 40)
#define SCLK_HIGH pin_high(8, 40)
#define SDIO_LOW pin_low(8, 41)
#define SDIO_HIGH pin_high(8, 41)
#define CS_BAR_LOW pin_low(8, 42)
#define CS_BAR_HIGH pin_high(8, 42)
#define RESET_LOW pin_low(8, 43)
#define RESET_HIGH pin_high(8, 43)
// DDS registers
#define CFR1 0
#define CFR2 1
#define ASF 2
#define ARR 3
#define FTW0 4
#define FTW1 6
#define RSCW0 7
#define RAM 0x0b

#define SYSFREQ 400E6
#define TWLEN 4294967296
#define MAXDBM -8.0
#define SINGLE_FREQ 1
#define FMTONE 2

// This disables the comparator. Set this to zero if
// square wave output is required!
#define COMPDIS 1

// function prototypes
void write_bytes(unsigned char instr, unsigned char* data, int len);
void set_sysfreq(void);
void set_amplitude(unsigned int amp);
void freq2ftw(double freq, unsigned char* data, int dbg);
void set_tone_100(double tonefreq);
unsigned int db2asf(double dblevel);

// Length 100 array containing 2pi of sinewave with p-p amplitude of 5000
const int
f_array[100]={
	0, 156, 313, 468, 621, 772, 920, 1064, 1204, 1339, 
	1469, 1593, 1711, 1822, 1926, 2022, 2110, 2190, 2262, 2324, 
	2377, 2421, 2455, 2480, 2495, 2500, 2495, 2480, 2455, 2421, 
	2377, 2324, 2262, 2190, 2110, 2022, 1926, 1822, 1711, 1593, 
	1469, 1339, 1204, 1064, 920, 772, 621, 468, 313, 156, 
	0, -156, -313, -468, -621, -772, -920, -1064, -1204, -1339, 
	-1469, -1593, -1711, -1822, -1926, -2022, -2110, -2190, -2262, -2324, 
	-2377, -2421, -2455, -2480, -2495, -2500, -2495, -2480, -2455, -2421, 
	-2377, -2324, -2262, -2190, -2110, -2022, -1926, -1822, -1711, -1593, 
	-1469, -1339, -1204, -1064, -920, -772, -621, -468, -313, -156};

// set to zero to disable debug, 1 to enable debug
int dds_dbg=1;

int
main(int argc, char *argv[])
{
	unsigned int tword; // 32-bit tuning word
	double freq; // desired frequency
	double freq0, freq1; // desired frequency ranges for sweep type modes
	double tone, tone2;
	double incr;
	double dblevel;
	unsigned int asf;
	unsigned char data[1024];
	char mode_s[32];
	int mode=0;
	int i;
	int c;
	int longarg=0;

	// set default settings
	mode=SINGLE_FREQ;
	asf=16383; // max amplitude is (2^14)-1 i.e. 16383
	freq=10000000; // default to a 10MHz signal
	tone=0;
	tone2=0;
	
	// Parse arguments
	while (1)
  {
  	static struct option long_options[] =
    {
      {"debug", no_argument,       &dds_dbg, 1},
      {"brief",   no_argument,       &dds_dbg, 0},
      {"null",     no_argument,       0, 'n'},
      {"tone",  required_argument, 0, 't'},
      {"tone2",  required_argument, 0, 'u'},
      {"mode",  required_argument, 0, 'm'},
      {"freq",  required_argument, 0, 'f'},
      {"dbm-level",  required_argument, 0, 'd'},
      {"rel-level",    required_argument, 0, 'r'},
      {"int-level",    required_argument, 0, 'i'},
      {0, 0, 0, 0}
    };
  	int option_index = 0;
  	c = getopt_long (argc, argv, "nt:u:m:f:d:r:i:",
                   long_options, &option_index);
  	if (c == -1)
    	break;

  	switch (c)
    {
    	case 0:
      	if (long_options[option_index].flag != 0)
        	break;
      	printf ("option %s", long_options[option_index].name);
      	if (optarg)
        	printf (" with arg %s", optarg);
      	printf ("\n");
      	break;
      	
    	case 'n':
      	// do nothing with this for now
      	break;

    	case 't':
    		sscanf(optarg, "%lf", &tone);
      	break;

    	case 'u':
      	sscanf(optarg, "%lf", &tone2);
      	break;

    	case 'm':
    		if (strcmp(optarg, "fmtone")==0)
    			mode=FMTONE;
    		else if (strcmp(optarg, "single-freq")==0)
    			mode=SINGLE_FREQ;
      	break;
      
      case 'f':
      	sscanf(optarg, "%lf", &freq);
      	if (freq>200E6)
				{
					fprintf(stderr, "%s: Error: Frequency needs to be between 0.0 and 200000000.0 (0.0-200.0MHz)\n", argv[0]);
					exit(1);
				}
				if (freq>160E6)
				{
					fprintf(stderr, "%s: Warning: Frequency is higher than the recommended limit of 160.0MHz\n", argv[0]);
				}
      	break;
			
			case 'r':
      	sscanf(optarg, "%lf", &dblevel);
      	// 'relative' dblevel of 0.0 equates to max output
      	asf=db2asf(dblevel);
      	break;
      	
      case 'd':
      	sscanf(optarg, "%lf", &dblevel);
      	// 'dbm' dblevel is actual dBm value. We subtract MAXDBM to get a relative value
      	asf=db2asf(dblevel-(MAXDBM));
      	break;
      	
      case 'i':
      	sscanf(optarg, "%u", &asf);
      	break;

    	case '?':
      	break;

    	default:
      	abort ();
    }
  }

	// Do iolib initialisation
	iolib_init();
  BBBIO_sys_Enable_GPIO(BBBIO_GPIO2);
	iolib_setdir(8, 39, BBBIO_DIR_OUT);
	iolib_setdir(8, 40, BBBIO_DIR_OUT);
	iolib_setdir(8, 41, BBBIO_DIR_OUT);
	iolib_setdir(8, 42, BBBIO_DIR_OUT);
	iolib_setdir(8, 43, BBBIO_DIR_OUT);
	
	// Reset the AD9954 and get the pins into a known state
	RESET_HIGH;
	CS_BAR_HIGH; // initialize *CS to be high
	IO_UPD_LOW; // initialise IO_UPD to be low
	SCLK_LOW;	// initialise SCLK to be low (data is toggled in on the rising edge)
	RESET_LOW; // bring the DDS board out of reset

	// Perform the desired action
	if (mode==SINGLE_FREQ)
	{	
		data[0]=0x06; // OSK Enable; we need this to have amplitude control
		data[1]=0; data[2]=0x02; data[3]=0x00; // We always stick to MSB first mode. 0x42
		if (COMPDIS) data[3]|=0x40; // disable comparator
		write_bytes(CFR1, data, 4);
		
		data[0]=255;
		write_bytes(ARR, data, 1); // set amplitude ramp rate register
		
		set_sysfreq();
		set_amplitude(asf); // 0x3fff is max amplitude
		
		freq2ftw(freq, data, dds_dbg); // convert frequency into 4 data bytes (frequency tuning word)
		write_bytes(FTW0, data, 4);
		if (dds_dbg) printf("done!\n");
	}
	else if (mode==FMTONE)
	{
		if (tone==0)
			tone=1000; // default to 1kHz tone if none was provided
			
		data[0]=0x06; // OSK Enable; we need this to have amplitude control
		data[0]|=0x80; data[1]=0; data[2]=0x02; data[3]=0x00; // RAM enable
		if (COMPDIS) data[3]|=0x40; // disable comparator
		write_bytes(CFR1, data, 4);
		
		data[0]=255;
		write_bytes(ARR, data, 1); // set amplitude ramp rate register
		
		set_sysfreq();
		set_amplitude(asf); // 0x3fff is max amplitude

		set_tone_100(tone);
		
		for (i=0; i<100; i++)
		{
			//We add to the carrier freq the value of the sinewave 
			freq2ftw(freq+f_array[i], &data[i*4], 0); // convert frequency into 4 data bytes (frequency tuning word)
		}
		write_bytes(RAM, data, 400);
		if (dds_dbg) printf("done FM tone mode!\n");
			
		if (tone2>0)
		{
			// alt tone mode
			if (dds_dbg) printf("Playing alternate tones..\n");
			while(1)
			{
				iolib_delay_ms(500);
				set_tone_100(tone2);
				iolib_delay_ms(500);
				set_tone_100(tone);
			}
		}
	}
	

	return(0);
}


// Convert from a relative level in dB (where 0dB is max amplitude)
// to an amplitude scale factor between 0 and 16383 (0x3FFF) (16383 is max amplitude)
unsigned int db2asf(double dblevel)
{
	unsigned int amp_i;
	double amp;
	amp=pow(10.0, dblevel/20.0)*16383.0;
	amp_i=(unsigned int)amp;
	if (dds_dbg) printf("Amplitude converted to asf %u\n", amp_i);
		
	return(amp_i);
}

// Set address ramp rate to generate a specific frequency-modulated tone
void
set_tone_100(double tonefreq)
{
	unsigned char data[5];
	double rr;
	unsigned int arr;
	unsigned char* aptr;
		
	rr=(1.0/tonefreq)*1000000.0;
	arr=(unsigned int)rr;
	aptr=(unsigned char*)&arr;
	
	// set the address ramp rate
	data[0]=*aptr;
	data[1]=*(aptr+1);	

	// these set the beginning address to zero and end address to 99
	data[2]=0x63; data[3]=0x00; data[4]=0x80; 

	write_bytes(RSCW0, data, 5);
}


// Sets SYSFREQ to 400MHz
void
set_sysfreq(void)
{
	unsigned char data[3];
	data[0]=0x18; data[1]=0; data[2]=0x24; // 100MHz external oscillator, multiplied by 4 for 400MHz SYSFREQ
	write_bytes(CFR2, data, 3);
}

// Sets the amplitude register. Range is 0 (lowest) to 0x3fff (highest)
void
set_amplitude(unsigned int amp)
{
	unsigned char data[2];
	unsigned char* aptr;
	aptr=(unsigned char*)&amp;
	if (amp>0x3fff)
	{
		fprintf(stderr, "Error: Amplitude value should not be higher than 0x3fff!\n");
		exit(1);
	}
	data[1]=*aptr; // adjust these lines for endian'ness
	data[0]=*(aptr+1);
	if (dds_dbg) printf("Amplitude bytes in hex are '%02x%02x'\n", data[0], data[1]);
	write_bytes(ASF, data, 2);
}

// converts a frequency into the 4 data bytes that represent it
void
freq2ftw(double freq, unsigned char* data, int dbg)
{
	unsigned char* tptr;
	unsigned int tword; // 32-bit tuning word
	
	tptr=(unsigned char*)&tword;
	tword=(unsigned int)((freq)*TWLEN/(SYSFREQ));
	if (dbg) printf("Tuning word for %lfHz is %d\n", freq, tword);
	data[3]=*tptr; // adjust these lines for endian'ness
	data[2]=*(tptr+1);
	data[1]=*(tptr+2);
	data[0]=*(tptr+3);
	if (dbg) printf("Tuning word in hex is '%02x%02x %02x%02x'\n", data[0], data[1], data[2], data[3]);
}

// Sends instruction and then data to the AD9954
void
write_bytes(unsigned char instr, unsigned char* data, int len)
{
	int i, j;
	unsigned char dbyte, ibyte;
	
	ibyte=instr;
	CS_BAR_LOW;
	// transmit instruction byte
	for (i=0; i<8; i++)
	{
		if (ibyte & 0x80)
		{
			SDIO_HIGH;
		}
		else
		{
			SDIO_LOW;
		}
		ibyte=ibyte<<1;
		SCLK_HIGH;		
		SCLK_LOW;
	}
	
	// transmit data byte(s)
	for (j=0; j<len; j++)
	{
		dbyte=data[j];
		for (i=0; i<8; i++)
		{
			if (dbyte & 0x80)
			{
				SDIO_HIGH;
			}
			else
			{
				SDIO_LOW;
			}
			dbyte=dbyte<<1;
			SCLK_HIGH;
			SCLK_LOW;
		}
	}

	IO_UPD_HIGH; // toggle IO_UPD to transfer the data into the registers
	iolib_delay_ms(1);
	IO_UPD_LOW;

	//CS_BAR_HIGH;
}

