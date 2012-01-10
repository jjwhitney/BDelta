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
#include "bdelta.h"
#include "checksum.h"
#include <list>
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
	std::list<Match> matches;
	std::list<Match>::iterator accessplace;
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

// Iterator helper function
template <class T>
inline T prior(T i) {return --i;}

void addMatch(BDelta_Instance *b, unsigned p1, unsigned p2, unsigned num, std::list<Match>::iterator place) {
	while (place != b->matches.begin() && prior(place)->p2 >= p2)
		b->matches.erase(prior(place));
#ifndef ALLOW_OVERLAP
	if (place != b->matches.begin() && prior(place)->p2 + prior(place)->num > p2)
		prior(place)->num = p2 - prior(place)->p2;
	if (place != b->matches.end() && p2 + num > place->p2)
		num = place->p2 - p2;
#endif
	// printf("%i, %i, %i, %x, %x\n", p1, p2, num, place, next);
	b->matches.insert(place, Match(p1, p2, num));
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

void sortTMatches(BDelta_Instance *b, std::list<Match>::iterator place, std::list<PotentialMatch> &matches) {
	unsigned lastf1Place = place != b->matches.begin() ? prior(place)->p1 + prior(place)->num : 0;
	matches.sort(DistanceFromP1(lastf1Place));
}

#ifdef DO_STATS_DEBUG
long long stata = 0, statb = 0;
#endif
void findMatches(BDelta_Instance *b, Checksums_Instance *h, unsigned start, unsigned end,
		std::list<Match>::iterator place) {
	const unsigned blocksize = h->blocksize;
	STACK_ALLOC(buf1, Token, blocksize);
	STACK_ALLOC(buf2, Token, blocksize);

	const unsigned maxPMatch = 256;
	std::list<PotentialMatch> pMatch;
	unsigned processMatchesPos = end;
	Token *inbuf = b->read2(buf1, start, blocksize),
	      *outbuf;
	Hash hash = Hash(inbuf, blocksize);
	unsigned buf_loc = blocksize;
	Hash::Value lastChecksum = ~hash.getValue();
	for (unsigned j = start + blocksize; j <= end; ++j) {
		unsigned thisTableIndex = h->tableIndex(hash.getValue());
		checksum_entry *c = h->htable[thisTableIndex];
		if (c && hash.getValue() != lastChecksum) {
			do {
				if (c->cksum == hash.getValue()) {
					if (pMatch.size() >= maxPMatch) {
						// Keep the best 16
						sortTMatches(b, place, pMatch);
						pMatch.resize(16);
#ifdef DO_STATS_DEBUG
						++statb;
#endif
					}
					pMatch.push_back(PotentialMatch(c->loc, j - blocksize, c->cksum));
					processMatchesPos = std::min(j + blocksize / 2, processMatchesPos);
				}
				++c;
			} while (h->tableIndex(c->cksum) == thisTableIndex);
		}
		lastChecksum = hash.getValue();

		if (j >= processMatchesPos) {
			processMatchesPos = end;
			sortTMatches(b, place, pMatch);
			for (std::list<PotentialMatch>::iterator i = pMatch.begin(); i != pMatch.end(); ++i) {
				unsigned p1 = i->p1, p2 = i->p2;
				unsigned fnum = match_forward(b, p1, p2);
				if (fnum >= blocksize) {
	#ifdef THOROUGH
					for (unsigned betterP1 = p1 - (p1 ? 1 : 0); betterP1; --betterP1) {
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
#ifndef CARELESSMATCH
					if (num < blocksize * 2)
						break; // I'd like to continue here, but first need to reduce pMatchCount.
#endif

					p1 -= bnum; p2 -= bnum;
					addMatch(b, p1, p2, num, place);
					if (p2 + num > j) {
						// Fast foward over matched area.
						j = p2 + num - blocksize;
						inbuf = b->read2(buf1, j, blocksize);
						hash = Hash(inbuf, blocksize);
						buf_loc = blocksize;
						j += blocksize;
					}
	#ifdef DO_STATS_DEBUG
					++stata;
	#endif
					break;
				}
			}
			pMatch.clear();
		}

		if (buf_loc == blocksize) {
			buf_loc = 0;
			std::swap(inbuf, outbuf);
			inbuf = b->read2(outbuf == buf1 ? buf2 : buf1, j, std::min(end - j, blocksize));
		}

		hash.advance(outbuf[buf_loc], inbuf[buf_loc]);
		++buf_loc;
	}
}

bool comparep1(Range r1, Range r2) {
	return r1.p < r2.p;
}

struct Checksums_Compare {
	Checksums_Instance &ci;
	Checksums_Compare(Checksums_Instance &h) : ci(h) {}
	bool operator() (checksum_entry c1, checksum_entry c2) {
		unsigned ti1 = ci.tableIndex(c1.cksum), ti2 = ci.tableIndex(c2.cksum);
		if (ti1 == ti2)
			if (c1.cksum == c2.cksum)
				return c1.loc < c2.loc;
			else
				return c1.cksum < c2.cksum;
		else
			return ti1 < ti2;
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
	b->matches.clear();
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
	for (std::list<Match>::iterator l = b->matches.begin(); l != b->matches.end(); ++l)
		unused[numunused++] = Range(l->p1, l->num);

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
		unsigned writeLoc = 0, readLoc, testAhead;
		for (readLoc = 0; readLoc < h.numchecksums; readLoc = testAhead) {
			for (testAhead = readLoc; testAhead < h.numchecksums && h.checksums[readLoc].cksum == h.checksums[testAhead].cksum; ++testAhead)
				;
			if (testAhead - readLoc <= maxIdenticalChecksums)
				for (unsigned i = readLoc; i < testAhead; ++i)
					h.checksums[writeLoc++] = h.checksums[i];
		}
		h.numchecksums = writeLoc;
#endif
	}

	h.checksums[h.numchecksums].cksum = std::numeric_limits<Hash::Value>::max(); // If there's only one checksum, we might hit this and not know it,
	h.checksums[h.numchecksums].loc = 0; // So we'll just read from the beginning of the file to prevent crashes.
	h.checksums[h.numchecksums + 1].cksum = 0;

	for (unsigned i = 0; i < h.htablesize; ++i) h.htable[i] = 0;
	for (int i = h.numchecksums - 1; i >= 0; --i)
		h.htable[h.tableIndex(h.checksums[i].cksum)] = &h.checksums[i];

//  if (verbose) printf("%i checksums\n", h.numchecksums);
	if (verbose) printf("compare files\n");

	last = 0;
	for (std::list<Match>::iterator l = b->matches.begin(); l != b->matches.end(); ++l) {
		if (l->p2 - last >= blocksize)
			findMatches(b, &h, last, l->p2, l);
		last = l->p2 + l->num;
	}
	if (b->data2_size - last >= blocksize) 
		findMatches(b, &h, last, b->data2_size, b->matches.end());
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
	std::list<Match>::iterator &accessplace = b->accessplace;
	if (access_int == -1) {access_int = 0; accessplace = b->matches.begin();}
	while ((unsigned)access_int < matchNum) {
		++accessplace;
		++access_int;
	}
	while ((unsigned)access_int > matchNum) {
		--accessplace;
		--access_int;
	}
	*p1 = accessplace->p1;
	*p2 = accessplace->p2;
	*num = accessplace->num;
}

int bdelta_getError(void *instance) {
	return ((BDelta_Instance*)instance)->errorcode;
}
