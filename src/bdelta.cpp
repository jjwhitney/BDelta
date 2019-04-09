/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <inttypes.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bdelta.h"
#include "file.h"

static const void * f_read(void *f, void *buf, unsigned place, unsigned num, BDelta_Instance * /*b*/, BDELTA_RESULT& result)
{
    fseek((FILE *)f, place, SEEK_SET);
    result = fread_fixed((FILE *)f, buf, num) ? BDELTA_OK : BDELTA_READ_ERROR;
    return buf;
}

static const char * fatal_alloc = "FATAL: unable to allocate memory\n";
static const char * fatal_read  = "FATAL: unable to read file\n";
static const char * fatal_write = "FATAL: unable to write file\n";

static void my_pass(BDelta_Instance *b, unsigned blocksize, unsigned minMatchSize, unsigned flags)
{
    bdelta_pass(b, blocksize, minMatchSize, 0, flags);
    switch (*bdelta_getError(b))
    {
    case BDELTA_MEM_ERROR:
        printf(fatal_alloc);
        exit(1);
    case BDELTA_READ_ERROR:
        printf(fatal_read);
        exit(2);
    case BDELTA_WRITE_ERROR:
        printf(fatal_write);
        exit(3);
    }

    bdelta_clean_matches(b, BDELTA_REMOVE_OVERLAP);
}

#define check_write_result(result) if(!(result)) { printf(fatal_read); return 3; }
#define check_read_result(result) if(!(result)) { printf(fatal_write); return 2; }
#define check_alloc(ptr) if (!(ptr)) { printf(fatal_alloc); exit(1); }

