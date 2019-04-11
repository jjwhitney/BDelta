/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <inttypes.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"

#ifdef _MSC_VER
#define fread_unlocked _fread_nolock
#define fwrite_unlocked _fwrite_nolock
#endif

static bool copy_bytes_to_file(FILE *infile, FILE *outfile, unsigned numleft)
{
    const size_t BUFFER_SIZE = 65536;
    std::unique_ptr<uint8_t[], void(*)(void*)> buf((uint8_t*)malloc(BUFFER_SIZE), free);
    while (numleft != 0)
    {
        const size_t len = std::min<size_t>(BUFFER_SIZE, numleft);
        size_t numread = fread_unlocked(buf.get(), 1, len, infile);
        if (len != numread)
        {
            printf("Could not read data\n");
            return false;
        }
        if (fwrite_unlocked(buf.get(), 1, numread, outfile) != numread)
        {
            printf("Could not write temporary data. Possibly out of space\n");
            return false;
        }
        numleft -= numread;
    }

    return true;
}

#define check_read_result(result) if(!(result)) { printf("FATAL: I/O error on reading\n"); return 2; }

int main(int argc, char **argv) 
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

    std::unique_ptr<FILE, int(*)(FILE*)> patchfile(fopen(argv[3], "rb"), fclose);
    char magic[3];
    check_read_result(fread_fixed(patchfile.get(), magic, 3));
    if (strncmp(magic, "BDT", 3))
    {
        printf("Given file is not a recognized patchfile\n");
        return 1;
    }
    uint16_t version = 0;
    uint8_t intsize = 0;
    check_read_result(read_value(patchfile.get(), &version));
    if (version != 1)
    {
        printf("unsupported patch version\n");
        return 1;
    }
    check_read_result(read_value(patchfile.get(), &intsize));
    if (intsize != 4)
    {
        printf("unsupported file pointer size\n");
        return 1;
    }
    uint32_t size1 = 0, size2 = 0, nummatches = 0;
    bool result = read_value(patchfile.get(), &size1) 
               && read_value(patchfile.get(), &size2) 
               && read_value(patchfile.get(), &nummatches);
    check_read_result(result);

    std::unique_ptr<uint32_t[], void(*)(void*)> copyloc1((uint32_t*)malloc(nummatches + 1), free),
                                                copyloc2((uint32_t*)malloc(nummatches + 1), free),
                                                copynum((uint32_t*)malloc(nummatches + 1), free);

    for (unsigned i = 0; i < nummatches; ++i)
    {
        result = read_value(patchfile.get(), &copyloc1[i])
              && read_value(patchfile.get(), &copyloc2[i])
              && read_value(patchfile.get(), &copynum[i]);
        check_read_result(result);
        size2 -= copyloc2[i] + copynum[i];
    }
    if (size2)
    {
        copyloc1[nummatches] = 0;
        copynum[nummatches] = 0;
        copyloc2[nummatches] = size2;
        ++nummatches;
    }

    std::unique_ptr<FILE, int(*)(FILE*)> ref(fopen(argv[1], "rb"), fclose);
    if (!ref)
    {
        printf("Error: unable to open file %s\n", argv[1]);
        return -1;
    }


    std::unique_ptr<FILE, int(*)(FILE*)> outfile(fopen(argv[2], "wb"), fclose);
    if (!outfile)
    {
        printf("Error: unable to open file %s\n", argv[2]);
        return -1;
    }
    constexpr size_t outfile_buffer_size = 256 * 1024;
    setvbuf(outfile.get(), nullptr, _IOFBF, outfile_buffer_size);

    for (unsigned i = 0; i < nummatches; ++i)
    {
        if (!copy_bytes_to_file(patchfile.get(), outfile.get(), copyloc2[i]))
        {
            printf("Error: patchfile is truncated\n");
            return -1;
        }

        fseek(ref.get(), copyloc1[i], SEEK_CUR);

        if (!copy_bytes_to_file(ref.get(), outfile.get(), copynum[i]))
        {
            printf("Error while copying from reference file\n");
            return -1;
        }
    }

    return 0;
}
