/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <inttypes.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system_error>

#include "bdelta.h"
#include "file.h"

static const void *f_read(void *f, void *buf, unsigned place, unsigned num)
{
    fseek((FILE *)f, place, SEEK_SET);
    fread_fixed((FILE *)f, buf, num);
    return buf;
}

static inline const void * m_read(void *f, void * /*buf*/, unsigned place, unsigned /*num*/)
{
    return (const char*)f + place;
}

static void my_pass(BDelta_Instance *b, unsigned blocksize, unsigned minMatchSize, unsigned flags)
{
    bdelta_pass(b, blocksize, minMatchSize, 0, flags);
    bdelta_clean_matches(b, BDELTA_REMOVE_OVERLAP);
}

int main(int argc, char **argv) 
{
    try 
    {
        bool all_ram_mode = false;
        std::unique_ptr<char[]> m1;
        std::unique_ptr<char[]> m2;

        if (argc > 1 && strcmp(argv[1], "--all-in-ram") == 0)
        {
            all_ram_mode = true;
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
        unsigned size  = (unsigned)getLenOfFile(argv[1]);
        unsigned size2 = (unsigned)getLenOfFile(argv[2]);
        FILE * f1 = fopen(argv[1], "rb");
        if (f1 == nullptr)
        {
            printf("unable to open file %s\n", argv[1]);
            exit(1);
        }
        std::unique_ptr<FILE, int(*)(FILE*)> f1_holder(f1, fclose);

        FILE * f2 = fopen(argv[2], "rb");
        if (f2 == nullptr)
        {
            printf("unable to open file %s\n", argv[2]);
            exit(1);
        }
        std::unique_ptr<FILE, int(*)(FILE*)> f2_holder(f2, fclose);
        
        BDelta_Instance * b = nullptr;

        if (all_ram_mode)
        {
            m1.reset(new char[size]);
            m2.reset(new char[size2]);
            fread_fixed(f1, m1.get(), size);
            fread_fixed(f2, m2.get(), size2);

            b = bdelta_init_alg(size, size2, m_read, m1.get(), m2.get(), 1);
        }
        else
            b = bdelta_init_alg(size, size2, f_read, f1, f2, 1);

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
        my_pass(b,  31,  62, 0);
        my_pass(b,   7,  14, 0);
        my_pass(b,   5,  10, 0);
        my_pass(b,   3,   6, 0);
        my_pass(b,  13,  26, BDELTA_GLOBAL);
        my_pass(b,   7,  14, 0);
        my_pass(b,   5,  10, 0);

        nummatches = bdelta_numMatches(b);

        unsigned * copyloc1 = new unsigned[nummatches + 1];
        unsigned * copyloc2 = new unsigned[nummatches + 1];
        unsigned *  copynum = new unsigned[nummatches + 1];
        std::unique_ptr<unsigned[]> copyloc1_holder(copyloc1), copyloc2_holder(copyloc2), copynum_holder(copynum);

        FILE *fout = fopen(argv[3], "wb");
        if (fout == nullptr) 
        {
            printf("couldn't open output file\n");
            exit(1);
        }
        std::unique_ptr<FILE, int(*)(FILE*)> fout_holder(fout, fclose);

        const char * magic = "BDT";
        fwrite_fixed(fout, magic, 3);
        unsigned short version = 1;
        write_word(fout, version);
        unsigned char intsize = 4;
        fwrite_fixed(fout, &intsize, 1);
        write_dword(fout, size);
        write_dword(fout, size2);
        write_dword(fout, nummatches);

        unsigned lastp1 = 0, lastp2 = 0;
        for (int i = 0; i < nummatches; ++i) 
        {
            unsigned p1 = 0, p2 = 0, num = 0;
            bdelta_getMatch(b, i, &p1, &p2, &num);
            copyloc1[i] = p1 - lastp1;
            write_dword(fout, copyloc1[i]);
            copyloc2[i] = p2 - lastp2;
            write_dword(fout, copyloc2[i]);
            copynum[i] = num;
            write_dword(fout, copynum[i]);
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
        const size_t WRITE_BUFFER_SIZE = 4096;
        std::unique_ptr<uint8_t[]> write_buffer(new uint8_t[WRITE_BUFFER_SIZE]);
        for (int i = 0; i < nummatches; ++i) 
        {
            unsigned num = copyloc2[i];
            while (num > 0) 
            {
                unsigned towrite = std::min<unsigned>(num, WRITE_BUFFER_SIZE);
                const void * block = all_ram_mode ? m_read(m2.get(), write_buffer.get(), fp, towrite) : f_read(f2, write_buffer.get(), fp, towrite);
                fwrite_fixed(fout, block, towrite);
                num -= towrite;
                fp += towrite;
            }

            if (i != nummatches) 
                fp += copynum[i];
        }

    } 
    catch (const std::exception& ex)
    {
        fprintf (stderr, "FATAL: %s\n", ex.what());
        return -1;
    }

    return 0;
}
