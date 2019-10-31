/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <cstring>
#include <inttypes.h>
#include <limits>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <utility>

#if !defined(TOKEN_SIZE) || TOKEN_SIZE == 1
typedef uint8_t Token;
#elif TOKEN_SIZE == 2
typedef uint16_t Token;
#elif TOKEN_SIZE == 4
typedef uint32_t Token;
#endif

#include "bdelta.h"
#include "checksum.h"
#include "noexcept.h"

const bool verbose = false;
struct checksum_entry 
{
    Hash::Value cksum; //Rolling checksums
    unsigned loc;
    checksum_entry(Hash::Value _cksum, unsigned _loc) noexcept : cksum(_cksum), loc(_loc) {}
};

struct Range 
{
    unsigned p, num;
    Range(unsigned _p, unsigned _num) noexcept : p(_p), num(_num) {}
};

struct Match 
{
    unsigned p1, p2, num;
    Match(unsigned _p1, unsigned _p2, unsigned _num) noexcept : p1(_p1), p2(_p2), num(_num) {}
};

typedef NoThrowList<Match> MatchList;
typedef typename MatchList::iterator MatchListIterator;

static const size_t TOKEN_BUFFER_SIZE = 4096;
static constexpr size_t BUFFER_DEFAULT_SIZE = 16 * 1024;

struct UnusedRange
{
    unsigned p, num;
    MatchListIterator ml, mr;
    UnusedRange() noexcept : p(0), num(0) {}
    UnusedRange(unsigned _p, unsigned _num, const MatchListIterator& _ml, const MatchListIterator& _mr) noexcept
        : p(_p), num(_num), ml(_ml), mr(_mr)
    {}
    void set(unsigned _p, unsigned _num, const MatchListIterator& _ml, const MatchListIterator& _mr)
    {
        p = _p;
        num = _num;
        ml = _ml;
        mr = _mr;
    }
};

struct _BDelta_Instance 
{
    const bdelta_readCallback cb;
    void *handle1, *handle2;
    unsigned data1_size, data2_size;
    MatchList matches;
    MatchListIterator accessplace;
    int access_int;
    BDELTA_RESULT errorcode;

    NoThrowMemoryStorage<Token> match_buffer;
    NoThrowMemoryStorage<Token> find_matches_buffer;
    NoThrowMemoryStorage<checksum_entry*> bdelta_pass_2_htable;
    NoThrowMemoryStorage<checksum_entry> bdelta_pass_2_hchecksums;
    NoThrowMemoryStorage<Token>          bdelta_pass_2_buf;
    NoThrowMemoryStorage<UnusedRange, Construct>    bdelta_pass_unused;

    _BDelta_Instance(bdelta_readCallback _cb, void * _handle1, void * _handle2,
                     unsigned _data1_size, unsigned _data2_size) noexcept
        : cb(_cb), handle1(_handle1), handle2(_handle2)
        , data1_size(_data1_size), data2_size(_data2_size)
        , access_int(-1), errorcode(BDELTA_OK), match_buffer(2 * TOKEN_BUFFER_SIZE)
        , find_matches_buffer(2 * BUFFER_DEFAULT_SIZE), bdelta_pass_2_htable(2 * BUFFER_DEFAULT_SIZE)
        , bdelta_pass_2_hchecksums(2 * BUFFER_DEFAULT_SIZE), bdelta_pass_2_buf(2 * BUFFER_DEFAULT_SIZE)
        , bdelta_pass_unused(2 * BUFFER_DEFAULT_SIZE)
    {
        if (!(match_buffer && find_matches_buffer && bdelta_pass_2_htable
            && bdelta_pass_2_hchecksums && bdelta_pass_2_buf && bdelta_pass_unused))
            errorcode = BDELTA_MEM_ERROR;
    }

    const Token *read1(void *buf, unsigned place, unsigned num) noexcept
    {
        return (cb == nullptr)? ((const Token*)((char *)handle1 + place)) : ((const Token*)cb(handle1, buf, place, num, this, this->errorcode));
    }
    const Token *read2(void *buf, unsigned place, unsigned num) noexcept
    {
        return (cb == nullptr)? ((const Token*)((char *)handle2 + place)) : ((const Token*)cb(handle2, buf, place, num, this, this->errorcode));
    }
};

