DESTDIR=
PREFIX   ?= /usr
BINDIR   ?= $(PREFIX)/bin
LIBDIR   ?= ${PREFIX}/lib
CXXFLAGS += -O2

ifeq ($(shell uname -s),Darwin)
	SHAREDLIB := libbdelta.dylib
else
	SHAREDLIB := libbdelta.so
endif

ALL_TARGETS = bpatch $(SHAREDLIB) bdelta

all: $(ALL_TARGETS)

libbdelta.a: libbdelta.cpp compatibility.h checksum.h file.h
	$(CXX) -c $(CXXFLAGS) $< -o $<.o
	$(AR) rs $@ $<.o

libbdelta.so: libbdelta.cpp compatibility.h checksum.h file.h
	$(CXX) -shared -fPIC $(CXXFLAGS) $(LDFLAGS) $< -o $@

libbdelta.dylib: libbdelta.cpp compatibility.h checksum.h file.h
	$(CXX) -dynamiclib $(CXXFLAGS) $< -o $@

bdelta: bdelta.cpp bdelta.h compatibility.h file.h $(SHAREDLIB)
	$(CXX) $< -o $@ $(CXXFLAGS) $(LDFLAGS) -L. -lbdelta

bpatch: bpatch.cpp compatibility.h bdelta.h file.h

% : %.cpp
	$(CXX) $< -o $@ $(CXXFLAGS) $(LDFLAGS)

install: libbdelta.so bdelta bpatch
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(LIBDIR)
	install -m 755 libbdelta.so $(DESTDIR)$(LIBDIR)
	install -m 755 bdelta $(DESTDIR)$(BINDIR)
	install -m 755 bpatch $(DESTDIR)$(BINDIR)

clean:
	-rm $(ALL_TARGETS)

.PHONY: clean
