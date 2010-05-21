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

typedef unsigned long long Checksum;

Checksum powChecksum(Checksum x, unsigned y) {
  Checksum res = 1;
  while (y) {
    if (y&1) res*=x;
    x*=x;
    y>>=1;
  }
//  for (unsigned i = 0; i < y; ++i) res*=x;
  return res;
}

static const unsigned multiplyamount = 181;

struct ChecksumManager {
	int blocksize;
	Checksum oldCoefficient;

	ChecksumManager(int blocksize) {
		this->blocksize = blocksize;
		oldCoefficient = powChecksum(multiplyamount, blocksize);
	}

	Checksum evaluateBlock(byte *buf) {
		Checksum ck = 0;
		for (int num = 0; num < blocksize; ++num) {
			ck *= multiplyamount;
			ck += buf[num];
		}
		return ck;
	}

	Checksum advanceChecksum(Checksum ck, byte out, byte in) {
		ck *= multiplyamount;
		ck -= out * oldCoefficient;
		ck += in;
		return ck;
	}
};

