/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "file.h"

#include <algorithm>

void fread_fixed(FILE *f, void * _buf, unsigned num_bytes)
{
    char * buf = (char *)_buf;

    while (num_bytes != 0)
    {
        unsigned block_size = num_bytes;
        if (block_size > MAX_IO_BLOCK_SIZE) 
            block_size = MAX_IO_BLOCK_SIZE;

        size_t r = fread(buf, 1, block_size, f);
        if (r != block_size)
        {
            const size_t BUFFER_SIZE = 512;
            std::unique_ptr<char[]> error_message_buffer(new char[BUFFER_SIZE]);
            snprintf(error_message_buffer.get(), BUFFER_SIZE, "read error: fread_fixed(block_size=%u) != %u", block_size, (unsigned)r);
            throw std::system_error(EIO, std::generic_category(), std::string(error_message_buffer.get()));
        }
        buf       += block_size;
        num_bytes -= block_size;
    }
}

void fwrite_fixed(FILE *f, const void * _buf, unsigned num_bytes)
{
    const char * buf = (const char *)_buf;

    while (num_bytes != 0)
    {
        unsigned block_size = std::min<unsigned>(num_bytes, MAX_IO_BLOCK_SIZE);

        size_t r = fwrite(buf, 1, block_size, f);
        if (r != block_size)
        {
            const size_t BUFFER_SIZE = 512;
            std::unique_ptr<char[]> error_message_buffer(new char[BUFFER_SIZE]);
            snprintf(error_message_buffer.get(), BUFFER_SIZE, "write error: fwrite_fixed(num_bytes=%u) != %u", block_size, (unsigned)r);
            throw std::system_error(EIO, std::generic_category(), std::string(error_message_buffer.get()));
        }
        buf       += block_size;
        num_bytes -= block_size;
    }
}
