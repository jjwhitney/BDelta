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
 
typedef unsigned char byte;

#include <stdio.h>
#include <stdlib.h> 
#include "container.h"
#include "bdelta.h"
#include "checksum.h"
#include <algorithm>

const bool verbose = false;
struct checksum_entry {
	Checksum cksum; //Rolling checksums
	unsigned loc;
};

struct Range {
	unsigned p, num;
	Range(unsigned p, unsigned num) {this->p=p; this->num=num;}
	Range() {}
};

struct Match {
	unsigned p1, p2, num;
	Match(unsigned p1, unsigned p2, unsigned num) 
		{this->p1=p1; this->p2=p2; this->num=num;}
};

struct BDelta_Instance {
	bdelta_readCallback f1, f2;
	unsigned f1_size, f2_size;
	DList<Match> matches;
	DLink<Match> *accessplace;
	int access_int;
	int errorcode;
};

struct Checksums_Instance {
	unsigned blocksize;
	ChecksumManager cman;
	unsigned htablesize;
	checksum_entry **htable; // Points to first match in checksums
	checksum_entry *checksums;  // Sorted list of all checksums
	unsigned numchecksums;

	Checksums_Instance(int blocksize) : cman(blocksize) {this->blocksize = blocksize;}
	void add(checksum_entry ck) {
		checksums[numchecksums] = ck;
		++numchecksums;
	}
        unsigned hashck(Checksum cksum) {
            return cman.modulo(cksum, htablesize);
	}
};


unsigned match_buf_forward(void *buf1, void *buf2, unsigned num) { 
	unsigned i = 0;
	while (i<num && (unsigned*)((char*)buf1+i)==(unsigned*)((char*)buf2+i)) i += sizeof(unsigned);
	if (i>=num)
		return num;
	while (i<num && ((byte*)buf1)[i]==((byte*)buf2)[i]) ++i;
	return i;
}
unsigned match_buf_backward(void *buf1, void *buf2, unsigned num) { 
	int i = num;
	do --i;
	while (i>=0 && ((byte*)buf1)[i]==((byte*)buf2)[i]);
	return num-i-1;
}
unsigned match_forward(BDelta_Instance *b, unsigned p1, unsigned p2) { 
	unsigned num = 0, match, numtoread;
	do {
		numtoread=std::min(b->f1_size-p1, b->f2_size-p2);
		if (numtoread>4096) numtoread=4096;
		void *read1 = b->f1(p1, numtoread);
		void *read2 = b->f2(p2, numtoread);
		p1+=numtoread; p2+=numtoread;
		match = match_buf_forward(read1, read2, numtoread);
		num+=match;
	} while (match && match==numtoread);
	return num;
}

unsigned match_backward(BDelta_Instance *b, unsigned p1, unsigned p2, unsigned blocksize) { 
	unsigned num = 0, match, numtoread;
	do {
		numtoread = std::min(p1, p2);
		if (numtoread > blocksize) numtoread = blocksize;
		p1-=numtoread; p2-=numtoread;  
		void *read1 = b->f1(p1, numtoread);
		void *read2 = b->f2(p2, numtoread);
		match = match_buf_backward(read1, read2, numtoread);
		num+=match;
	} while (match && match==numtoread);
	return num;
}


void addMatch(BDelta_Instance *b, unsigned p1, unsigned p2, unsigned num, DLink<Match> *&place) {
	while (place && place->obj->p2>=p2) {
		DLink<Match> *toerase = place;
		place=place->prev;
		b->matches.erase(toerase);
	}
/*
	if (place && place->obj->p2+place->obj->num>p2)
		place->obj->num=p2-place->obj->p2;
*/
	// Code below performs nearly the same as above, but isn't as likely to split unicode characters.
	if (place) {
		unsigned lastPlace = place->obj->p2 + place->obj->num;
		if (lastPlace > p2) {
			unsigned diff = lastPlace - p2;
			num -= diff;
			p1 += diff;
			p2 += diff;
		}
	}
	DLink<Match> *next = place?place->next:b->matches.first;
	// if (next && p2>=next->obj->p2) {printf("Bad thing\n"); }// goto outofhere;
	if (next && p2+num>next->obj->p2)
		num=next->obj->p2-p2;
	// printf("%i, %i, %i, %x, %x\n", p1, p2, num, place, next);
	place = b->matches.insert(new Match(p1, p2, num), place, next);
}

