cdef extern from "bdelta.h":
    ctypedef struct BDelta_Instance:
        pass

    ctypedef void *(*bdelta_readCallback)(void *handle, void *buf, unsigned place, unsigned num)
    BDelta_Instance *bdelta_init_alg(unsigned data1_size, unsigned data2_size,
        bdelta_readCallback cb, void *handle1, void *handle2,
        unsigned tokenSize)
    void bdelta_done_alg(BDelta_Instance *b)
    
    void bdelta_pass(BDelta_Instance *b, unsigned blockSize, unsigned minMatchSize, bint local)

    void bdelta_swap_inputs(BDelta_Instance *b)
    void bdelta_clean_matches(BDelta_Instance *b, int removeOverlap)

    unsigned bdelta_numMatches(BDelta_Instance *b)

    void bdelta_getMatch(BDelta_Instance *b, unsigned matchNum,
	    unsigned *p1, unsigned *p2, unsigned *num)

    int bdelta_getError(BDelta_Instance *b)
    void bdelta_showMatches(BDelta_Instance *b)

cdef void *readCallback(void *handle, void *buf, unsigned place, unsigned num):
    cdef char *str = <bytes>handle
    return str + ((place + 1) * 2);

cdef class BDelta:
    cdef BDelta_Instance *_b
    cdef bytes str1, str2

    def __cinit__(self, str1, str2):
        self.str1 = str1.encode('UTF-16')
        self.str2 = str2.encode('UTF-16')
        self._b = bdelta_init_alg(len(str1), len(str2), readCallback, <void*>self.str1, <void*>self.str2, 2)

    def __dealloc__(self):
        self.str1 = None
        self.str2 = None
        bdelta_done_alg(self._b)

    def b_pass(self, blockSize, minMatchSize, local):
        bdelta_pass(self._b, blockSize, minMatchSize, local)

    def matches(self):
        cdef unsigned p1, p2, num
        for i in xrange(bdelta_numMatches(self._b)):
            bdelta_getMatch(self._b, i, &p1, &p2, &num)
            yield (int(p1), int(p2), int(num))