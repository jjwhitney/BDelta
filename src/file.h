/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FILE_H__
#define __FILE_H__

#include <inttypes.h>
#ifdef USE_CXX17
#include <filesystem>
#endif // USE_CXX17
#include <stdio.h>
#include <system_error>
#include <type_traits>

static constexpr size_t MAX_IO_BLOCK_SIZE = (1024 * 1024);

void fread_fixed(FILE *f, void * _buf, unsigned num_bytes);
void fwrite_fixed(FILE *f, const void * _buf, unsigned num_bytes);


#ifdef BIG_ENDIAN_HOST

static inline unsigned read_word(FILE *f)
{
    unsigned char b, b2;
    fread_fixed(f, &b, 1);
    fread_fixed(f, &b2, 1);
    return (b2 << 8) + b;
}

static inline unsigned read_dword(FILE *f)
{
    unsigned low = read_word(f);
    return (read_word(f) << 16) + low;
}

static inline void write_word(FILE *f, unsigned number)
{
    unsigned char b = number & 255,
                  b2 = number >> 8;
    fwrite_fixed(f, &b, 1);
    fwrite_fixed(f, &b2, 1);
}

static inline void write_dword(FILE *f, unsigned number)
{
    write_word(f, number & 65535);
    write_word(f, number >> 16);
}

#else

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
static inline T read_type(FILE *f)
{
    T result = 0;
    fread_fixed(f, &result, sizeof(result));
    return result;
}

#define read_byte(f)  read_type<uint8_t>((f))
#define read_word(f)  read_type<uint16_t>((f))
#define read_dword(f) read_type<uint32_t>((f))

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
static inline void write_type(FILE *f, T number)
{
    fwrite_fixed(f, &number, sizeof(number));
}

#define write_byte(f, v)  write_type<uint8_t>((f), (v))
#define write_word(f, v)  write_type<uint16_t>((f), (v))
#define write_dword(f, v) write_type<uint32_t>((f), (v))

#endif // BIG_ENDIAN

#ifdef USE_CXX17

template <class T>
static inline bool fileExists(const T& fname)
{
    std::error_code ec;
    return std::filesystem::exists(fname, ec);
}

template <class T>
static inline uint64_t getLenOfFile(const T& fname)
{
    std::error_code ec;
    return std::filesystem::file_size(fname, ec);
}

#else
    
static bool fileExists(const char * fname)
{
    FILE *f = fopen(fname, "rb");
    bool exists = (f != nullptr);
    if (exists) 
        fclose(f);
    return exists;
}

static unsigned getLenOfFile(const char * fname)
{
    FILE *f = fopen(fname, "rb");
    fseek(f, 0, SEEK_END);
    unsigned len = ftell(f);
    fclose(f);
    return len;
}

#endif // USE_CXX17

#endif // __FILE_H__
