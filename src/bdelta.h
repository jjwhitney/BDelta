/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct _BDelta_Instance BDelta_Instance;

typedef int64_t pos;

BDelta_Instance *bdelta_init_alg(pos data1_size, pos data2_size,
				 void *handle1, void *handle2);
void bdelta_done_alg(BDelta_Instance *b);

void bdelta_pass(BDelta_Instance *b, unsigned blockSize, unsigned minMatchSize, pos maxHoleSize, unsigned flags);

void bdelta_swap_inputs(BDelta_Instance *b);
void bdelta_clean_matches(BDelta_Instance *b, unsigned flags);

unsigned bdelta_numMatches(BDelta_Instance *b);

void bdelta_getMatch(BDelta_Instance *b, unsigned matchNum,
	pos *p1, pos *p2, pos *num);

int bdelta_getError(BDelta_Instance *b);
void bdelta_showMatches(BDelta_Instance *b);

// Flags for bdelta_pass()
#define BDELTA_GLOBAL 1
#define BDELTA_SIDES_ORDERED 2

// Flags for bdelta_clean_matches()
#define BDELTA_REMOVE_OVERLAP 1

enum BDELTA_RESULT {
	BDELTA_OK         =  0,
	BDELTA_MEM_ERROR  = -1,
	BDELTA_READ_ERROR = -2
};

#ifdef __cplusplus
}
#endif // __cplusplus
