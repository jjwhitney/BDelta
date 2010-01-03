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

// bdelta uses two callback functions to supply it with the
// data to be compared, callback functions must keep data 
// incorrupt until two more calls for data
typedef void *(*bdelta_readCallback)(unsigned place, unsigned num);

void *bdelta_init_alg(unsigned f1_size, unsigned f2_size, 
                      bdelta_readCallback f1, bdelta_readCallback f2);
void  bdelta_done_alg(void *instance);

//returns the total number of matches found
unsigned bdelta_pass(void *instance, unsigned blocksize);

void bdelta_getMatch(void *instance, unsigned matchNum,
	unsigned *p1, unsigned *p2, unsigned *num);

int bdelta_getError(void *instance);

const int
	BDELTA_OK			= 0,
	BDELTA_MEM_ERROR	= -1,
	BDELTA_READ_ERROR	= -2;