long long stata = 0, statb = 0;
void findMatches(BDelta_Instance *b, Checksums_Instance *h, unsigned start, unsigned end,
		DLink<Match> *place) {
	byte *inbuf, *outbuf;
	unsigned buf_loc;
	const unsigned blocksize = h->blocksize;
	
	const unsigned maxSectionMatches = 256;
	checksum_entry *checkMatches[maxSectionMatches];
	int matchP2[maxSectionMatches];
	int numcheckMatches;
	int j = start;
	while (j < end) {
		inbuf = (byte*)b->f2(j, blocksize);
		Checksum cksum = h->cman.evaluateBlock(inbuf);
		buf_loc=blocksize;
		j+=blocksize;

		numcheckMatches = 0;

		unsigned endi = end;
		for (; j < endi; ++j) {
			if (buf_loc==blocksize) {
				buf_loc=0;
				outbuf=inbuf;
				inbuf=(byte*)b->f2(j, blocksize);
			}
			checksum_entry *c = h->htable[h->hashck(cksum)];
			if (c) {
				while (h->hashck(c->cksum)==h->hashck(cksum)) {
					if (c->cksum==cksum) {
						if (numcheckMatches>=maxSectionMatches) {
							endi = j;
							numcheckMatches=0;
							break;
						}
						matchP2[numcheckMatches] = j-blocksize;
						checkMatches[numcheckMatches++] = c;
						if (endi==end) endi = j+blocksize;
						if (endi>end) endi=end;
					}
					++c;
				} //else ++statb;
			}

			const byte
				oldbyte = outbuf[buf_loc],
				newbyte = inbuf[buf_loc];
			++buf_loc;
			cksum = h->cman.advanceChecksum(cksum, oldbyte, newbyte);
		}

		if (numcheckMatches) {
			unsigned lastf1Place = place?place->obj->p1+place->obj->num:0;
			int closestMatch=0;
			for (int i = 1; i < numcheckMatches; ++i)
				if (abs(lastf1Place-checkMatches[i]->loc) <
						abs(lastf1Place-checkMatches[closestMatch]->loc))
					closestMatch=i;

			bool badMatch = (false && blocksize<=16 &&
					(checkMatches[closestMatch]->loc<lastf1Place ||
					 checkMatches[closestMatch]->loc>lastf1Place+blocksize));
			if (! badMatch) {
				unsigned p1 = checkMatches[closestMatch]->loc, p2 = matchP2[closestMatch];
				unsigned fnum = match_forward(b, p1, p2);
				// if (fnum<blocksize) falsematches++; else truematches++;

				if (fnum >= blocksize) {
					unsigned bnum = match_backward(b, p1, p2, blocksize);
					unsigned num=fnum+bnum;
					p1 -= bnum; p2 -= bnum;
					addMatch(b, p1, p2, num, place);
					j=p2+num;
					//printf("p1: %i, p2: %i, num: %i\n", p1, p2, num);
					++stata;
				} else ++statb;
			}
		}
	}
}

bool comparep1(Range r1, Range r2) {
	return r1.p < r2.p;
}

struct Checksums_Compare {
        Checksums_Instance &ci;
        Checksums_Compare(Checksums_Instance &h) : ci(h) {}
	unsigned hashPart(Checksum cksum) {return ci.hashck(cksum);}
	bool operator() (checksum_entry c1, checksum_entry c2) {
		return (hashPart(c1.cksum) < hashPart(c2.cksum));
	}
};

void *bdelta_init_alg(unsigned f1_size, unsigned f2_size, 
		bdelta_readCallback f1, bdelta_readCallback f2) {
	BDelta_Instance *b = new BDelta_Instance;
	if (!b) return 0;
	b->f1_size=f1_size;
	b->f2_size=f2_size;
	b->f1=f1;
	b->f2=f2;
	b->access_int=-1;
	return b;
}

void bdelta_done_alg(void *instance) {
	BDelta_Instance *b = (BDelta_Instance*)instance;
	while (!b->matches.empty()) {
		delete b->matches.first->obj;
		b->matches.erase(b->matches.first);
	}
	delete b;
}

