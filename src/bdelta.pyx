cdef extern from "bdelta.h":
    ctypedef struct BDelta_Instance:
        pass

    ctypedef void *const_void_ptr "const void*"
    ctypedef void *(*bdelta_readCallback)(void *handle, void *buf, unsigned place, unsigned num)
    BDelta_Instance *bdelta_init_alg(unsigned data1_size, unsigned data2_size,
        bdelta_readCallback cb, void *handle1, void *handle2,
        unsigned tokenSize)
    void bdelta_done_alg(BDelta_Instance *b)
    
    void bdelta_pass(BDelta_Instance *b, unsigned blockSize, unsigned minMatchSize, unsigned maxHoleSize, unsigned flags)

    void bdelta_swap_inputs(BDelta_Instance *b)
    void bdelta_clean_matches(BDelta_Instance *b, unsigned flags)

    unsigned bdelta_numMatches(BDelta_Instance *b)

    void bdelta_getMatch(BDelta_Instance *b, unsigned matchNum,
	    unsigned *p1, unsigned *p2, unsigned *num)

    int bdelta_getError(BDelta_Instance *b)
    void bdelta_showMatches(BDelta_Instance *b)

    cdef enum PassFlags:
        BDELTA_GLOBAL,
        BDELTA_SIDES_ORDERED
    cdef enum CleanFlags:
        BDELTA_REMOVE_OVERLAP

cdef const_void_ptr readCallback(void *handle, void *buf, unsigned place, unsigned num):
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

    def b_pass(self, blockSize, minMatchSize, maxHoleSize, globalScope = False, sidesOrdered = False):
        bdelta_pass(self._b, blockSize, minMatchSize, maxHoleSize,
        	(BDELTA_GLOBAL if globalScope else 0) | (BDELTA_SIDES_ORDERED if sidesOrdered else 0))

    def matches(self):
        cdef unsigned p1, p2, num
        for i in xrange(bdelta_numMatches(self._b)):
            bdelta_getMatch(self._b, i, &p1, &p2, &num)
            yield (int(p1), int(p2), int(num))