/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include "file.h"
#include "compatibility.h"

typedef int64_t pos;

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
	try {
		if (argc != 4) {
			printf("usage: bpatch <oldfile> <newfile> <patchfile>\n");
			printf("needs a reference file, file to output, and patchfile:\n");
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
		unsigned short version = read_varint(patchfile);
		if (version != 1 && version != 2) {
			printf("unsupported patch version\n");
			return 1;
		}
		char intsize;
		fread_fixed(patchfile, &intsize, 1);
		if (intsize != 4) {
			printf("unsupported file pointer size\n");
			return 1;
		}
		pos size1 = read_varint(patchfile),
		    size2 = read_varint(patchfile);

		unsigned nummatches = read_varint(patchfile);

		pos * copyloc1 = new pos[nummatches + 1];
		pos * copyloc2 = new pos[nummatches + 1];
		pos *  copynum = new pos[nummatches + 1];

		for (unsigned i = 0; i < nummatches; ++i) {

		  copyloc1[i] = read_varint(patchfile);
		  copyloc2[i] = read_varint(patchfile);
		  copynum[i] = read_varint(patchfile);

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

			pos copyloc = copyloc1[i];
			fseek(ref, copyloc, SEEK_CUR);

			pos curofs=ftello(ref);

			if (!copy_bytes_to_file(ref, outfile, copynum[i])) {
				printf("Error while copying from reference file (ofs %ld, %u bytes)\n", curofs, copynum[i]);
				return -1;
			}
		}

		delete [] copynum;
		delete [] copyloc2;
		delete [] copyloc1;

	} catch (const char * desc){
		fprintf (stderr, "FATAL: %s\n", desc);
		return -1;
	}

	return 0;
}
