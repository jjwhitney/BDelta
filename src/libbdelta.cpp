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
#include "compatibility.h"

#if !defined(TOKEN_SIZE) || TOKEN_SIZE == 1
typedef uint8_t Token;
#elif TOKEN_SIZE == 2
typedef uint16_t Token;
#elif TOKEN_SIZE == 4
typedef uint32_t Token;
#endif

// Enables delta chunk statistics
// #define DO_STATS_DEBUG

#include <stdio.h>
#include <stdlib.h> 
#include "container.h"
#include "bdelta.h"
#include "checksum.h"
#include <limits>
#include <algorithm>
const bool verbose = false;
struct checksum_entry {
	Hash::Value cksum; //Rolling checksums
	unsigned loc;
	checksum_entry() {}
	checksum_entry(Hash::Value cksum, unsigned loc)
		{this->cksum = cksum; this->loc = loc;}
};

struct Range {
	unsigned p, num;
	Range() {}
	Range(unsigned p, unsigned num) {this->p = p; this->num = num;}
};

struct Match {
	unsigned p1, p2, num;
	Match(unsigned p1, unsigned p2, unsigned num) 
		{this->p1 = p1; this->p2 = p2; this->num = num;}
};

struct BDelta_Instance {
	bdelta_readCallback cb;
	void *handle1, *handle2;
	unsigned data1_size, data2_size;
	DList<Match> matches;
	DLink<Match> *accessplace;
	int access_int;
	int errorcode;

	Token *read1(void *buf, unsigned place, unsigned num)
		{return (Token*)cb(handle1, buf, place, num);}
	Token *read2(void *buf, unsigned place, unsigned num)
		{return (Token*)cb(handle2, buf, place, num);}
};

struct Checksums_Instance {
	unsigned blocksize;
	unsigned htablesize;
	checksum_entry **htable; // Points to first match in checksums
	checksum_entry *checksums;  // Sorted list of all checksums
	unsigned numchecksums;

	Checksums_Instance(int blocksize) {this->blocksize = blocksize;}
	void add(checksum_entry ck) {
		checksums[numchecksums] = ck;
		++numchecksums;
	}
	unsigned tableIndex(Hash::Value hashValue) {
		return Hash::modulo(hashValue, htablesize);
	}
};


unsigned match_buf_forward(void *buf1, void *buf2, unsigned num) { 
	unsigned i = 0;
	while (i < num && ((Token*)buf1)[i]==((Token*)buf2)[i]) ++i;
	return i;
}
unsigned match_buf_backward(void *buf1, void *buf2, unsigned num) { 
	int i = num;
	do --i;
	while (i >= 0 && ((Token*)buf1)[i] == ((Token*)buf2)[i]);
	return num - i - 1;
}
unsigned match_forward(BDelta_Instance *b, unsigned p1, unsigned p2) { 
	unsigned num = 0, match, numtoread;
	do {
		numtoread = std::min(b->data1_size - p1, b->data2_size - p2);
		if (numtoread > 4096) numtoread = 4096;
		Token buf1[4096], buf2[4096];
		Token *read1 = b->read1(buf1, p1, numtoread),
		      *read2 = b->read2(buf2, p2, numtoread);
		p1 += numtoread; p2 += numtoread;
		match = match_buf_forward(read1, read2, numtoread);
		num += match;
	} while (match && match == numtoread);
	return num;
}

unsigned match_backward(BDelta_Instance *b, unsigned p1, unsigned p2, unsigned blocksize) {
	unsigned num = 0, match, numtoread;
	do {
		numtoread = std::min(p1, p2);
		if (numtoread > blocksize) numtoread = blocksize;
		p1 -= numtoread; p2 -= numtoread;
		Token buf1[4096], buf2[4096];
		Token *read1 = b->read1(buf1, p1, numtoread),
		      *read2 = b->read2(buf2, p2, numtoread);
		match = match_buf_backward(read1, read2, numtoread);
		num += match;
	} while (match && match == numtoread);
	return num;
}


