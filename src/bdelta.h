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

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Callback function must return a pointer to the data requested.
// A "fill and forget" buffer is provided, but can be ignored, so
// long as the data persists throughout the life of bdelta_pass().
typedef void *(*bdelta_readCallback)(void *handle, void *buf, unsigned place, unsigned num);

void *bdelta_init_alg(unsigned data1_size, unsigned data2_size,
		bdelta_readCallback cb, void *handle1, void *handle2,
		unsigned tokenSize);
void  bdelta_done_alg(void *instance);

void bdelta_pass(void *instance, unsigned blocksize);

void bdelta_pass_local(void *instance, unsigned blocksize);

void bdelta_swap_inputs(void *instance);
void bdelta_clean_matches(void *instance);

unsigned bdelta_nummatches(void *instance);

void bdelta_getMatch(void *instance, unsigned matchNum,
	unsigned *p1, unsigned *p2, unsigned *num);

int bdelta_getError(void *instance);

enum BDELTA_RESULT {
	BDELTA_OK         =  0,
	BDELTA_MEM_ERROR  = -1,
	BDELTA_READ_ERROR = -2
};

#ifdef __cplusplus
}
#endif // __cplusplus