struct Checksums_Instance 
{
    const unsigned blocksize;
    const unsigned htablesize;
    checksum_entry ** const htable;    // Points to first match in checksums
    checksum_entry * const checksums;  // Sorted list of all checksums
    unsigned numchecksums;

    Checksums_Instance(unsigned _blocksize, unsigned _htablesize, checksum_entry ** _htable, checksum_entry * _checksums) noexcept
        : blocksize(_blocksize), htablesize(_htablesize), htable(_htable)
        , checksums(_checksums), numchecksums(0)
    {
        memset(htable, 0, htablesize * sizeof(htable[0]));
    }
    
    template <class T>
    void add(T&& ck) noexcept
    {
        checksums[numchecksums++] = std::forward<T>(ck);
    }
    unsigned tableIndex(Hash::Value hashValue) noexcept
    {
        return Hash::modulo(hashValue, htablesize);
    }
};

static inline unsigned match_buf_forward(const void *buf1, const void *buf2, unsigned num) noexcept
{ 
    unsigned i = 0;
    while (i < num && ((const Token*)buf1)[i] == ((const Token*)buf2)[i])
        ++i;
    return i;
}

static inline unsigned match_buf_backward(const void *buf1, const void *buf2, unsigned num) noexcept
{ 
    int i = num;
    do
    {
        --i;
    }
    while (i >= 0 && ((const Token*)buf1)[i] == ((const Token*)buf2)[i]);
    return (num - i - 1);
}

static unsigned match_forward(BDelta_Instance *b, unsigned p1, unsigned p2) noexcept
{ 
    unsigned num = 0, match, numtoread;
    Token * buf1 = b->match_buffer.get(), * buf2 = buf1 + TOKEN_BUFFER_SIZE;
    do 
    {
        numtoread = std::min<unsigned>(std::min(b->data1_size - p1, b->data2_size - p2), TOKEN_BUFFER_SIZE);
        const Token *read1 = b->read1(buf1, p1, numtoread),
                    *read2 = b->read2(buf2, p2, numtoread);

        if (b->errorcode != BDELTA_OK)
            return 0;

        p1 += numtoread;
        p2 += numtoread;
        match = match_buf_forward(read1, read2, numtoread);
        num += match;
    } while (match && match == numtoread);
    return num;
}

static unsigned match_backward(BDelta_Instance *b, unsigned p1, unsigned p2, unsigned blocksize) noexcept
{
    unsigned num = 0u, match, numtoread;
    Token * buf1 = b->match_buffer.get(), * buf2 = buf1 + TOKEN_BUFFER_SIZE;
    do 
    {
        numtoread = std::min<unsigned>(std::min(std::min(p1, p2), blocksize), TOKEN_BUFFER_SIZE);
        p1 -= numtoread;
        p2 -= numtoread;
        const Token *read1 = b->read1(buf1, p1, numtoread),
                    *read2 = b->read2(buf2, p2, numtoread);

        if (b->errorcode != BDELTA_OK)
            return 0;

        match = match_buf_backward(read1, read2, numtoread);
        num += match;
    } while (match && match == numtoread);
    return num;
}

// Iterator helper function
template <class T>
static inline T bdelta_next(T i) noexcept { return ++i; }

const auto compareMatchP2 = [](const Match& r1, const Match& r2) noexcept
{
    return ((r1.p2 != r2.p2) ? (r1.p2 < r2.p2) : (r1.num > r2.num));
};

static void addMatch(BDelta_Instance *b, unsigned p1, unsigned p2, unsigned num, MatchListIterator place) noexcept
{
    Match newMatch = Match(p1, p2, num);
    while (place != b->matches.begin() && !compareMatchP2(*place, newMatch))
        --place;
    while (place != b->matches.end() && compareMatchP2(*place, newMatch))
        ++place;
    if (b->matches.insert(place, newMatch) == nullptr)
        b->errorcode = BDELTA_MEM_ERROR;
}

template<class T>
T absoluteDifference(T a, T b) noexcept
{
    return std::max(a, b) - std::min(a, b);
}

