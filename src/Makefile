DESTDIR=
PREFIX=/usr
BINDIR=$(PREFIX)/bin
LIBDIR=${PREFIX}/lib
CXXFLAGS=-O2

all: bpatch libbdelta.so bdelta
libbdelta.so: libbdelta.cpp container.h checksum.h file.h
	${CC} -shared -fPIC ${CXXFLAGS} $< -o $@
bdelta: bdelta.cpp container.h file.h
	${CC} $< -o $@ ${CXXFLAGS} -L. -lbdelta -lstdc++
bpatch: bpatch.cpp file.h
% : %.cpp
	${CC} $< -o $@ ${CXXFLAGS} -lstdc++
install: libbdelta.so bdelta bpatch
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 755 libbdelta.so $(DESTDIR)$(LIBDIR)
	install -m 755 bdelta $(DESTDIR)$(BINDIR)
	install -m 755 bpatch $(DESTDIR)$(BINDIR)
