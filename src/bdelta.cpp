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

void my_pass(BDelta_Instance *b, unsigned blocksize, unsigned minMatchSize, bool local) {
	bdelta_pass(b, blocksize, minMatchSize, local);
	bdelta_clean_matches(b, true);
}

int main(int argc, char **argv) {
	try {
		if (argc != 4) {
			printf("usage: bdelta <oldfile> <newfile> <patchfile>\n");
			printf("needs two files to compare + output file:\n");
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

		BDelta_Instance *b = bdelta_init_alg(size, size2, f_read, f1, f2, 1);
		int nummatches;

		// List of primes for reference. Taken from Wikipedia.
		//            1	  2	  3	  4	  5	  6	  7	  8	  9	 10	 11	 12	 13	 14	 15	 16	 17	 18	 19	 20
		// 1-20       2	  3	  5	  7	 11	 13	 17	 19	 23	 29	 31	 37	 41	 43	 47	 53	 59	 61	 67	 71
		// 21-40     73	 79	 83	 89	 97	101	103	107	109	113	127	131	137	139	149	151	157	163	167	173
		// 41-60    179	181	191	193	197	199	211	223	227	229	233	239	241	251	257	263	269	271	277	281
		// 61-80    283	293	307	311	313	317	331	337	347	349	353	359	367	373	379	383	389	397	401	409
		// 81-100   419	421	431	433	439	443	449	457	461	463	467	479	487	491	499	503	509	521	523	541
		// 101-120  547	557	563	569	571	577	587	593	599	601	607	613	617	619	631	641	643	647	653	659
		// 121-140  661	673	677	683	691	701	709	719	727	733	739	743	751	757	761	769	773	787	797	809
		// 141-160  811	821	823	827	829	839	853	857	859	863	877	881	883	887	907	911	919	929	937	941
		// 161-180  947	953	967	971	977	983	991	997

		int seq[] = {503, 127, 31, 7, 5, 3, -31, 31, 7, 5, 3, -7, 2};
		my_pass(b, 997, 1994, true);
		my_pass(b, 503, 1006, true);
		my_pass(b, 127, 254, true);
		my_pass(b,  31,  62, true);
		my_pass(b,   7,  14, true);
		my_pass(b,   5,  10, true);
		my_pass(b,   3,   6, true);
		my_pass(b,  13,  26, false);
		my_pass(b,   7,  14, true);
		my_pass(b,   5,  10, true);

		bdelta_clean_matches(b, true);

		nummatches = bdelta_nummatches(b);

		unsigned * copyloc1 = new unsigned[nummatches + 1];
		unsigned * copyloc2 = new unsigned[nummatches + 1];
		unsigned *  copynum = new unsigned[nummatches + 1];

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

		delete [] copynum;
		delete [] copyloc2;
		delete [] copyloc1;

	} catch (const char * desc){
		fprintf (stderr, "FATAL: %s\n", desc);
		return -1;
	}
	return 0;
}
