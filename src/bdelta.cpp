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

const int BUFNUM=16;
class Buffered_File {
	FILE *f;
	char *buf[BUFNUM];
	unsigned bufplace[BUFNUM];
	int bufsize;
public:
	Buffered_File(char *fname, unsigned maxread) {
		f = fopen(fname, "rb");
		bufsize=maxread;
		for (int i = 0; i < BUFNUM; ++i) {
			buf[i] = new char[bufsize];
			bufplace[i] = 0;
		}
	}
	~Buffered_File() {
		for (int i = 0; i < BUFNUM; ++i)
			delete buf[i];
		fclose(f);
	}
	void *read(unsigned place, unsigned num) {
		for (int i = 0; i < BUFNUM; ++i)
			if (bufplace[i] && place>bufplace[i] && place+num<bufplace[i]+bufsize) 
				return buf[i]+place-bufplace[i];
		char *lastbuf = buf[BUFNUM-1];
		for (int i = BUFNUM-1; i > 0; --i) {
			buf[i] = buf[i-1];
			bufplace[i] = bufplace[i-1];
		}
		buf[0]=lastbuf;
		bufplace[0]=place;
		// NB: if ftell(f)==place, this has no effect.
		fseek(f, place, SEEK_SET);
		fread(buf[0], 1, bufsize, f);
		return buf[0];
	}
};

Buffered_File *f1, *f2;

void *f1_read(unsigned place, unsigned num) {
	return f1->read(place, num);
}
void *f2_read(unsigned place, unsigned num) {
	return f2->read(place, num);
}

int main(int argc, char **argv) {
	if (argc!=4) {
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
	f1 = new Buffered_File(argv[1], 4096);
	f2 = new Buffered_File(argv[2], 4096);

	void *b = bdelta_init_alg(size, size2, f1_read, f2_read);
	int nummatches;
	for (int i = 512; i >= 16; i/=2)
		nummatches = bdelta_pass(b, i);

	unsigned copyloc1[nummatches+1];
	unsigned copyloc2[nummatches+1];
	unsigned copynum[nummatches+1];

	FILE *fout = fopen(argv[3], "wb");
	if (!fout) {
		printf("couldn't open output file\n");
		exit(1);
	}

	char *magic = "BDT";
	fwrite(magic, 1, 3, fout);
	unsigned short version = 1;
	write_word(fout, version);
	unsigned char intsize = 4;
	fwrite(&intsize, 1, 1, fout);
	write_dword(fout, size);
	write_dword(fout, size2);
	write_dword(fout, nummatches);

	unsigned lastp1 = 0,
		lastp2 = 0;
	for (int i = 0; i < nummatches; ++i) {
		unsigned p1, p2, num;
		bdelta_getMatch(b, i, &p1, &p2, &num);
		// printf("%*x, %*x, %*x, %*x\n", 10, p1, 10, p2, 10, num, 10, p2-lastp2);
		copyloc1[i] = p1-lastp1;
		write_dword(fout, copyloc1[i]);
		copyloc2[i] = p2-lastp2;
		write_dword(fout, copyloc2[i]);
		copynum[i] = num;
		write_dword(fout, copynum[i]);
		lastp1=p1+num;
		lastp2=p2+num;
	}
	if (size2!=lastp2) {
		copyloc1[nummatches]=0; copynum[nummatches]=0; 
		copyloc2[nummatches]=size2-lastp2;
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
		while (num>0) {
			unsigned towrite = num;
			if (towrite>4096) towrite=4096;
			void *buf = f2->read(fp, towrite);
			fwrite(buf, 1, towrite, fout);
			num-=towrite;
			fp+=towrite;
		}
		// fp+=copyloc2[i];
		if (i!=nummatches) fp+=copynum[i];
	}
 
	fclose(fout);

	bdelta_done_alg(b);

	delete f1;
	delete f2;
}
