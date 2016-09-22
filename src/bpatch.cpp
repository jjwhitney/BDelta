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
		FILE *ref = fopen(argv[1], "rb");
		char magic[3];
		fread_fixed(patchfile, magic, 3);
		if (strncmp(magic, "BDT", 3)) {
			printf("Given file is not a recognized patchfile\n");
			return 1;
		}
		unsigned short version = read_varuint(patchfile);
		if (version != 3) {
			printf("unsupported patch version\n");
			return 1;
		}
		pos size1 = read_varuint(patchfile),
		    size2 = read_varuint(patchfile);

		unsigned nummatches = read_varuint(patchfile);

		FILE *outfile = fopen(argv[2], "wb");

		for (unsigned i = 0; i < nummatches; ++i) {

		  pos nump = read_varuint(patchfile);
		  if (!copy_bytes_to_file(patchfile, outfile, nump)) {
		    printf("Error.  patchfile is truncated\n");
		    return -1;
		  }
		  fseeko(ref, read_varint(patchfile), SEEK_CUR);
		  pos numr = read_varuint(patchfile);

		  if (!copy_bytes_to_file(ref, outfile, numr)) {
		    printf("Error while copying from reference file (ofs %ld, %u bytes)\n", ftello(ref), numr);
		    return -1;
		  }
		  size2 -= nump + numr;
		}
		if (size2) {
		  pos nump = read_varuint(patchfile);
		  if (!copy_bytes_to_file(patchfile, outfile, nump)) {
		    printf("Error.  patchfile is truncated\n");
		    return -1;
		  }
		  ++nummatches;
		}

	} catch (const char * desc){
		fprintf (stderr, "FATAL: %s\n", desc);
		return -1;
	}

	return 0;
}