void addMatch(BDelta_Instance *b, unsigned p1, unsigned p2, unsigned num, DLink<Match> *&place) {
	while (place && place->obj->p2 >= p2) {
		DLink<Match> *toerase = place;
		place = place->prev;
		b->matches.erase(toerase);
	}
#ifndef ALLOW_OVERLAP
	if (place && place->obj->p2 + place->obj->num > p2)
		place->obj->num = p2 - place->obj->p2;
#endif
	DLink<Match> *next = place ? place->next : b->matches.first;
	// if (next && p2 >= next->obj->p2) {printf("Bad thing\n");}// goto outofhere;
#ifndef ALLOW_OVERLAP
	if (next && p2 + num > next->obj->p2)
		num = next->obj->p2 - p2;
#endif
	// printf("%i, %i, %i, %x, %x\n", p1, p2, num, place, next);
	place = b->matches.insert(new Match(p1, p2, num), place, next);
}

struct PotentialMatch {
	unsigned p1, p2;
	Hash::Value cksum;
	PotentialMatch() {}
	PotentialMatch(unsigned p1, unsigned p2, Hash::Value cksum)
		{this->p1 = p1; this->p2 = p2; this->cksum = cksum;}
};

template<class T>
T absoluteDifference(T a, T b) {
	return std::max(a, b) - std::min(a, b);
}

struct DistanceFromP1 {
	unsigned place;
	DistanceFromP1(unsigned place) {this->place = place;}
	bool operator() (PotentialMatch m1, PotentialMatch m2) {
		return absoluteDifference(place, m1.p1) < absoluteDifference(place, m2.p1);
	}
};

void sortTMatches(DLink<Match> *place, PotentialMatch *matches, unsigned numMatches) {
	unsigned lastf1Place = place ? place->obj->p1 + place->obj->num : 0;
	std::sort(matches, matches + numMatches, DistanceFromP1(lastf1Place));
}

#ifdef DO_STATS_DEBUG
long long stata = 0, statb = 0;
#endif
void findMatches(BDelta_Instance *b, Checksums_Instance *h, unsigned start, unsigned end,
		DLink<Match> *place) {
	const unsigned blocksize = h->blocksize;
	STACK_ALLOC(buf1, Token, blocksize);
	STACK_ALLOC(buf2, Token, blocksize);
	
	const unsigned maxPMatch = 256;
	PotentialMatch pMatch[maxPMatch];
	int pMatchCount = 0;
	unsigned processMatchesPos = end;
	Token *inbuf = b->read2(buf1, start, blocksize),
	      *outbuf;
	Hash hash = Hash(inbuf, blocksize);
	unsigned buf_loc = blocksize;
	unsigned j = start + blocksize;
	Hash::Value lastChecksum = ~hash.getValue();
	do {
		unsigned thisTableIndex = h->tableIndex(hash.getValue());
		checksum_entry *c = h->htable[thisTableIndex];
		if (c && hash.getValue() != lastChecksum) {
			do {
				if (c->cksum == hash.getValue()) {
					if (pMatchCount >= maxPMatch) {
						// Keep the best 16
						sortTMatches(place, pMatch, pMatchCount);
						pMatchCount = 16;
#ifdef DO_STATS_DEBUG
						++statb;
#endif
					}
					pMatch[pMatchCount++] = PotentialMatch(c->loc, j - blocksize, c->cksum);
					processMatchesPos = std::min(j + blocksize, processMatchesPos);
				}
				++c;
			} while (h->tableIndex(c->cksum) == thisTableIndex);
		}
		lastChecksum = hash.getValue();

		if (buf_loc == blocksize) {
			buf_loc = 0;
			std::swap(inbuf, outbuf);
			inbuf = b->read2(outbuf == buf1 ? buf2 : buf1, j, std::min(end - j, blocksize));
		}

		const Token
			oldToken = outbuf[buf_loc],
			newToken = inbuf[buf_loc];
		++buf_loc;
		hash.advance(oldToken, newToken);
		++j;



		if (j < processMatchesPos)
			continue;
		
		processMatchesPos = end;
		sortTMatches(place, pMatch, pMatchCount);
		for (int i = 0; i < pMatchCount; ++i) {
			unsigned p1 = pMatch[i].p1, p2 = pMatch[i].p2;
			unsigned fnum = match_forward(b, p1, p2);
			if (fnum >= blocksize) {
#ifdef THOROUGH
				for (unsigned betterP1 = p1 - 1; betterP1; --betterP1) {
					unsigned nfnum = match_forward(b, betterP1, p2);
					if (nfnum > fnum) {
						fnum = nfnum;
						p1 = betterP1;
					} else
						break;
				}
#endif
				unsigned bnum = match_backward(b, p1, p2, blocksize);
				unsigned num = fnum + bnum;

				p1 -= bnum; p2 -= bnum;
				addMatch(b, p1, p2, num, place);
				if (p2 + num > j) {
					// Fast foward over matched area.
					j = p2 + num;
					if (j < end) {
						inbuf = b->read2(buf1, j, std::min(end - j, blocksize));
						hash = Hash(inbuf, h->blocksize);
					}
					buf_loc = blocksize;
					j += blocksize;
				}
#ifdef DO_STATS_DEBUG
				++stata;
#endif
				break;
			}
		}
		pMatchCount = 0;
	} while (j < end);
}

