/* Copyright (C) 2003-2010  John Whitney
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: John Whitney <jjw@deltup.org>
 */

#include <stdio.h>
#include <string.h>
#include "file.h"
#include "compatibility.h"

bool copy_bytes_to_file(FILE *infile, FILE *outfile, unsigned numleft) {
	size_t numread;
	do {
		char buf[1024];
		numread = fread(buf, 1, numleft > 1024 ? 1024 : numleft, infile);
		if (fwrite(buf, 1, numread, outfile) != numread) {
			printf("Could not write temporary data.  Possibly out of space\n");
			return false;
		}
		numleft -= numread;
	} while (numleft && !(numread < 1024 && numleft));
	return (numleft == 0);
}

int main(int argc, char **argv) {
	if (argc != 4) {
		printf("needs a reference file, file to output, and patchfile:\n");
		printf("delta oldfile newfile patchfile\n");
		return 1;
	}

	if (!fileExists(argv[1]) || !fileExists(argv[3])) {
		printf("one of the input files does not exist\n");
		return 1;
	}

	FILE *patchfile = fopen(argv[3], "rb");
	char magic[3];
	fread_fixed(patchfile, magic, 3);
	if (strncmp(magic, "BDT", 3)) {
		printf("Given file is not a recognized patchfile\n");
		return 1;
	}
	unsigned short version = read_word(patchfile);
	if (version != 1) {
		printf("unsupported patch version\n");
		return 1;
	}
	char intsize;
	fread_fixed(patchfile, &intsize, 1);
	if (intsize != 4) {
		printf("unsupported file pointer size\n");
		return 1;
	}
	unsigned size1 = read_dword(patchfile), 
		size2 = read_dword(patchfile);

	unsigned nummatches = read_dword(patchfile);

	unsigned * copyloc1 = new unsigned[nummatches + 1];
	unsigned * copyloc2 = new unsigned[nummatches + 1];
	unsigned *  copynum = new unsigned[nummatches + 1];

	for (unsigned i = 0; i < nummatches; ++i) {
		copyloc1[i] = read_dword(patchfile);
		copyloc2[i] = read_dword(patchfile);
		copynum[i] = read_dword(patchfile);
		size2 -= copyloc2[i] + copynum[i];
	}
	if (size2) {
		copyloc1[nummatches] = 0; copynum[nummatches] = 0; 
		copyloc2[nummatches] = size2;
		++nummatches;
	}

	FILE *ref = fopen(argv[1], "rb");
	FILE *outfile = fopen(argv[2], "wb");

	for (unsigned i = 0; i < nummatches; ++i) {
		if (!copy_bytes_to_file(patchfile, outfile, copyloc2[i])) {
			printf("Error.  patchfile is truncated\n");
			return -1;
		}

		int copyloc = copyloc1[i];
		fseek(ref, copyloc, SEEK_CUR);

		if (!copy_bytes_to_file(ref, outfile, copynum[i])) {
			printf("Error while copying from reference file\n");
			return -1;
		}
	}

	delete [] copynum;
	delete [] copyloc2;
	delete [] copyloc1;

	return 0;  
}