int main(int argc, char **argv) 
{
    std::unique_ptr<uint8_t[], void(*)(void*)> m1(nullptr, free), m2(nullptr, free);

    const bool all_ram_mode = (argc > 1 && strcmp(argv[1], "--all-in-ram") == 0);
    if (all_ram_mode)
    {
        --argc;
        ++argv;
    }
    if (argc != 4)
    {
        printf("usage: bdelta [--all-in-ram] <oldfile> <newfile> <patchfile>\n");
        printf("needs two files to compare + output file:\n");
        exit(1);
    }
    if (!fileExists(argv[1]) || !fileExists(argv[2]))
    {
        printf("one of the input files does not exist\n");
        exit(1);
    }
    uint32_t size = (uint32_t)getLenOfFile(argv[1]);
    uint32_t size2 = (uint32_t)getLenOfFile(argv[2]);
    std::unique_ptr<FILE, int(*)(FILE*)> f1(fopen(argv[1], "rb"), fclose);
    if (!f1)
    {
        printf("unable to open file %s\n", argv[1]);
        exit(1);
    }


    std::unique_ptr<FILE, int(*)(FILE*)> f2(fopen(argv[2], "rb"), fclose);
    if (!f2)
    {
        printf("unable to open file %s\n", argv[2]);
        exit(1);
    }


    BDelta_Instance * b = nullptr;

    if (all_ram_mode)
    {
        m1.reset((uint8_t*)malloc(size));
        m2.reset((uint8_t*)malloc(size2));
        check_alloc(m1 && m2);
        fread_fixed(f1.get(), m1.get(), size);
        fread_fixed(f2.get(), m2.get(), size2);

        b = bdelta_init_alg(size, size2, nullptr, m1.get(), m2.get(), 1);
    }
    else
        b = bdelta_init_alg(size, size2, f_read, f1.get(), f2.get(), 1);

    check_alloc(b);

    std::unique_ptr<BDelta_Instance, void(*)(BDelta_Instance*)> b_holder(b, bdelta_done_alg);

    int nummatches = 0;

    // List of primes for reference. Taken from Wikipedia.
    //            1      2      3      4      5      6      7      8      9     10     11     12     13     14     15     16     17     18     19     20
    // 1-20       2      3      5      7     11     13     17     19     23     29     31     37     41     43     47     53     59     61     67     71
    // 21-40     73     79     83     89     97    101    103    107    109    113    127    131    137    139    149    151    157    163    167    173
    // 41-60    179    181    191    193    197    199    211    223    227    229    233    239    241    251    257    263    269    271    277    281
    // 61-80    283    293    307    311    313    317    331    337    347    349    353    359    367    373    379    383    389    397    401    409
    // 81-100   419    421    431    433    439    443    449    457    461    463    467    479    487    491    499    503    509    521    523    541
    // 101-120  547    557    563    569    571    577    587    593    599    601    607    613    617    619    631    641    643    647    653    659
    // 121-140  661    673    677    683    691    701    709    719    727    733    739    743    751    757    761    769    773    787    797    809
    // 141-160  811    821    823    827    829    839    853    857    859    863    877    881    883    887    907    911    919    929    937    941
    // 161-180  947    953    967    971    977    983    991    997

    my_pass(b, 997, 1994, 0);
    my_pass(b, 503, 1006, 0);
    my_pass(b, 127, 254, 0);
    my_pass(b, 31, 62, 0);
    my_pass(b, 7, 14, 0);
    my_pass(b, 5, 10, 0);
    my_pass(b, 3, 6, 0);
    my_pass(b, 13, 26, BDELTA_GLOBAL);
    my_pass(b, 7, 14, 0);
    my_pass(b, 5, 10, 0);

    nummatches = bdelta_numMatches(b);

    const size_t mem_len = (nummatches + 1) * sizeof(uint32_t);
    std::unique_ptr<uint32_t[], void(*)(void*)> copyloc1((uint32_t*)malloc(mem_len), free),
                                                copyloc2((uint32_t*)malloc(mem_len), free),
                                                copynum((uint32_t*)malloc(mem_len), free);

    check_alloc(copyloc1 && copyloc2 && copynum);

    std::unique_ptr<FILE, int(*)(FILE*)> fout(fopen(argv[3], "wb"), fclose);
    if (!fout)
    {
        printf("couldn't open output file\n");
        exit(1);
    }
    constexpr size_t fout_buffer_size = 256 * 1024;
    setvbuf(fout.get(), nullptr, _IOFBF, fout_buffer_size);

    const char * magic = "BDT";
    check_write_result(fwrite_fixed(fout.get(), magic, 3));
    uint16_t version = 1;
    check_write_result(write_value(fout.get(), &version));
    uint8_t intsize = 4;
    check_write_result(write_value(fout.get(), &intsize));
    bool result = write_value(fout.get(), &size)
        && write_value(fout.get(), &size2)
        && write_value(fout.get(), &nummatches);
    check_write_result(result);

    unsigned lastp1 = 0, lastp2 = 0;
    for (int i = 0; i < nummatches; ++i)
    {
        unsigned p1 = 0, p2 = 0, num = 0;
        bdelta_getMatch(b, i, &p1, &p2, &num);
        copyloc1[i] = p1 - lastp1;
        result = write_value(fout.get(), &copyloc1[i]);
        copyloc2[i] = p2 - lastp2;
        result = result && write_value(fout.get(), &copyloc2[i]);
        copynum[i] = num;
        result = result && write_value(fout.get(), &copynum[i]);
        check_write_result(result);
        lastp1 = p1 + num;
        lastp2 = p2 + num;
    }
    if (size2 != lastp2)
    {
        copyloc1[nummatches] = 0;
        copynum[nummatches] = 0;
        copyloc2[nummatches] = size2 - lastp2;
        ++nummatches;
    }

    unsigned fp = 0;
    constexpr size_t WRITE_BUFFER_SIZE = 1024 * 1024;
    std::unique_ptr<uint8_t[], void (*)(void*)> write_buffer((uint8_t*)malloc(WRITE_BUFFER_SIZE), free);
    check_alloc(write_buffer);
    BDELTA_RESULT * res = bdelta_getError(b);
    for (int i = 0; i < nummatches; ++i)
    {
        unsigned num = copyloc2[i];
        while (num > 0)
        {
            unsigned towrite = std::min<unsigned>(num, WRITE_BUFFER_SIZE);
            const void * block = all_ram_mode ? (const void *)((char*)m2.get() + fp) : f_read(f2.get(), write_buffer.get(), fp, towrite, b, *res);
            check_read_result(*res == BDELTA_OK);
            check_write_result(fwrite_fixed(fout.get(), block, towrite));
            num -= towrite;
            fp += towrite;
        }

        if (i != nummatches)
            fp += copynum[i];
    }

    return 0;
}
