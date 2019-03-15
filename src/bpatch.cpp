/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <inttypes.h>
#include <memory>
#include <stdio.h>
#include <string.h>

#include "file.h"

static bool copy_bytes_to_file(FILE *infile, FILE *outfile, unsigned numleft)
{
    const size_t BUFFER_SIZE = 65536;
    std::unique_ptr<uint8_t[]> buf(new uint8_t[BUFFER_SIZE]);
    while (numleft != 0)
    {
        const size_t len = std::min<size_t>(BUFFER_SIZE, numleft);
        size_t numread = fread(buf.get(), 1, len, infile);
        if (len != numread)
        {
            printf("Could not read data\n");
            return false;
        }
        if (fwrite(buf.get(), 1, numread, outfile) != numread) 
        {
            printf("Could not write temporary data.  Possibly out of space\n");
            return false;
        }
        numleft -= numread;
    }

    return true;
}

int main(int argc, char **argv) 
{
    try 
    {
        if (argc != 4) 
        {
            printf("usage: bpatch <oldfile> <newfile> <patchfile>\n");
            printf("needs a reference file, file to output, and patchfile:\n");
            return 1;
        }

        if (!fileExists(argv[1]) || !fileExists(argv[3])) 
        {
            printf("one of the input files does not exist\n");
            return 1;
        }

        FILE *patchfile = fopen(argv[3], "rb");
        char magic[3];
        fread_fixed(patchfile, magic, 3);
        if (strncmp(magic, "BDT", 3)) 
        {
            printf("Given file is not a recognized patchfile\n");
            return 1;
        }
        unsigned short version = read_word(patchfile);
        if (version != 1) 
        {
            printf("unsupported patch version\n");
            return 1;
        }
        char intsize;
        fread_fixed(patchfile, &intsize, 1);
        if (intsize != 4) 
        {
            printf("unsupported file pointer size\n");
            return 1;
        }
        /*unsigned size1 =*/ read_dword(patchfile);
        unsigned size2 = read_dword(patchfile);

        unsigned nummatches = read_dword(patchfile);

        unsigned * copyloc1 = new unsigned[nummatches + 1];
        unsigned * copyloc2 = new unsigned[nummatches + 1];
        unsigned *  copynum = new unsigned[nummatches + 1];
        std::unique_ptr<unsigned[]> copyloc1_holder(copyloc1), copyloc2_holder(copyloc2), copynum_holder(copynum);

        for (unsigned i = 0; i < nummatches; ++i) 
        {
            copyloc1[i] = read_dword(patchfile);
            copyloc2[i] = read_dword(patchfile);
            copynum[i] = read_dword(patchfile);
            size2 -= copyloc2[i] + copynum[i];
        }
        if (size2) {
            copyloc1[nummatches] = 0; copynum[nummatches] = 0;
            copyloc2[nummatches] = size2;
            ++nummatches;
        }

        FILE * ref = fopen(argv[1], "rb");
        if (ref == nullptr)
        {
            printf("Error: unable to open file %s\n", argv[1]);
            return -1;
        }
        std::unique_ptr<FILE, int(*)(FILE*)> ref_holder(ref, fclose);

        const size_t fout_buffer_size = 65536;
        std::unique_ptr<char[]> outfile_buffer(new char[fout_buffer_size]);
        FILE * outfile = fopen(argv[2], "wb");
        if (outfile == nullptr)
        {
            printf("Error: unable to open file %s\n", argv[2]);
            return -1;
        }
        setvbuf(outfile, outfile_buffer.get(), _IOFBF, fout_buffer_size);
        std::unique_ptr<FILE, int(*)(FILE*)> outfile_holder(outfile, fclose);

        for (unsigned i = 0; i < nummatches; ++i) 
        {
            if (!copy_bytes_to_file(patchfile, outfile, copyloc2[i])) 
            {
                printf("Error: patchfile is truncated\n");
                return -1;
            }

            int copyloc = copyloc1[i];
            fseek(ref, copyloc, SEEK_CUR);

            if (!copy_bytes_to_file(ref, outfile, copynum[i])) 
            {
                printf("Error while copying from reference file\n");
                return -1;
            }
        }
    } 
    catch (const std::exception& ex)
    {
        fprintf (stderr, "FATAL: %s\n", ex.what());
        return -1;
    }

    return 0;
}