bool comparep1(Range r1, Range r2) {
	return r1.p < r2.p;
}

struct Checksums_Compare {
	Checksums_Instance &ci;
	Checksums_Compare(Checksums_Instance &h) : ci(h) {}
	bool operator() (checksum_entry c1, checksum_entry c2) {
		return (ci.tableIndex(c1.cksum) < ci.tableIndex(c2.cksum));
	}
};

void *bdelta_init_alg(unsigned data1_size, unsigned data2_size, 
		bdelta_readCallback cb, void *handle1, void *handle2,
		unsigned tokenSize) {
	if (tokenSize != sizeof(Token)) {
		printf("Error: BDelta library compiled for token size of %lu.\n", sizeof(Token));
		return 0;
	}
	BDelta_Instance *b = new BDelta_Instance;
	if (!b) return 0;
	b->data1_size = data1_size;
	b->data2_size = data2_size;
	b->cb = cb;
	b->handle1 = handle1;
	b->handle2 = handle2;
	b->access_int = -1;
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
	b->access_int = -1;

	Range *unused = new Range[b->matches.size() + 1];
	if (!unused) {b->errorcode = BDELTA_MEM_ERROR; return 0;}
	unsigned numunused = 0;
	for (DLink<Match> *l = b->matches.first; l; l = l->next)
		unused[numunused++] = Range(l->obj->p1, l->obj->num);

	std::sort(unused, unused + numunused, comparep1);

	// Trick loop below into including the free range at the end.
	unused[numunused++] = Range(b->data1_size, b->data1_size);

	unsigned last = 0;
	unsigned missing = 0;
	for (unsigned i = 0; i < numunused; ++i) {
		unsigned nextstart = unused[i].p + unused[i].num;
		if (unused[i].p <= last)
			++missing;
		else
			unused[i - missing] = Range(last, unused[i].p - last);
		last = std::max(last, nextstart);
	}
	numunused -= missing;



	unsigned numblocks = 0;
	for (unsigned i = 0; i < numunused; ++i) {
		numblocks += unused[i].num / blocksize;
	}

	if (verbose) printf("Starting search for matching blocks of size %i\n", blocksize);
	// numblocks = size / blocksize;
	if (verbose) printf("found %i blocks\n", numblocks);
	h.htablesize = 1 << 16;
	while (h.htablesize < numblocks) h.htablesize <<= 1;
	// h.htablesize <<= 2;
	// htablesize >>= 0;
	if (verbose) printf("creating hash table of size %i\n", h.htablesize);
	// h.htablesize = 65536;
	h.htable = new checksum_entry*[h.htablesize];
	if (!h.htable) {b->errorcode = BDELTA_MEM_ERROR; return 0;}
	h.checksums = new checksum_entry[numblocks + 2];
	if (!h.checksums) {b->errorcode = BDELTA_MEM_ERROR; return 0;}

	if (verbose) printf("find checksums\n");

	h.numchecksums = 0;
	// unsigned numchecksums = 0;
	STACK_ALLOC(buf, Token, blocksize);
	for (unsigned i = 0; i < numunused; ++i) {
		unsigned first = unused[i].p, last = unused[i].p + unused[i].num;
		for (unsigned loc = first; loc + blocksize <= last; loc += blocksize) {
			Token *read = b->read1(buf, loc, blocksize);
			Hash::Value blocksum = Hash(read, h.blocksize).getValue();
			// Adjacent checksums are never repeated.
			if (! h.numchecksums || blocksum != h.checksums[h.numchecksums - 1].cksum)
				h.add(checksum_entry(blocksum, loc));
		}
	}
	if (h.numchecksums) {
		std::sort(h.checksums, h.checksums + h.numchecksums, Checksums_Compare(h));
#ifndef THOROUGH
		const unsigned maxIdenticalChecksums = 256;
		int j = 0;
		for (unsigned i = 0; i < h.numchecksums;) {
			if (i + maxIdenticalChecksums < h.numchecksums &&
					h.checksums[i].cksum == h.checksums[i + maxIdenticalChecksums].cksum) {
				i += maxIdenticalChecksums;
				Hash::Value cksum = h.checksums[i].cksum;
				while (i < h.numchecksums && h.checksums[i].cksum == cksum) ++i;
			} else {
				h.checksums[j++] = h.checksums[i++];
			}
		}
		h.numchecksums = j;
#endif
	}

	h.checksums[h.numchecksums].cksum = 0;
	h.checksums[h.numchecksums + 1].cksum = std::numeric_limits<Hash::Value>::max();

	for (unsigned i = 0; i < h.htablesize; ++i) h.htable[i] = 0;
	for (int i = h.numchecksums - 1; i >= 0; --i)
		h.htable[h.tableIndex(h.checksums[i].cksum)] = &h.checksums[i];

//  if (verbose) printf("%i checksums\n", h.numchecksums);
	if (verbose) printf("compare files\n");

	last = 0;
	for (DLink<Match> *l = b->matches.first; l; l = l->next) {
		if (l->obj->p2 - last >= blocksize)
			findMatches(b, &h, last, l->obj->p2, l->prev);
		last = l->obj->p2 + l->obj->num;
	}
	if (b->data2_size - last >= blocksize) 
		findMatches(b, &h, last, b->data2_size, b->matches.last);
	// printf("afterwards: %i, %i, %i\n", b->matches.first->next->obj->p1, b->matches.first->next->obj->p2, b->matches.first->next->obj->num);
	delete [] unused;
	delete [] h.htable;
	delete [] h.checksums;
#ifdef DO_STATS_DEBUG
	printf("a = %.lli; b = %.lli\n", stata, statb);
#endif
	// printf("Found %i matches\n", b->matches.size());
	return b->matches.size();
}


void bdelta_getMatch(void *instance, unsigned matchNum, 
		unsigned *p1, unsigned *p2, unsigned *num) {
	BDelta_Instance *b = (BDelta_Instance*)instance;
	int &access_int = b->access_int;
	DLink<Match> *&accessplace = b->accessplace;
	if (access_int == -1) {access_int = 0; accessplace = b->matches.first;}
	while ((unsigned)access_int < matchNum) {
		accessplace = accessplace->next;
		++access_int;
	}
	while ((unsigned)access_int > matchNum) {
		accessplace = accessplace->prev;
		--access_int;
	}
	*p1 = accessplace->obj->p1;
	*p2 = accessplace->obj->p2;
	*num = accessplace->obj->num;
}

int bdelta_getError(void *instance) {
	return ((BDelta_Instance*)instance)->errorcode;
}
