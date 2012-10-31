/* Copyright (C) 2003-2010  John Whitney
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: John Whitney <jjw@deltup.org>
 */


struct Hash {
public:
	typedef uint64_t Value;
	Hash() {}
	Hash(const Token *buf, unsigned blocksize) {
		value = 0;
		for (unsigned num = 0; num < blocksize; ++num)
			advance_add(buf[num]);
		oldCoefficient = powHash(multiplyAmount, blocksize);
	}
	void advance(Token out, Token in) {
		advance_remove(out);
		advance_add(in);
	}
	static unsigned modulo(Value hash, unsigned d) {
		// Assumes d is power of 2.
		return hash & (d - 1);
	}
	Value getValue() {return value >> extraProcBits;}
private:
	typedef uint64_t ProcValue;
	static const unsigned extraProcBits = (sizeof(ProcValue) - sizeof(Value)) * 8;

	static const ProcValue multiplyAmount = (1ll << extraProcBits) | 181;
	ProcValue oldCoefficient, value;

	void advance_add(Token in) {
		value += in;
		value *= multiplyAmount;
	}
	void advance_remove(Token out) {
		value -= out * oldCoefficient;
	}
	static ProcValue powHash(ProcValue x, unsigned y) {
		ProcValue res = 1;
		while (y) {
			if (y & 1) res *= x;
			x *= x;
			y >>= 1;
		}
		return res;
	}
};

