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
#include "container.h"
#include "bdelta.h"
const bool verbose = false;
typedef unsigned char byte;
typedef unsigned long long Checksum;
struct checksum_entry {
	Checksum cksum; //Rolling checksums
	unsigned loc;
	int next;
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
	unsigned hashsize;
	int *hash;
	checksum_entry *hash_items;
	unsigned numhashitems;
};

const unsigned multiplyamount = 181;

unsigned match_buf_forward(void *buf1, void *buf2, unsigned num) { 
	unsigned  i = 0;
	while (i<num && (unsigned*)((char*)buf1+i)==(unsigned*)((char*)buf2+i)) i+=4;
	while (i<num && ((byte*)buf1)[i]==((byte*)buf2)[i]) ++i;
	return i;
}
unsigned match_buf_backward(void *buf1, void *buf2, unsigned num) { 
	int i = num;
	do --i;
	while (i>=0 && ((byte*)buf1)[i]==((byte*)buf2)[i]);
	return num-i-1;
}
inline unsigned lesser(unsigned a, unsigned b) {return a<b?a:b;}
unsigned match_forward(BDelta_Instance *b, unsigned p1, unsigned p2) { 
	unsigned num = 0, match, numtoread;
	do {
		numtoread=lesser(b->f1_size-p1, b->f2_size-p2);
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
		numtoread = lesser(p1, p2);
		if (numtoread > blocksize) numtoread = blocksize;
		p1-=numtoread; p2-=numtoread;  
		void *read1 = b->f1(p1, numtoread);
		void *read2 = b->f2(p2, numtoread);
		match = match_buf_backward(read1, read2, numtoread);
		num+=match;
	} while (match && match==numtoread);
	return num;
}


void calculate_block_checksum(byte *blockptr, unsigned blocksize, 
                              unsigned &sum, Checksum &accum) {
	sum = 0; accum = 0;
	// Checksum rsum=0;
	for (unsigned buf_loc = 0; buf_loc < blocksize; ++buf_loc) {
		sum += blockptr[buf_loc];
		accum *= multiplyamount;
		accum += sum;
		// rsum = (rsum<<shiftSize)|(rsum>>(32-shiftSize));
		// rsum^=blockptr[buf_loc];
	}
}

void addMatch(BDelta_Instance *b, unsigned p1, unsigned p2, unsigned num, DLink<Match> *&place) {
	while (place && place->obj->p2>=p2) {
		DLink<Match> *toerase = place;
		place=place->prev;
		b->matches.erase(toerase);
	}
	if (place && place->obj->p2+place->obj->num>p2)
		place->obj->num=p2-place->obj->p2;
	DLink<Match> *next = place?place->next:b->matches.first;
	// if (next && p2>=next->obj->p2) {printf("Bad thing\n"); }// goto outofhere;
	if (next && p2+num>next->obj->p2)
		num=next->obj->p2-p2;
	// printf("%i, %i, %i, %x, %x\n", p1, p2, num, place, next);
	place = b->matches.insert(new Match(p1, p2, num), place, next);
}

//long long stata = 0, statb = 0;
void findMatches(BDelta_Instance *b, Checksums_Instance *h, unsigned start, unsigned end,
		DLink<Match> *place, Checksum oldcoefficient) {
	byte *inbuf, *outbuf;
	unsigned buf_loc;
	const unsigned blocksize = h->blocksize;
  
	unsigned sum;
	Checksum accum;
  
	const unsigned maxSectionMatches = 128;//16+b->f2_size/262140;
	int checkMatches[maxSectionMatches];
	int matchP2[maxSectionMatches];
	int numcheckMatches;
	int challengerP1, challengerP2 = end, challengerNum=0;
	int j = start;
	while (j < end) {
		inbuf = (byte*)b->f2(j, blocksize);
		calculate_block_checksum(inbuf, blocksize, sum, accum);
		buf_loc=blocksize;
		j+=blocksize;
    
		numcheckMatches = 0;
    
		unsigned endi = end;
		int i;
		for (i = j; i < endi; ++i) {
			if (buf_loc==blocksize) {
				buf_loc=0;
				outbuf=inbuf;
				inbuf=(byte*)b->f2(i, blocksize);
			}
			const Checksum ck = accum;
			int c = h->hash[ck&(h->hashsize-1)];
			if (c!=-1) {
				// ++stata;
				// if (c!=-1) {
				int start = c;
				do {
					c=h->hash_items[c].next;
					if (h->hash_items[c].cksum==ck) {
						// printf("%i\n", numcheckMatches);
						if (numcheckMatches>=maxSectionMatches) {
							i = endi;
							numcheckMatches=0;//printf("too many matches\n");
							break;
						}
						matchP2[numcheckMatches] = i-blocksize;
						checkMatches[numcheckMatches++] = c;
						if (endi==end) endi = i+blocksize;
						if (endi>end) endi=end;
					}
				} while (c!=start);
			} //else ++statb;
			const byte 
				oldbyte = outbuf[buf_loc],
				newbyte = inbuf[buf_loc];
			++buf_loc;
			accum -= oldcoefficient*oldbyte;
			accum*=multiplyamount;
			sum = sum - oldbyte + newbyte;
			accum += sum;

			// static int lastmark = 0;
			// if (start==0 && end==size2 && i>lastmark*(size2/20)) {
			//	fprintf(stderr, "checkpoint %i\n", lastmark);
			//	lastmark++;
			// }
		}
    
		j=i;
		// again:
		if (numcheckMatches) {
			unsigned lastf1Place = place?place->obj->p1+place->obj->num:0;
			int closestMatch=0;
			for (int i = 1; i < numcheckMatches; ++i)
				if (abs(lastf1Place-h->hash_items[checkMatches[i]].loc) <
						abs(lastf1Place-h->hash_items[checkMatches[closestMatch]].loc))
					closestMatch=i;

			unsigned p1 = h->hash_items[checkMatches[closestMatch]].loc, p2 = matchP2[closestMatch];
			unsigned fnum = match_forward(b, p1, p2);
			// if (fnum<blocksize) falsematches++; else truematches++;

			if (fnum >= blocksize) {
				unsigned bnum = match_backward(b, p1, p2, blocksize);
				unsigned num=fnum+bnum;
				p1 -= bnum; p2 -= bnum;
				addMatch(b, p1, p2, num, place);
				j=p2+num;
			} 
		}
	}
}

