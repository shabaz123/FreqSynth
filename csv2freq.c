// csv2freq.c
// Converts a CSV list of frequencies (in Hertz) into an array of
// frequencies (as integers).
// rev 1.0 Shabaz December 2013

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYSFREQ 400E6
#define TWLEN 4294967296

void freq2ftw(double freq, unsigned char* data);

int
main(int argc, char *argv[])
{
	FILE* infile;
	FILE* outfile;
	unsigned char c;
	unsigned char freq_s[128];
	double freq;
	unsigned char data[4];
	int freq_i;
	int ret;
	int notfirst=0;
	int ctr=0;
	int i;
	
	if (argc<2)
	{
		fprintf(stderr, "%s: Error: Invalid args\n", argv[0]);
		fprintf(stderr, "Syntax is: %s <infile> [outfile]\n", argv[0]);
		exit(1);
	}
	infile=fopen(argv[1], "rb");
	if (infile==NULL)
	{
		fprintf(stderr, "%s: Error: Unable to open input file '%s'\n", argv[0], argv[1]);
		exit(1);
	}
		
	if (argc>2)
	{
		outfile=fopen(argv[2], "w");
	}
	else
	{
		outfile=fopen("out.txt", "w");
	}
	if (outfile==NULL)
	{
		fprintf(stderr, "%s: Error: Unable to create output file\n", argv[0]);
		exit(1);
	}
	
	// read until we see a comma or EOF
	// Once we see comma or EOF, we can convert to FTW format
	i=0;
	while(1)
	{
		ret=fread(&c, 1, 1, infile);
		if ((ret!=1) || (c==',') || (c=='\n') || c=='\r' || (c==' '))
		{
			// We have seen a separator or whitespace or EOF
			if (i==0) // no characters read?
			{
				if (ret!=1) // EOF
				{
					// we're done
					break;
				}
				continue;
			}
			else
			{
				// some characters were read. Convert to FTW
				freq_s[i]='\0'; // terminate the string
				i=0;
				sscanf(freq_s, "%lf", &freq);
				freq_i=(int)freq;
				//freq2ftw(freq, data); // data array now contains the 4-byte FTW
				if (notfirst)
				{
					fprintf(outfile, ", ");
				}
				else
				{
					notfirst=1;
				}
				if (ctr%10==0) // do a newline every so often
				{
					fprintf(outfile, "\n");
				}
				ctr++;
				fprintf(outfile, "%d", freq_i);
			}
		}
		else
		{
			freq_s[i]=c;
			i++;
		}
	} // end while(1)
	
	fclose(infile);
	fclose(outfile);
	
	return(0);
}

// converts a frequency into the 4 data bytes that represent it
void
freq2ftw(double freq, unsigned char* data)
{
	unsigned char* tptr;
	unsigned int tword; // 32-bit tuning word
	
	tptr=(unsigned char*)&tword;
	tword=(unsigned int)((freq)*TWLEN/(SYSFREQ));
	data[3]=*tptr; // adjust these lines for endian'ness
	data[2]=*(tptr+1);
	data[1]=*(tptr+2);
	data[0]=*(tptr+3);
}
