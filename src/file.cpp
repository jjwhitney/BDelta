/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "file.h"

#include <algorithm>
#include <memory>

#ifdef _MSC_VER
#define fwrite_unlocked _fwrite_nolock
#define fread_unlocked  _fread_nolock
#elif __MINGW32__
#define fwrite_unlocked fwrite
#define fread_unlocked  fread
#endif 

bool fread_fixed(FILE *f, void * _buf, unsigned num_bytes) noexcept
{
    char * buf = (char *)_buf;

    while (num_bytes != 0)
    {
        unsigned block_size = std::min<unsigned>(num_bytes, MAX_IO_BLOCK_SIZE);

        size_t r = fread_unlocked(buf, 1, block_size, f);
        if (r != block_size)
            return false;

        buf       += block_size;
        num_bytes -= block_size;
    }

    return true;
}

bool fwrite_fixed(FILE *f, const void * _buf, unsigned num_bytes) noexcept
{
    const char * buf = (const char *)_buf;

    while (num_bytes != 0)
    {
        unsigned block_size = std::min<unsigned>(num_bytes, MAX_IO_BLOCK_SIZE);

        size_t r = fwrite_unlocked(buf, 1, block_size, f);
        if (r != block_size)
            return false;

        buf       += block_size;
        num_bytes -= block_size;
    }

    return true;
}