static void findMatches(BDelta_Instance *b, Checksums_Instance *h, unsigned minMatchSize, unsigned start, unsigned end, unsigned place, MatchListIterator iterPlace) noexcept
{
    const unsigned blocksize = h->blocksize;

    if (!b->find_matches_buffer.resize(blocksize * 2))
    {
        b->errorcode = BDELTA_MEM_ERROR;
        return;
    }

    Token * buf1 = b->find_matches_buffer.get(), * buf2 = buf1 + blocksize;

    unsigned best1 = 0, best2 = 0, bestnum = 0;
    unsigned processMatchesPos = 0;
    const Token *inbuf = b->read2(buf1, start, blocksize),
                *outbuf = nullptr;
    Hash hash(inbuf, blocksize);
    unsigned buf_loc = blocksize;
    for (unsigned j = start + blocksize; ; ++j) 
    {
        unsigned thisTableIndex = h->tableIndex(hash.getValue());
        checksum_entry *c = h->htable[thisTableIndex];
        if (c) 
        {
            do 
            {
                if (c->cksum == hash.getValue()) 
                {
                    unsigned p1 = c->loc, p2 = j - blocksize;
                    unsigned fnum = match_forward(b, p1, p2);
                    if (b->errorcode != BDELTA_OK)
                        return;

                    if (fnum >= blocksize) 
                    {
                        unsigned bnum = match_backward(b, p1, p2, blocksize);
                        if (b->errorcode != BDELTA_OK)
                            return;

                        unsigned num = fnum + bnum;
                        if (num >= minMatchSize) 
                        {
                            p1 -= bnum; p2 -= bnum;
                            bool foundBetter;
                            if (bestnum) 
                            {
                                double oldValue = double(bestnum) / (absoluteDifference(place, best1) + blocksize * 2),
                                       newValue = double(num) / (absoluteDifference(place, p1) + blocksize * 2);
                                foundBetter = newValue > oldValue;
                            } 
                            else 
                            {
                                foundBetter = true;
                                processMatchesPos = std::min(j + blocksize - 1, end);
                            }
                            if (foundBetter) 
                            {
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

        if (bestnum && j >= processMatchesPos) 
        {
            addMatch(b, best1, best2, bestnum, iterPlace);
            if (b->errorcode != BDELTA_OK)
                return;

            place = best1 + bestnum;
            unsigned matchEnd = best2 + bestnum;
            if (matchEnd > j) 
            {
                if (matchEnd >= end)
                    j = end;
                else 
                {
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

        if (buf_loc == blocksize) 
        {
            buf_loc = 0;
            std::swap(inbuf, outbuf);
            inbuf = b->read2(outbuf == buf1 ? buf2 : buf1, j, std::min(end - j, blocksize));
        }

        if (j >= end)
            break;

        hash.advance(outbuf[buf_loc], inbuf[buf_loc]);
        ++buf_loc;
    }
}

struct Checksums_Compare 
{
    Checksums_Instance &ci;
    explicit Checksums_Compare(Checksums_Instance &h) noexcept : ci(h) {}
    bool operator() (checksum_entry c1, checksum_entry c2) noexcept
    {
        unsigned ti1 = ci.tableIndex(c1.cksum), ti2 = ci.tableIndex(c2.cksum);
        if (ti1 == ti2)
        {
            if (c1.cksum == c2.cksum)
                return c1.loc < c2.loc;
            else
                return c1.cksum < c2.cksum;
        }
        else
            return ti1 < ti2;
    }
};

BDelta_Instance *bdelta_init_alg(unsigned data1_size, unsigned data2_size,
                                 bdelta_readCallback cb, void *handle1, void *handle2,
                                 unsigned tokenSize) noexcept
{
    if (tokenSize != sizeof(Token)) 
    {
        printf("Error: BDelta library compiled for token size of %lu.\n", (unsigned long)sizeof (Token));
        return nullptr;
    }
    void * p = malloc(sizeof(BDelta_Instance));
    if (p == nullptr)
        return nullptr;

    return (new(p) BDelta_Instance(cb, handle1, handle2, data1_size, data2_size));
}

void bdelta_done_alg(BDelta_Instance *b) noexcept
{
    free(b);
}

// Adapted from http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static unsigned roundUpPowerOf2(unsigned v) noexcept
{
    --v;
    for (int i = 1; i <= 16; i *= 2)
        v |= (v >> i);
    return (v + 1);
}

static void bdelta_pass_2(BDelta_Instance *b, uint32_t blocksize, uint32_t minMatchSize, UnusedRange *unused, uint32_t numunused, UnusedRange *unused2) noexcept
{
    b->access_int = -1;

    unsigned numblocks = 0;
    for (unsigned i = 0; i < numunused; ++i)
        numblocks += unused[i].num;
    numblocks /= blocksize;

    b->bdelta_pass_2_htable.resize(std::max(2u, roundUpPowerOf2(numblocks)));
    b->bdelta_pass_2_hchecksums.resize(numblocks + 2);
    if (!(b->bdelta_pass_2_htable.resize(std::max(2u, roundUpPowerOf2(numblocks))) && b->bdelta_pass_2_hchecksums.resize(numblocks + 2)))
    {
        b->errorcode = BDELTA_MEM_ERROR;
        return;
    }

    Checksums_Instance h(blocksize, b->bdelta_pass_2_htable.size(), b->bdelta_pass_2_htable.get(), b->bdelta_pass_2_hchecksums.get());

    b->bdelta_pass_2_buf.resize(blocksize);
    if (!b->bdelta_pass_2_buf.resize(blocksize))
    {
        b->errorcode = BDELTA_MEM_ERROR;
        return;
    }

    for (unsigned i = 0; i < numunused; ++i)
    {
        unsigned first = unused[i].p, last = unused[i].p + unused[i].num;
        for (unsigned loc = first; loc + blocksize <= last; loc += blocksize) 
        {
            const Token *read = b->read1(b->bdelta_pass_2_buf.get(), loc, blocksize);
            Hash::Value blocksum = Hash(read, blocksize).getValue();
            // Adjacent checksums are never repeated.
            h.add(checksum_entry(blocksum, loc));
        }
    }

    if (h.numchecksums) 
    {
        std::sort(h.checksums, h.checksums + h.numchecksums, Checksums_Compare(h));
        const unsigned maxIdenticalChecksums = 2;
        unsigned writeLoc = 0, readLoc, testAhead;
        for (readLoc = 0; readLoc < h.numchecksums; readLoc = testAhead) 
        {
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

    for (int i = h.numchecksums - 1; i >= 0; --i)
        h.htable[h.tableIndex(h.checksums[i].cksum)] = &h.checksums[i];

    for (unsigned i = 0; i < numunused; ++i)
    {
        if (unused2[i].num >= blocksize)
        {
            findMatches(b, &h, minMatchSize, unused2[i].p, unused2[i].p + unused2[i].num, unused[i].p, unused2[i].mr);
            if (b->errorcode != BDELTA_OK)
                return;
        }
    }
}

void bdelta_swap_inputs(BDelta_Instance *b) noexcept
{
    for (auto& m : b->matches)
        std::swap(m.p1, m.p2);
    std::swap(b->data1_size, b->data2_size);
    std::swap(b->handle1, b->handle2);
    b->matches.sort(compareMatchP2);
}

void bdelta_clean_matches(BDelta_Instance *b, unsigned flags) noexcept
{
    auto nextL = b->matches.begin();
    if (nextL == b->matches.end()) 
        return;
    while (true) 
    {
        auto l = nextL;
        if (++nextL == b->matches.end())
            break;

        int overlap = l->p2 + l->num - nextL->p2;
        if (overlap >= 0) 
        {
            if ((unsigned)overlap >= nextL->num) 
            {
                b->matches.erase(nextL);
                nextL = l;
                continue;
            }
            if (flags & BDELTA_REMOVE_OVERLAP)
                l->num -= overlap;
        }
    }
}

void bdelta_showMatches(BDelta_Instance *b) noexcept
{
    for (const auto& m : b->matches)
        printf("(%d, %d, %d), ", m.p1, m.p2, m.num);
    printf("\n\n");
}

static void get_unused_blocks(UnusedRange *unused, uint32_t numunused) noexcept
{
    unsigned nextStartPos = 0;
    for (unsigned i = 1; i < numunused; ++i) 
    {
        unsigned startPos = nextStartPos;
        nextStartPos = std::max(startPos, unused[i].p + unused[i].num);
        unused[i].set(startPos, unused[i].p < startPos ? 0 : unused[i].p - startPos, unused[i-1].mr, unused[i].mr);
    }
}

void bdelta_pass(BDelta_Instance *b, uint32_t blocksize, uint32_t minMatchSize, uint32_t maxHoleSize, uint32_t flags) noexcept
{
    // Place an empty Match at beginning so we can assume there's a Match to the left of every hole.
    if (b->matches.emplace_front(0, 0, 0) == nullptr)
    {
        b->errorcode = BDELTA_MEM_ERROR;
        return;
    }
    // Trick for including the free range at the end.
    if (b->matches.emplace_back(b->data1_size, b->data2_size, 0) == nullptr)
    {
        b->errorcode = BDELTA_MEM_ERROR;
        return;
    }

    const size_t BUFFER_SIZE = b->matches.size() + 1;
    if (!b->bdelta_pass_unused.resize(BUFFER_SIZE * 2))
    {
        b->errorcode = BDELTA_MEM_ERROR;
        return;
    }

    UnusedRange *unused = b->bdelta_pass_unused.get(),
                *unused2 = unused + BUFFER_SIZE;

    uint32_t numunused = 0;
    for (auto l = b->matches.begin(); l != b->matches.end(); ++l)
    {
        unused[numunused].set(l->p1, l->num, l, l);
        unused2[numunused++].set(l->p2, l->num, l, l);
    }

    // Leave empty match at beginning
    std::sort(unused + 1, unused + numunused, [](const UnusedRange& r1, const UnusedRange& r2) noexcept
    {
        return ((r1.p != r2.p) ? (r1.p < r2.p) : (r1.num > r2.num));
    }); 

    get_unused_blocks(unused,  numunused);
    get_unused_blocks(unused2, numunused);

    if ((flags & BDELTA_GLOBAL) != 0)
    {
        bdelta_pass_2(b, blocksize, minMatchSize, unused, numunused, unused2);
        if (b->errorcode != BDELTA_OK)
            return;
    }  
    else
    {
        std::sort(unused + 1, unused + numunused, [](const UnusedRange& r1, const UnusedRange& r2) noexcept
        {
            return ((r1.mr->p2 != r2.mr->p2) ? (r1.mr->p2 < r2.mr->p2) : (r1.mr->num > r2.mr->num));
        });
        for (unsigned i = 1; i < numunused; ++i)
        {
            UnusedRange u1 = unused[i], u2 = unused2[i];
            if (u1.num >= blocksize && u2.num >= blocksize)
                if (!maxHoleSize || (u1.num <= maxHoleSize && u2.num <= maxHoleSize))
                    if ((flags & BDELTA_SIDES_ORDERED) == 0 || (bdelta_next(u1.ml) == u1.mr && bdelta_next(u2.ml) == u2.mr))
                    {
                        bdelta_pass_2(b, blocksize, minMatchSize, &u1, 1, &u2);
                        if (b->errorcode != BDELTA_OK)
                            return;
                    }    
        }
    }

    if (verbose) 
        printf("pass (blocksize: %u, matches: %u)\n", blocksize, (unsigned)b->matches.size());

    // Get rid of the dummy values we placed at the ends.
    b->matches.erase(std::find_if(b->matches.begin(), b->matches.end(), [](const Match &m) noexcept { return m.num == 0; }));
    b->matches.pop_back();
}

unsigned bdelta_numMatches(BDelta_Instance *b) noexcept
{
    return b->matches.size();
}

void bdelta_getMatch(BDelta_Instance *b, unsigned matchNum, unsigned *p1, unsigned *p2, unsigned *num) noexcept
{
    int &access_int = b->access_int;
    auto& accessplace = b->accessplace;
    if (access_int == -1) 
    {
        access_int = 0;
        accessplace = b->matches.begin();
    }
    while ((unsigned)access_int < matchNum) 
    {
        ++accessplace;
        ++access_int;
    }
    while ((unsigned)access_int > matchNum) 
    {
        --accessplace;
        --access_int;
    }
    *p1 = accessplace->p1;
    *p2 = accessplace->p2;
    *num = accessplace->num;
}

BDELTA_RESULT * bdelta_getError(BDelta_Instance *instance) noexcept
{
    return &(instance->errorcode);
}
