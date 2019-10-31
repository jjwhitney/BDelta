/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __CHECKSUM_H__
#define __CHECKSUM_H__
 
struct Hash 
{
    typedef uint64_t Value;
    Hash(const Token * buf, unsigned blocksize) noexcept
    {
        value = 0;
        for (unsigned num = 0; num < blocksize; ++num)
            advance_add(buf[num]);
        oldCoefficient = powHash(multiplyAmount, blocksize);
    }
    void advance(Token out, Token in) noexcept
    {
        advance_remove(out);
        advance_add(in);
    }
    static unsigned modulo(Value hash, unsigned d) noexcept
    {
        // Assumes d is power of 2.
        return (hash & (d - 1));
    }
    Value getValue() noexcept { return value >> extraProcBits; }
private:
    typedef uint64_t ProcValue;
    static constexpr unsigned extraProcBits = (sizeof(ProcValue) - sizeof(Value)) * 8;

    static constexpr ProcValue multiplyAmount = ((1ll << extraProcBits) | 181);
    ProcValue oldCoefficient, value;

    void advance_add(Token in) noexcept
    {
        value = (value + in) * multiplyAmount;
    }
    void advance_remove(Token out) noexcept { value -= out * oldCoefficient; }
    static ProcValue powHash(ProcValue x, unsigned y) noexcept
    {
        ProcValue res = 1;
        while (y) 
        {
            if (y & 1)
                res *= x;
            x *= x;
            y >>= 1;
        }
        return res;
    }
};

#endif // __CHECKSUM_H__
