/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FILE_H__
#define __FILE_H__

#include <inttypes.h>
#ifdef USE_CXX17
#include <filesystem>
#endif // USE_CXX17
#include <memory>
#include <stdio.h>
#include <system_error>
#include <type_traits>

static constexpr size_t MAX_IO_BLOCK_SIZE = (1024 * 1024);

bool fread_fixed(FILE *f, void * _buf, unsigned num_bytes) noexcept;
bool fwrite_fixed(FILE *f, const void * _buf, unsigned num_bytes) noexcept;

#ifdef BIG_ENDIAN_HOST
template <typename T>
struct HalfType;

template <>
struct HalfType<uint16_t>
{
    typedef uint8_t type;
};

template <>
struct HalfType<uint32_t>
{
    typedef uint16_t type;
};

template <typename T>
static inline bool read_value(FILE* f, T* value)
{
    typename HalfType::type low, high;
    if (!(fread_fixed(f, &low, sizeof(low)) && fread_fixed(f, &high, sizeof(high))))
        return false;
    constexpr int shift = sizeof(low) * 8;
    *value = ((high << shift) + low);
    return true;
}

template <> static inline bool read_value(FILE* f, uint8_t* value);
{
    return fread_fixed(f, value, sizeof(*value));
}

template <typename T>
static inline bool write_value(FILE* f, T* value)
{
    constexpr int shift = sizeof(HalfType::type) * 8;
    constexpr int mask = ((1 << shift) - 1);

    typename HalfType::type low = (*value & mask), high = (*value >> 8);
    return (fwrite_fixed(f, &low, sizeof(low)) && fwrite_fixed(f, &high, sizeof(high)));
}

template <> static inline bool write_value(FILE * f, uint8_t* value);
{
    return write_fixed(f, value, sizeof(*value));
}

#else

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
static inline bool read_value(FILE* f, T* value)
{
    return fread_fixed(f, value, sizeof(*value));
}

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
static inline bool write_value(FILE* f, T* value)
{
    return fwrite_fixed(f, value, sizeof(*value));
}

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
    
static inline bool fileExists(const char * fname)
{
    std::unique_ptr<FILE, int(*)(FILE*)> f(fopen(fname, "rb"), fclose);
    return (bool)f;
}

static inline unsigned getLenOfFile(const char * fname)
{
    std::unique_ptr<FILE, int(*)(FILE*)> f(fopen(fname, "rb"), fclose);
    fseek(f.get(), 0, SEEK_END);
    return ftell(f.get());
}

#endif // USE_CXX17

#endif // __FILE_H__