// TODO: maybe make this function a member of Checksums_Instance?
void add_cksum(BDelta_Instance *b, Checksums_Instance *h, unsigned place) {
	const unsigned blocksize = h->blocksize;
	byte *blockbuf = (byte*)b->f1(place, blocksize);
	unsigned sum;
	Checksum accum;
	calculate_block_checksum(blockbuf, blocksize, sum, accum);
	Checksum ck = accum;
	h->hash_items[h->numhashitems].cksum = ck;
	h->hash_items[h->numhashitems].loc = place;
	if (h->hash[ck&(h->hashsize-1)] != -1
		// && (hash[ck&(hashsize-1)]->cksum1!=c->cksum1
		// || hash[ck&(hashsize-1)]->cksum2!=c->cksum2)
	) {
		h->hash_items[h->numhashitems].next = 
		h->hash_items[h->hash[ck&(h->hashsize-1)]].next;
		h->hash_items[h->hash[ck&(h->hashsize-1)]].next = h->numhashitems;
	} else
		h->hash_items[h->numhashitems].next = h->numhashitems;
	h->hash[ck&(h->hashsize-1)] = h->numhashitems;
	// if (i < 10000000) printf("%*llx, %*x\n", 18, ck, 10, i); else exit(1);
	++h->numhashitems;
} 

int comparep1(const void *r1, const void *r2) {
	if (((Range*)r1)->p < ((Range*)r2)->p) return -1;
	return 1;
}

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

	Checksums_Instance h;
	h.blocksize = blocksize;
	BDelta_Instance *b = (BDelta_Instance*)instance;
	b->access_int=-1;

	Range *unused = new Range[b->matches.size() + 1];
	if (!unused) {b->errorcode=BDELTA_MEM_ERROR; return 0;}
	int numunused = 0;
	for (DLink<Match> *l = b->matches.first; l; l=l->next)
		unused[numunused++] = Range(l->obj->p1, l->obj->num);

	qsort(unused, numunused, sizeof(Range), comparep1);
/*
	for (int i = 0; i < numunused; ++i) 
		for (int j = i+1; j < numunused; ++j)
			if (unused[i].p > unused[j].p) {
				Range temp = unused[i];
				unused[i] = unused[j];
				unused[j] = temp;
			}
*/

	unsigned last = 0;
	unsigned missing = 0;
	for (unsigned i = 0; i < numunused; ++i) {
		unsigned nextstart = unused[i].p + unused[i].num;
		if (unused[i].p<=last) 
			++missing;
		else
			unused[i-missing] = Range(last, unused[i].p-last);
		last = nextstart;
	}
	numunused-=missing;
	unused[numunused++] = Range(last, b->f1_size-last);



	unsigned numblocks = 0;
	for (unsigned i = 0; i < numunused; ++i) {
		numblocks+=unused[i].num/blocksize;
	}

	if (verbose) printf("Starting search for matching blocks of size %i\n", blocksize);
	// numblocks=size/blocksize;
	if (verbose) printf("found %i blocks\n", numblocks);
	h.hashsize = 1<<16;
	while (h.hashsize<numblocks) h.hashsize<<=1;
	// h.hashsize<<=2;
	// hashsize>>=0;
	if (verbose) printf("creating hash of size %i\n", h.hashsize);
	h.hash = new int[h.hashsize];
	if (!h.hash) {b->errorcode=BDELTA_MEM_ERROR; return 0;}
	h.hash_items = new checksum_entry[numblocks];
	if (!h.hash_items) {b->errorcode=BDELTA_MEM_ERROR; return 0;}

	if (verbose) printf("find checksums\n");
	for (unsigned i = 0; i < h.hashsize; ++i) h.hash[i]=-1;

	h.numhashitems=0;
	// unsigned numchecksums = 0;
	for (unsigned i = 0; i < numunused; ++i) {
		unsigned p1 = unused[i].p, p2 = unused[i].p + unused[i].num;
		while (p1+blocksize <= p2) {
			// ++numchecksums;
			add_cksum(b, &h, p1);
			p1+=blocksize;
		}
	}
	// if (verbose) printf("%i checksums\n", h.numhashitems);
	if (verbose) printf("compare files\n");
  
	Checksum oldcoefficient = 1;
	for (unsigned i = 1; i < blocksize; ++i) {
		oldcoefficient*=multiplyamount;
		++oldcoefficient;
	}

	last = 0;
	for (DLink<Match> *l = b->matches.first; l; l=l->next) {
		if (l->obj->p2 - last >= blocksize)    
			findMatches(b, &h, last, l->obj->p2, l->prev, oldcoefficient);
		last = l->obj->p2+l->obj->num;
	}
	if (b->f2_size-last>=blocksize) 
		findMatches(b, &h, last, b->f2_size, b->matches.last, oldcoefficient);
	delete unused;
	delete h.hash;
	delete h.hash_items;
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
