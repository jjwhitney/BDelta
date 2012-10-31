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

#include <stdio.h>
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

struct _BDelta_Instance {
	bdelta_readCallback cb;
	void *handle1, *handle2;
	unsigned data1_size, data2_size;
	std::list<Match> matches;
	std::list<Match>::iterator accessplace;
	int access_int;
	int errorcode;

	const Token *read1(void *buf, unsigned place, unsigned num)
		{return (const Token*)cb(handle1, buf, place, num);}
	const Token *read2(void *buf, unsigned place, unsigned num)
		{return (const Token*)cb(handle2, buf, place, num);}
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


unsigned match_buf_forward(const void *buf1, const void *buf2, unsigned num) { 
	unsigned i = 0;
	while (i < num && ((const Token*)buf1)[i]==((const Token*)buf2)[i]) ++i;
	return i;
}
unsigned match_buf_backward(const void *buf1, const void *buf2, unsigned num) { 
	int i = num;
	do --i;
	while (i >= 0 && ((const Token*)buf1)[i] == ((const Token*)buf2)[i]);
	return num - i - 1;
}
unsigned match_forward(BDelta_Instance *b, unsigned p1, unsigned p2) { 
	unsigned num = 0, match, numtoread;
	do {
		numtoread = std::min(b->data1_size - p1, b->data2_size - p2);
		if (numtoread > 4096) numtoread = 4096;
		Token buf1[4096], buf2[4096];
		const Token *read1 = b->read1(buf1, p1, numtoread),
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
		const Token *read1 = b->read1(buf1, p1, numtoread),
		            *read2 = b->read2(buf2, p2, numtoread);
		match = match_buf_backward(read1, read2, numtoread);
		num += match;
	} while (match && match == numtoread);
	return num;
}

// Iterator helper function
template <class T>
inline T prior(T i) {return --i;}
template <class T>
inline T next(T i) {return ++i;}

void addMatch(BDelta_Instance *b, unsigned p1, unsigned p2, unsigned num, std::list<Match>::iterator place) {
	while (place != b->matches.begin() && prior(place)->p2 > p2)
		--place;
	while (place != b->matches.end() && place->p2 < p2)
		++place;
	b->matches.insert(place, Match(p1, p2, num));
}

template<class T>
T absoluteDifference(T a, T b) {
	return std::max(a, b) - std::min(a, b);
}

void findMatches(BDelta_Instance *b, Checksums_Instance *h, unsigned minMatchSize, unsigned start, unsigned end, unsigned place, std::list<Match>::iterator iterPlace) {
	const unsigned blocksize = h->blocksize;
	STACK_ALLOC(buf1, Token, blocksize);
	STACK_ALLOC(buf2, Token, blocksize);

	unsigned best1, best2, bestnum = 0;
	unsigned processMatchesPos;
	const Token *inbuf = b->read2(buf1, start, blocksize),
	            *outbuf;
	Hash hash = Hash(inbuf, blocksize);
	unsigned buf_loc = blocksize;
	for (unsigned j = start + blocksize; j <= end; ++j) {
		unsigned thisTableIndex = h->tableIndex(hash.getValue());
		checksum_entry *c = h->htable[thisTableIndex];
		if (c) {
			do {
				if (c->cksum == hash.getValue()) {
					unsigned p1 = c->loc, p2 = j - blocksize;
					unsigned fnum = match_forward(b, p1, p2);
					if (fnum >= blocksize) {
						unsigned bnum = match_backward(b, p1, p2, blocksize);
						unsigned num = fnum + bnum;
						if (num >= minMatchSize) {
							p1 -= bnum; p2 -= bnum;
							bool foundBetter;
							if (bestnum) {
								double oldValue = double(bestnum) / (absoluteDifference(place, best1) + blocksize * 2),
									   newValue = double(num) / (absoluteDifference(place, p1) + blocksize * 2);
								foundBetter = newValue > oldValue;
							} else {
								foundBetter = true;
								processMatchesPos = std::min(j + blocksize - 1, end);
							}
							if (foundBetter) {
								best1 = p1;
								best2 = p2;
								bestnum = num;
							}

						}
					}
				}
				++c;
			} while (h->tableIndex(c->cksum) == thisTableIndex);
		}

		if (bestnum && j >= processMatchesPos) {
			addMatch(b, best1, best2, bestnum, iterPlace);
			place = best1 + bestnum;
			unsigned matchEnd = best2 + bestnum;
			if (matchEnd > j) {
				if (matchEnd >= end)
					j = end;
				else {
					// Fast forward over matched area.
					j = matchEnd - blocksize;
					inbuf = b->read2(buf1, j, blocksize);
					hash = Hash(inbuf, blocksize);
					buf_loc = blocksize;
					j += blocksize;
				}
			}
			bestnum = 0;
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

BDelta_Instance *bdelta_init_alg(unsigned data1_size, unsigned data2_size,
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

void bdelta_done_alg(BDelta_Instance *b) {
	b->matches.clear();
	delete b;
}

struct UnusedRange {
	unsigned p, num;
	std::list<Match>::iterator ml, mr;
	UnusedRange() {}
	UnusedRange(unsigned p, unsigned num, std::list<Match>::iterator ml, std::list<Match>::iterator mr) {
		this->p = p; this->num = num; this->ml = ml; this->mr = mr;
	}
};


bool comparep(UnusedRange r1, UnusedRange r2) {
	return r1.p < r2.p;
}
bool comparemrp2(UnusedRange r1, UnusedRange r2) {
	return r1.mr->p2 < r2.mr->p2;
}

bool compareMatchP2(Match r1, Match r2) {
	return r1.p2 < r2.p2;
}

// Adapted from http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
unsigned roundUpPowerOf2(unsigned v) {
	--v;
	for (int i = 1; i <= 16; i *= 2)
		v |= v >> i;
	return v + 1;
}

void bdelta_pass_2(BDelta_Instance *b, unsigned blocksize, unsigned minMatchSize, UnusedRange *unused, unsigned numunused, UnusedRange *unused2, unsigned numunused2) {
	Checksums_Instance h(blocksize);
	b->access_int = -1;

	unsigned numblocks = 0;
	for (unsigned i = 0; i < numunused; ++i) {
		numblocks += unused[i].num / blocksize;
	}

	// numblocks = size / blocksize;
	h.htablesize = std::max((unsigned)2, roundUpPowerOf2(numblocks));
	h.htable = new checksum_entry*[h.htablesize];
	if (!h.htable) {b->errorcode = BDELTA_MEM_ERROR; return;}
	h.checksums = new checksum_entry[numblocks + 2];
	if (!h.checksums) {b->errorcode = BDELTA_MEM_ERROR; return;}

	h.numchecksums = 0;
	// unsigned numchecksums = 0;
	STACK_ALLOC(buf, Token, blocksize);
	for (unsigned i = 0; i < numunused; ++i) {
		unsigned first = unused[i].p, last = unused[i].p + unused[i].num;
		for (unsigned loc = first; loc + blocksize <= last; loc += blocksize) {
			const Token *read = b->read1(buf, loc, blocksize);
			Hash::Value blocksum = Hash(read, blocksize).getValue();
			// Adjacent checksums are never repeated.
			//if (! h.numchecksums || blocksum != h.checksums[h.numchecksums - 1].cksum)
				h.add(checksum_entry(blocksum, loc));
		}
	}

	if (h.numchecksums) {
		std::sort(h.checksums, h.checksums + h.numchecksums, Checksums_Compare(h));
		const unsigned maxIdenticalChecksums = 2;
		unsigned writeLoc = 0, readLoc, testAhead;
		for (readLoc = 0; readLoc < h.numchecksums; readLoc = testAhead) {
			for (testAhead = readLoc; testAhead < h.numchecksums && h.checksums[readLoc].cksum == h.checksums[testAhead].cksum; ++testAhead)
				;
			if (testAhead - readLoc <= maxIdenticalChecksums)
				for (unsigned i = readLoc; i < testAhead; ++i)
					h.checksums[writeLoc++] = h.checksums[i];
		}
		h.numchecksums = writeLoc;
	}
	h.checksums[h.numchecksums].cksum = std::numeric_limits<Hash::Value>::max(); // If there's only one checksum, we might hit this and not know it,
	h.checksums[h.numchecksums].loc = 0; // So we'll just read from the beginning of the file to prevent crashes.
	h.checksums[h.numchecksums + 1].cksum = 0;

	for (unsigned i = 0; i < h.htablesize; ++i) h.htable[i] = 0;
	for (int i = h.numchecksums - 1; i >= 0; --i)
		h.htable[h.tableIndex(h.checksums[i].cksum)] = &h.checksums[i];

	for (unsigned i = 0; i < numunused2; ++i)
		if (unused2[i].num >= blocksize)
			findMatches(b, &h, minMatchSize, unused2[i].p, unused2[i].p + unused2[i].num, unused[i].p, unused2[i].mr);

	delete [] h.htable;
	delete [] h.checksums;
}

void bdelta_swap_inputs(BDelta_Instance *b) {
	for (std::list<Match>::iterator l = b->matches.begin(); l != b->matches.end(); ++l)
		std::swap(l->p1, l->p2);
	std::swap(b->data1_size, b->data2_size);
	std::swap(b->handle1, b->handle2);
	b->matches.sort(compareMatchP2);
}

void bdelta_clean_matches(BDelta_Instance *b, unsigned flags) {
	// TODO: delete worse match when there's a conflict.
	std::list<Match>::iterator place = b->matches.begin();
	while (true) {
		while (place != b->matches.begin() && place != b->matches.end() && prior(place)->p2 + prior(place)->num >= place->p2 + place->num)
			place = b->matches.erase(place);

		if (place == b->matches.end())
			break;

		if (flags & BDELTA_REMOVE_OVERLAP)
			if (place != b->matches.begin() && prior(place)->p2 + prior(place)->num > place->p2) {
				prior(place)->num = place->p2 - prior(place)->p2;
				if (! prior(place)->num)
					b->matches.erase(prior(place));
			}
		++place;
	}
}

void bdelta_showMatches(BDelta_Instance *b) {
	for (std::list<Match>::iterator l = b->matches.begin(); l != b->matches.end(); ++l)
		printf("(%d, %d, %d), ", l->p1, l->p2, l->num);
	printf ("\n\n");
}

void get_unused_blocks(UnusedRange *unused, unsigned *numunusedptr) {
	unsigned &numunused = *numunusedptr;

	unsigned last = 0;
	std::list<Match>::iterator lastnext = unused[0].ml;
	for (unsigned i = 0; i < numunused; ++i) {
		unsigned nextstart = unused[i].p + unused[i].num;

		std::list<Match>::iterator mr = unused[i].ml;
		unused[i] = UnusedRange(last, unused[i].p < last ? 0 : unused[i].p - last, lastnext, mr);
		lastnext = next(mr);

		last = std::max(last, nextstart);
	}
}

void bdelta_pass(BDelta_Instance *b, unsigned blocksize, unsigned minMatchSize, unsigned maxHoleSize, unsigned flags) {
	// Trick for including the free range at the end.
	b->matches.push_back(Match(b->data1_size, b->data2_size, 0));

	UnusedRange *unused = new UnusedRange[b->matches.size() + 1],
			    *unused2 = new UnusedRange[b->matches.size() + 1];
	unsigned numunused = 0, numunused2 = 0;
	for (std::list<Match>::iterator l = b->matches.begin(); l != b->matches.end(); ++l) {
		unused[numunused++] = UnusedRange(l->p1, l->num, l, l);
		unused2[numunused2++] = UnusedRange(l->p2, l->num, l, l);
	}

	std::sort(unused, unused + numunused, comparep);
	//std::sort(unused2, unused2 + numunused2, comparep);

	get_unused_blocks(unused, &numunused);

	get_unused_blocks(unused2, &numunused2);
	//std::sort(unused2, unused2 + numunused2, comparemrp2);

	if (flags & BDELTA_GLOBAL)
		bdelta_pass_2(b, blocksize, minMatchSize, unused, numunused, unused2, numunused2);
	else {
		std::sort(unused, unused + numunused, comparemrp2);
		for (unsigned i = 0; i < numunused; ++i) {
			UnusedRange u1 = unused[i], u2 = unused2[i];
			if (u1.num >= blocksize && u2.num >= blocksize)
				if (! maxHoleSize || (u1.num <= maxHoleSize && u2.num <= maxHoleSize))
					if (! (flags & BDELTA_SIDES_ORDERED) || (next(u1.ml) == u1.mr))
						bdelta_pass_2(b, blocksize, minMatchSize, &u1, 1, &u2, 1);
		}
	}

	if (verbose) printf("pass (blocksize: %d, matches: %zu)\n", blocksize, b->matches.size());

	// Get rid of the dummy value we placed at the end.
	b->matches.pop_back();

	delete [] unused;
	delete [] unused2;
}

unsigned bdelta_numMatches(BDelta_Instance *b) {
	return b->matches.size();
}

void bdelta_getMatch(BDelta_Instance *b, unsigned matchNum,
		unsigned *p1, unsigned *p2, unsigned *num) {
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

int bdelta_getError(BDelta_Instance *instance) {
	return instance->errorcode;
}