unsigned bdelta_pass(void *instance, unsigned blocksize) {
	if (verbose) printf("Organizing leftover blocks\n");

	Checksums_Instance h(blocksize);
	BDelta_Instance *b = (BDelta_Instance*)instance;
	b->access_int=-1;

	Range *unused = new Range[b->matches.size() + 1];
	if (!unused) {b->errorcode=BDELTA_MEM_ERROR; return 0;}
	int numunused = 0;
	for (DLink<Match> *l = b->matches.first; l; l=l->next)
		unused[numunused++] = Range(l->obj->p1, l->obj->num);

        std::sort(unused, unused + numunused, comparep1);

	// Trick loop below into including the free range at the end.
	unused[numunused++] = Range(b->f1_size, b->f1_size);

	unsigned last = 0;
	unsigned missing = 0;
	for (unsigned i = 0; i < numunused; ++i) {
		unsigned nextstart = unused[i].p + unused[i].num;
		if (unused[i].p<=last) 
			++missing;
		else
			unused[i-missing] = Range(last, unused[i].p-last);
		last = std::max(last, nextstart);
	}
	numunused-=missing;



	unsigned numblocks = 0;
	for (unsigned i = 0; i < numunused; ++i) {
		numblocks+=unused[i].num/blocksize;
	}

	if (verbose) printf("Starting search for matching blocks of size %i\n", blocksize);
	// numblocks=size/blocksize;
	if (verbose) printf("found %i blocks\n", numblocks);
	h.htablesize = 1<<16;
	while (h.htablesize<numblocks) h.htablesize<<=1;
	// h.htablesize<<=2;
	// htablesize>>=0;
	if (verbose) printf("creating hash table of size %i\n", h.htablesize);
	// h.htablesize=65536;
	h.htable = new checksum_entry*[h.htablesize];
	if (!h.htable) {b->errorcode=BDELTA_MEM_ERROR; return 0;}
	h.checksums = new checksum_entry[numblocks+2];
	if (!h.checksums) {b->errorcode=BDELTA_MEM_ERROR; return 0;}

	if (verbose) printf("find checksums\n");

	h.numchecksums=0;
	// unsigned numchecksums = 0;
	for (unsigned i = 0; i < numunused; ++i) {
		unsigned first = unused[i].p, last = unused[i].p + unused[i].num;
		for (unsigned loc = first; loc + blocksize <= last; loc += blocksize) {
			byte *buf = (byte*)b->f1(loc, blocksize);
			h.add((checksum_entry){h.cman.evaluateBlock(buf), loc});
		}
	}
	if (h.numchecksums)
		std::sort(h.checksums, h.checksums + h.numchecksums, Checksums_Compare(h));

	h.checksums[h.numchecksums].cksum = 0;
	h.checksums[h.numchecksums+1].cksum = (Checksum)-1;

	for (unsigned i = 0; i < h.htablesize; ++i) h.htable[i]=0;
	for (int i = h.numchecksums-1; i >= 0; --i)
		h.htable[h.hashck(h.checksums[i].cksum)] = &h.checksums[i];

//  if (verbose) printf("%i checksums\n", h.numchecksums);
	if (verbose) printf("compare files\n");

	last = 0;
	for (DLink<Match> *l = b->matches.first; l; l=l->next) {
		if (l->obj->p2 - last >= blocksize)    
			findMatches(b, &h, last, l->obj->p2, l->prev);
		last = l->obj->p2+l->obj->num;
	}
	if (b->f2_size-last>=blocksize) 
		findMatches(b, &h, last, b->f2_size, b->matches.last);
	// printf("afterwards: %i, %i, %i\n", b->matches.first->next->obj->p1, b->matches.first->next->obj->p2, b->matches.first->next->obj->num);
	delete unused;
	delete h.htable;
	delete h.checksums;
	// printf("a = %.lli; b = %.lli\n", stata, statb);
	// printf("Found %i matches\n", b->matches.size());
	return b->matches.size();
}


void bdelta_getMatch(void *instance, unsigned matchNum, 
		unsigned *p1, unsigned *p2, unsigned *num) {
	BDelta_Instance *b = (BDelta_Instance*)instance;
	int &access_int = b->access_int;
	DLink<Match> *&accessplace = b->accessplace;
	if (access_int==-1) {access_int = 0; accessplace=b->matches.first;}
	while (access_int<matchNum) {
		accessplace=accessplace->next;
		++access_int;
	}
	while (access_int>matchNum) {
		accessplace=accessplace->prev;
		--access_int;
	}
	*p1 = accessplace->obj->p1;
	*p2 = accessplace->obj->p2;
	*num = accessplace->obj->num;
}

int bdelta_getError(void *instance) {
	return ((BDelta_Instance*)instance)->errorcode;
}
