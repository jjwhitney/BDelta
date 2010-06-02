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
	typedef unsigned long long Value;
	Value value;

	Hash(Token *buf, unsigned blocksize) {
		value = 0;
		for (int num = 0; num < blocksize; ++num) {
			value *= multiplyamount;
			value += buf[num];
		}
		oldCoefficient = powHash(multiplyamount, blocksize);
	}
	void advance(Token out, Token in) {
		value *= multiplyamount;
		value -= out * oldCoefficient;
		value += in;
	}
        static unsigned modulo(Value hash, unsigned d) {
            // Assumes d is power of 2.
            return hash & (d-1);
	}
private:
	static Value powHash(Value x, unsigned y) {
		Value res = 1;
		while (y) {
			if (y&1) res*=x;
			x*=x;
			y>>=1;
		}
		return res;
	}

	static const unsigned multiplyamount = 181;
	Value oldCoefficient;
};

