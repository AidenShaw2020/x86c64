
#if 0

Usage of the works is permitted provided that this
instrument is retained with the works, so that any entity
that uses the works is notified of this instrument.

DISCLAIMER: THE WORKS ARE WITHOUT WARRANTY.

#endif

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KERNEL_CONFIG_LENGTH (32 - 2 - 4)

int main(int argc, char ** argv) {
	uint32_t size, traileroffset, ii, configlength = 0, paras = 0,
		stacksegment = 0, stackpointer = 0, imagesize;
	uint16_t ip;
	uint8_t * buffer = malloc(0xFF01);
	uint8_t config[KERNEL_CONFIG_LENGTH] = { 0 };
	uint8_t header[32] = { 'M', 'Z', 0 };
	uint8_t headeroriginal[32];
	FILE * fin;
	FILE * fout;
	if (argc < 3) {
		fprintf(stderr, "error name\n");
		return 1;
	}
	if (!buffer) {
		fprintf(stderr, "error allocating\n");
		return 1;
	}
	fin = fopen(argv[1], "rb");
	if (!fin) {
		fprintf(stderr, "error opening\n");
		return 1;
	}
	size = fread(buffer, 1, 0xFF01, fin);
	if (size == 0xFF01 || size < 64) {
		fprintf(stderr, "error reading\n");
		return 1;
	}
	fclose(fin);
/*
	if (0 == memcmp(&buffer[2], "CONFIG", 6)) {
		configlength = 8 + buffer[8] + buffer[9] * 256LU;
		if (configlength > KERNEL_CONFIG_LENGTH) {
			fprintf(stderr, "error config\n");
			return 1;
		}
		memcpy(config, &buffer[2], configlength);
	}
*/
	for (ip = 0;;) {
		if (buffer[ip] == 0xEB) {
			ii = ip + 2 + buffer[ip+1];
			ip = ii;
			if ((ip + 16LU) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
			continue;
		}
		if (buffer[ip] == 0xE9) {
			ii = ip + 3 + buffer[ip+1] + buffer[ip+2] * 256LU;
			ip = ii;
			if ((ip + 16LU) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
			continue;
		}
		break;
	}
	if (buffer[ip] == 0xB9) {
		ii = buffer[ip + 1] + buffer[ip + 2] * 256LU;
		traileroffset = ii;
		if (ii != ip) {
			fprintf(stderr, "error trailer\n");
			return 1;
		}
		ii = 0;
		ip += 3;
	} else if (buffer[ip] == 0xE8) {
		traileroffset = ip;
		ii = 40;
		++ip;
	} else {
		fprintf(stderr, "error trailer\n");
		return 1;
	}
	for ( ; ; ++ii) {
		if ((ip + 1LU) > size) {
			fprintf(stderr, "error jumping\n");
			return 1;
		}
		switch (ii) {
		case 0: {
			if (buffer[ip] != 0x0E) {
				fprintf(stderr, "error trailer\n");
				return 1;
			}
			++ip;
			continue;
		}
		case 1: {
			if (buffer[ip] != 0x1F) {
				fprintf(stderr, "error trailer\n");
				return 1;
			}
			++ip;
			continue;
		}
		case 2: {
			if (buffer[ip] != 0x8C) {
				fprintf(stderr, "error trailer\n");
				return 1;
			}
			++ip;
			continue;
		}
		case 3: {
			if (buffer[ip] != 0xC8) {
				fprintf(stderr, "error trailer\n");
				return 1;
			}
			++ip;
			continue;
		}
		case 4: {
			if (buffer[ip] == 0x48) {
				--ii;
				++paras;
				++ip;
				continue;
			}
			if (buffer[ip] != 0x8E) {
				fprintf(stderr, "error trailer\n");
				return 1;
			}
			++ip;
			continue;
		}
		case 5: {
			uint8_t sequence[] = { 0xC0, 0x93, 0x31, 0xFF, 0xFC, 0xAA, 0x8B, 0xF7, 0xF3, 0xA4 };
			uint32_t sequencelength = sizeof(sequence);
			if ((ip + sequencelength) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
			if (0 == memcmp(&buffer[ip], sequence, sequencelength)) {
				ip += sequencelength;
				continue;
			} else {
			 uint8_t sequence[] = { 0xC0, 0x31, 0xFF, 0xBE, 0x00, 0x00, 0xFC, 0xF3, 0xA4, 0x26, 0x88, 0x1E, 0x00, 0x00, 0xB8 };
			 uint32_t sequencelength = sizeof(sequence);
			 if ((ip + sequencelength) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			 }
			 if (0 != memcmp(&buffer[ip], sequence, sequencelength)) {
				fprintf(stderr, "error trailer\n");
				return 1;
			 }
			 ip += sequencelength;
			 ii = 41 - 1;
			 continue;
			}
		}
		case 6: {
			uint8_t sequence[] = {
0x55,				/* 20 push bp */
0x26, 0x8C, 0x16, 0x1E, 0x00,	/* 21 mov word [es:(#32 - 2)], ss */
0x26, 0x89, 0x26, 0x1C, 0x00,	/* 26 mov word [es:(#32 - 4)], sp */
 			};
			uint32_t sequencelength = sizeof(sequence);
			if ((ip + sequencelength) <= size
			    && 0 == memcmp(&buffer[ip], sequence, sequencelength)) {
				ip += sequencelength;
				continue;
			}
			continue;
		}
		case 7: {
			uint8_t sequence[] = { 0x1E, 0x58, 0x05 };
			uint32_t sequencelength = sizeof(sequence);
			if ((ip + sequencelength) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
			if (0 != memcmp(&buffer[ip], sequence, sequencelength)) {
				fprintf(stderr, "error trailer\n");
				return 1;
			}
			ip += sequencelength;
			continue;
		}
		case 8: {
			if ((ip + 2UL) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
			stacksegment = buffer[ip] + buffer[ip+1] * 256UL;
			ip += 2;
			continue;
		}
		case 9: {
			uint8_t sequence[] = { 0x8E, 0xD0, 0xBC };
			uint32_t sequencelength = sizeof(sequence);
			if ((ip + sequencelength) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
			if (0 != memcmp(&buffer[ip], sequence, sequencelength)) {
				fprintf(stderr, "error trailer\n");
				return 1;
			}
			ip += sequencelength;
			continue;
		}
		case 10: {
			if ((ip + 2UL) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
			stackpointer = buffer[ip] + buffer[ip+1] * 256UL;
			ip += 2;
			continue;
		}
		case 11: {
			uint8_t sequence[] = { 0x31, 0xC0, 0xFF, 0xE0 };
			uint32_t sequencelength = sizeof(sequence);
			if ((ip + sequencelength) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
			if (0 == memcmp(&buffer[ip], sequence, sequencelength)) {
				ip += sequencelength;
				break;
			} else {
				uint8_t sequence[] = { 0xFF, 0x26, 0x06, 0x00 };
				uint32_t sequencelength = sizeof(sequence);
				if ((ip + sequencelength) > size) {
					fprintf(stderr, "error jumping\n");
					return 1;
				}
				if (0 != memcmp(&buffer[ip], sequence, sequencelength)) {
					fprintf(stderr, "error trailer\n");
					return 1;
				}
				fprintf(stderr, "error device not yet supported\n");
				return 1;
			}
		}
		case 40: {
			uint8_t sequence[] = { 0x00, 0x00, 0x59, 0x0E, 0x1F, 0x8C, 0xC8 };
			uint32_t sequencelength = sizeof(sequence);
			if ((ip + sequencelength) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
			if (0 != memcmp(&buffer[ip], sequence, sequencelength)) {
				fprintf(stderr, "error trailer\n");
				return 1;
			}
			ip += sequencelength;
			ii = 4 - 1;
			continue;
		}
		case 41: {
			if ((ip + 2UL) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
			stacksegment = buffer[ip] + buffer[ip+1] * 256UL - 0x60;
			ip += 2;
			continue;
		}
		case 42: {
			uint8_t sequence[] = { 0x8E, 0xD0, 0xBC };
			uint32_t sequencelength = sizeof(sequence);
			if ((ip + sequencelength) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
			if (0 != memcmp(&buffer[ip], sequence, sequencelength)) {
				fprintf(stderr, "error trailer\n");
				return 1;
			}
			ip += sequencelength;
			continue;
		}
		case 43: {
			if ((ip + 2UL) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
			stackpointer = buffer[ip] + buffer[ip+1] * 256UL;
			ip += 2;
			continue;
		}
		case 44: {
			uint8_t sequence[] = { 0x31, 0xC0, 0x50, 0xC3 };
			uint32_t sequencelength = sizeof(sequence);
			if ((ip + sequencelength) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
			if (0 == memcmp(&buffer[ip], sequence, sequencelength)) {
				ip += sequencelength;
				break;
			} else {
				fprintf(stderr, "error trailer\n");
				return 1;
			}
		}
		default: {
			fprintf(stderr, "error trailer\n");
			return 1;
		}
		}
		break;
	}
	printf("filesize=%-6"PRIu32" traileroffset=%-6"PRIu32
		" trailersize=%-4"PRIu32
		" paras=%04"PRIX32"h"
		" ss=%04"PRIX32"h sp=%04"PRIX32"h\n",
		size, traileroffset, size - traileroffset,
		paras, stacksegment, stackpointer);
	if (paras != 2) {
		fprintf(stderr, "error paras\n");
		return 1;
	}
	imagesize = traileroffset - 16UL * paras + sizeof(header);
	ii = imagesize & 511;
	header[2] = ii & 0xFF;
	header[3] = (ii >> 8) & 0xFF;
	ii = imagesize / 512;
	if ((ii & 511) != 0) ++ii;
	header[4] = ii & 0xFF;
	header[5] = (ii >> 8) & 0xFF;
	ii = sizeof(header) / 16;
	header[8] = ii & 0xFF;
	header[9] = (ii >> 8) & 0xFF;
	ii = 0xFFFF;
	header[12] = ii & 0xFF;
	header[13] = (ii >> 8) & 0xFF;
	ii = stacksegment;
	header[14] = ii & 0xFF;
	header[15] = (ii >> 8) & 0xFF;
	ii = stackpointer;
	header[16] = ii & 0xFF;
	header[17] = (ii >> 8) & 0xFF;

	for (ii = 0; ii < 32; ++ii) {
		printf(" %02"PRIX8, header[ii]);
		if ((ii & 7) == 7)
			puts("");
	}
	memcpy(headeroriginal, header, 32);

#if 0
	header[0x2] = 0x0C;
	header[0x3] = 0x0;
	header[0x4] = 0x69;
	header[0x6] = 0x1;
	header[0xA] = 0x85;
	header[0xB] = 0x8;
	header[0x18] = 0x1C;
	header[0x1C] = 0x5;
	header[0x1D] = 0x0;
	header[0x1E] = 0xFE;
	header[0x1F] = 0x0C;
	buffer[16UL * paras + 0xCFE * 16UL + 5] -= 0x60;
#elif 0
	header[0x6] = 0x1;
	header[0x18] = 0x1C;
	header[0x1C] = 0x5;
	header[0x1D] = 0x0;
	header[0x1E] = 0xFE;
	header[0x1F] = 0x0C;
	buffer[16UL * paras + 0xCFE * 16UL + 5] -= 0x60;
#else
	{
		uint8_t sequence[] = { 0xEA, 0x00, 0x00, 0x60, 0x00 };
		uint32_t sequencelength = sizeof(sequence), segment, offset;
		for (ii = sequencelength; ii < 128; ++ii) {
			if (0 == memcmp(&buffer[traileroffset - ii], sequence, sequencelength)) {
				break;
			}
		}
		if (ii >= 128) {
			fprintf(stderr, "error unrelocating\n");
			return 1;
		}
		ii = traileroffset - ii;
		ii += 3;
		buffer[ii] = 0;
		ii -= 16UL * paras;
		segment = ii >> 4UL;
		offset = ii & 15UL;
		header[0x6] = 0x1;
		header[0x18] = 0x1C;
		header[0x1C] = offset;
		header[0x1E] = segment & 0xFF;
		header[0x1F] = (segment >> 8) & 0xFF;
	}
#endif

	for (ii = 0; ii < 32; ++ii) {
		printf("%c%02"PRIX8,
			(header[ii] == headeroriginal[ii] ? ' ' : '*'),
			header[ii]);
		if ((ii & 7) == 7)
			puts("");
	}

	fout = fopen(argv[2], "wb");
	if (!fout) {
		fprintf(stderr, "error creating\n");
		return 1;
	}
	ii = fwrite(header, 1, sizeof(header), fout);
	if (ii != sizeof(header)) {
		fprintf(stderr, "error writing\n");
		return 1;
	}
	ii = fwrite(buffer + 16UL * paras, 1, imagesize - sizeof(header), fout);
	if (ii != (imagesize - sizeof(header))) {
		fprintf(stderr, "error writing\n");
		return 1;
	}

	fclose(fout);
	{
		char commandbuffer[256];
		char * ext;
		uint32_t namelength = strlen(argv[2]);
		if (namelength < 4) {
			fprintf(stderr, "error name extension\n");
			return 1;
		}
		ext = &argv[2][namelength - 4];
		if (0 != strcmp(ext, ".exe")) {
			fprintf(stderr, "error name extension\n");
			return 1;
		}
		snprintf(commandbuffer, sizeof(commandbuffer), "upx -d %s", argv[2]);
		system(commandbuffer);
		fout = fopen(argv[2], "r+b");
		if (!fout) {
			fprintf(stderr, "error re-opening\n");
			return 1;
		}
		ii = fread(header, 1, sizeof header, fout);
		if (ii != sizeof header) {
			fprintf(stderr, "error reading\n");
			return 1;
		}
		if (header[0] != 'M' || header[1] != 'Z') {
			fprintf(stderr, "error header\n");
			return 1;
		}
		ii = header[8] + header[9] * 256UL;
		ii *= 16UL;

		fseek(fout, ii, SEEK_SET);
		size = fread(buffer, 1, 0x100, fout);
		if (size < 64) {
			fprintf(stderr, "error reading\n");
			return 1;
		}
		ip = 0;
	{
		uint32_t ii;
		if (buffer[ip] == 0xEB) {
			ii = ip + 2 + buffer[ip+1];
			ip = ii;
			if ((ip + 16LU) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
		} else if (buffer[ip] == 0xE9) {
			ii = ip + 3 + buffer[ip+1] + buffer[ip+2] * 256LU;
			ip = ii;
			if ((ip + 16LU) > size) {
				fprintf(stderr, "error jumping\n");
				return 1;
			}
		}
	}
		if (buffer[ip] == 0xF9) { /* stc */
			buffer[ip] = 0xF8; /* clc */
			printf("Undid signal patch\n");
			fseek(fout, ii + ip, SEEK_SET);
			ii = fwrite(&buffer[ip], 1, 1, fout);
			if (ii != 1) {
				fprintf(stderr, "error rewriting\n");
				return 1;
			}
		}
/*
		ii = header[8] + header[9] * 256UL;
		ii *= 16UL;
		fseek(fout, ii + 2UL, SEEK_SET);
		fwrite(config, 1, configlength, fout);
*/
		fclose(fout);
	}
	return 0;
}
