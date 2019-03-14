/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <inttypes.h>
#ifdef USE_CXX17
#include <filesystem>
#endif // USE_CXX17
#include <stdio.h>
#include <type_traits>

#define MAX_IO_BLOCK_SIZE (1024 * 1024)

static char error_message_buffer[512];

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
            snprintf(error_message_buffer, sizeof(error_message_buffer), "read error: fread_fixed(block_size=%u) != %u", block_size, (unsigned)r);
            throw error_message_buffer;
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
        unsigned block_size = num_bytes;
        if (block_size > MAX_IO_BLOCK_SIZE) block_size = MAX_IO_BLOCK_SIZE;

        size_t r = fwrite(buf, 1, block_size, f);
        if (r != block_size)
        {
            snprintf(error_message_buffer, sizeof(error_message_buffer), "write error: fwrite_fixed(num_bytes=%u) != %u", block_size, (unsigned)r);
            throw error_message_buffer;
        }
        buf       += block_size;
        num_bytes -= block_size;
    }
}

#ifdef BIG_ENDIAN

unsigned read_word(FILE *f) 
{
    unsigned char b, b2;
    fread_fixed(f, &b, 1);
    fread_fixed(f, &b2, 1);
    return (b2 << 8) + b;
}

unsigned read_dword(FILE *f) 
{
    unsigned low = read_word(f);
    return (read_word(f) << 16) + low;
}

void write_word(FILE *f, unsigned number) 
{
    unsigned char b = number & 255,
                  b2 = number >> 8;
    fwrite_fixed(f, &b, 1);
    fwrite_fixed(f, &b2, 1);
}

void write_dword(FILE *f, unsigned number) 
{
    write_word(f, number & 65535);
    write_word(f, number >> 16);
}

#else

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
inline T read_type(FILE *f)
{
    T result = 0;
    fread_fixed(f, &result, sizeof(result));
    return result;
}

#define read_byte(f)  read_type<uint8_t>((f))
#define read_word(f)  read_type<uint16_t>((f))
#define read_dword(f) read_type<uint32_t>((f))


inline void write_word(FILE *f, uint16_t number)
{
    fwrite_fixed(f, &number, sizeof(number));
}

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
inline void write_type(FILE *f, T number)
{
    fwrite_fixed(f, &number, sizeof(number));
}

#define write_byte(f, v)  write_type<uint8_t>((f), (v))
#define write_word(f, v)  write_type<uint16_t>((f), (v))
#define write_dword(f, v) write_type<uint32_t>((f), (v))

#endif // BIG_ENDIAN

#ifdef USE_CXX17

template <class T>
inline bool fileExists(const T& fname) 
{
    std::error_code ec;
    return std::filesystem::exists(fname, ec);
}

template <class T>
inline uint64_t getLenOfFile(const const T& fname) 
{
    std::error_code ec;
    return std::filesystem::file_size(fname, ec);
}

#else
    
bool fileExists(const char * fname) 
{
    FILE *f = fopen(fname, "rb");
    bool exists = (f != nullptr);
    if (exists) 
        fclose(f);
    return exists;
}

unsigned getLenOfFile(const char * fname) 
{
    FILE *f = fopen(fname, "rb");
    fseek(f, 0, SEEK_END);
    unsigned len = ftell(f);
    fclose(f);
    return len;
}

#endif // USE_CXX17
