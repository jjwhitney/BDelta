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
#include <stdlib.h>
#include "bdelta.h"
#include "file.h"
#include "compatibility.h"

void *f_read(void *f, void *buf, unsigned place, unsigned num) {
	fseek((FILE *)f, place, SEEK_SET);
	fread_fixed((FILE *)f, buf, num);
	return buf;
}

int main(int argc, char **argv) {
	if (argc != 4) {
		printf("needs two files to compare + output file:\n");
		printf("delta oldfile newfile patchfile\n");
		exit(1);
	}
	if (!fileExists(argv[1]) || !fileExists(argv[2])) {
		printf("one of the input files does not exist\n");
		exit(1);
	}
	unsigned size = getLenOfFile(argv[1]); 
	unsigned size2 = getLenOfFile(argv[2]);
	FILE *f1 = fopen(argv[1], "rb"),
	     *f2 = fopen(argv[2], "rb");

	void *b = bdelta_init_alg(size, size2, f_read, f1, f2, 1);
	int nummatches;
	for (int i = 512; i >= 16; i /= 2)
		nummatches = bdelta_pass(b, i);

	STACK_ALLOC(copyloc1, unsigned, nummatches + 1);
	STACK_ALLOC(copyloc2, unsigned, nummatches + 1);
	STACK_ALLOC(copynum, unsigned, nummatches + 1);

	FILE *fout = fopen(argv[3], "wb");
	if (!fout) {
		printf("couldn't open output file\n");
		exit(1);
	}

	const char *magic = "BDT";
	fwrite_fixed(fout, magic, 3);
	unsigned short version = 1;
	write_word(fout, version);
	unsigned char intsize = 4;
	fwrite_fixed(fout, &intsize, 1);
	write_dword(fout, size);
	write_dword(fout, size2);
	write_dword(fout, nummatches);

	unsigned lastp1 = 0,
		lastp2 = 0;
	for (int i = 0; i < nummatches; ++i) {
		unsigned p1, p2, num;
		bdelta_getMatch(b, i, &p1, &p2, &num);
		// printf("%*x, %*x, %*x, %*x\n", 10, p1, 10, p2, 10, num, 10, p2-lastp2);
		copyloc1[i] = p1 - lastp1;
		write_dword(fout, copyloc1[i]);
		copyloc2[i] = p2 - lastp2;
		write_dword(fout, copyloc2[i]);
		copynum[i] = num;
		write_dword(fout, copynum[i]);
		lastp1 = p1 + num;
		lastp2 = p2 + num;
	}
	if (size2 != lastp2) {
		copyloc1[nummatches] = 0; copynum[nummatches] = 0; 
		copyloc2[nummatches] = size2 - lastp2;
		++nummatches;
	}

// write_unsigned_list(adds, nummatches+1, fout);
// write_unsigned_list(copynum, nummatches, fout);
// write_signed_list(copyloc, nummatches, fout);

//  fwrite(copyloc1, 4, nummatches, fout);
//  fwrite(copyloc2, 4, nummatches, fout);
//  fwrite(copynum, 4, nummatches, fout);
	unsigned fp = 0;
	for (int i = 0; i < nummatches; ++i) {
		unsigned num = copyloc2[i];
		while (num > 0) {
			unsigned towrite = (num > 4096) ? 4096 : num;
			unsigned char buf[4096];
			f_read(f2, buf, fp, towrite);
			fwrite_fixed(fout, buf, towrite);
			num -= towrite;
			fp += towrite;
		}
		// fp+=copyloc2[i];
		if (i != nummatches) fp += copynum[i];
	}
 
	fclose(fout);

	bdelta_done_alg(b);

	fclose(f1);
	fclose(f2);
}
