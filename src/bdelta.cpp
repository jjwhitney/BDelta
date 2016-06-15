/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bdelta.h"
#include "file.h"
#include "compatibility.h"

const void *f_read(void *f, void *buf, pos place, pos num) {
	fseek((FILE *)f, place, SEEK_SET);
	fread_fixed((FILE *)f, buf, num);
	return buf;
}

const void *m_read(void *f, void * buf, pos place, pos num) {
	if (0) {
		/*
		 * BDelta uses only returned pointer
		 * and does not modify it's contents.
		 *
		 * But bugs happen.
		 */
		memcpy (buf, (char*)f + place, num);
		return buf;
	}
	return (const char*)f + place;
}

void my_pass(BDelta_Instance *b, unsigned blocksize, unsigned minMatchSize, unsigned flags) {
	bdelta_pass(b, blocksize, minMatchSize, 0L, flags);
	bdelta_clean_matches(b, BDELTA_REMOVE_OVERLAP);
}

int main(int argc, char **argv) {
	try {
		bool all_ram_mode = false;
		char * m1 = NULL;
		char * m2 = NULL;

		if (argc > 1 && strcmp(argv[1], "--all-in-ram") == 0)
		{
			all_ram_mode = true;
			--argc;
			++argv;
		}
		if (argc != 4) {
			printf("usage: bdelta [--all-in-ram] <oldfile> <newfile> <patchfile>\n");
			printf("needs two files to compare + output file:\n");
			exit(1);
		}
		if (!fileExists(argv[1]) || !fileExists(argv[2])) {
			printf("one of the input files does not exist\n");
			exit(1);
		}
		pos size = getLenOfFile(argv[1]);
		pos size2 = getLenOfFile(argv[2]);
		FILE *f1 = fopen(argv[1], "rb"),
		     *f2 = fopen(argv[2], "rb");
		
		BDelta_Instance *b;

		if (all_ram_mode)
		{
			m1 = new char[size];
			m2 = new char[size2];
			fread_fixed(f1, m1, size);
			fread_fixed(f2, m2, size2);

			b = bdelta_init_alg(size, size2, m_read, m1, m2);
		}
		else
		{
			b = bdelta_init_alg(size, size2, f_read, f1, f2);
		}
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

		my_pass(b, 997, 1994, 0);
		my_pass(b, 503, 1006, 0);
		my_pass(b, 127, 254, 0);
		my_pass(b,  31,  62, 0);
		my_pass(b,   7,  14, 0);
		my_pass(b,   5,  10, 0);
		my_pass(b,   3,   6, 0);
		my_pass(b,  13,  26, BDELTA_GLOBAL);
		my_pass(b,   7,  14, 0);
		my_pass(b,   5,  10, 0);

		nummatches = bdelta_numMatches(b);

		pos * copyloc1 = new pos[nummatches + 1];
		pos * copyloc2 = new pos[nummatches + 1];
		pos *  copynum = new pos[nummatches + 1];

		FILE *fout = fopen(argv[3], "wb");
		if (!fout) {
			printf("couldn't open output file\n");
			exit(1);
		}

		const char *magic = "BDT";
		fwrite_fixed(fout, magic, 3);
		unsigned short version = 2;
		write_varint(fout, version);
		unsigned char intsize = 4;
		fwrite_fixed(fout, &intsize, 1);
		write_varint(fout, size);
		write_varint(fout, size2);
		write_varint(fout, nummatches);

		pos lastp1 = 0,
		  lastp2 = 0;
		for (int i = 0; i < nummatches; ++i) {
			pos p1, p2, num;
			bdelta_getMatch(b, i, &p1, &p2, &num);
			// printf("%*x, %*x, %*x, %*x\n", 10, p1, 10, p2, 10, num, 10, p2-lastp2);
			copyloc1[i] = p1 - lastp1;
			copyloc2[i] = p2 - lastp2;
			copynum[i] = num;
			write_varint(fout, copyloc1[i]);
			write_varint(fout, copyloc2[i]);
			write_varint(fout, copynum[i]);

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
		pos fp = 0;
		for (int i = 0; i < nummatches; ++i) {
			pos num = copyloc2[i];
			while (num > 0) {
				unsigned towrite = (num > 4096) ? 4096 : num;
				unsigned char buf[4096];
				const void * b;
				if (all_ram_mode)
					b = m_read(m2, buf, fp, towrite);
				else
					b = f_read(f2, buf, fp, towrite);
				fwrite_fixed(fout, b, towrite);
				num -= towrite;
				fp += towrite;
			}
			// fp+=copyloc2[i];
			if (i != nummatches) fp += copynum[i];
		}
 
		fclose(fout);

		bdelta_done_alg(b);

		delete [] m1;
		delete [] m2;

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
