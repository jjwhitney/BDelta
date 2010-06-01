// "Hello BDelta!"

#include "bdelta.h"
#include <string.h>
#include <stdio.h>

const int smallestMatch = 8;
char *a = "abcdefghijklmnopqrstuvwxyz", *b = "abcdefghijklmnopqrstuvwxyz";
void *a_read(unsigned place, unsigned num) {
	return (char*)(a + place);
}

void *b_read(unsigned place, unsigned num) {
	return (char*)(b + place);
}
main() {
	void *bi = bdelta_init_alg(strlen(a), strlen(b), a_read, b_read);
	int nummatches;
	for (int i = 64; i >= smallestMatch; i/=2)
		nummatches = bdelta_pass(bi, i);
	for (int i = 0; i < nummatches; ++i) {
		unsigned p1, p2, num;
		bdelta_getMatch(bi, i, &p1, &p2, &num);
		printf("Got match\n");
	}

}

