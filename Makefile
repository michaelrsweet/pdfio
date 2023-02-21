#
# Makefile for PDFio.
#
# Copyright © 2021-2023 by Michael R Sweet.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

# POSIX makefile
.POSIX:

# Build silently
.SILENT:

# Variables
AR		=	ar
ARFLAGS		=	cr
CC		=	cc
CFLAGS		=	'-DPDFIO_VERSION="$(VERSION)"'
CODESIGN_IDENTITY =	Developer ID
COMMONFLAGS	=	-Os -g -Isrc
#COMMONFLAGS	=	-O0 -g -fsanitize=address
DESTDIR		=	$(DSTROOT)
DSO		=	cc
DSOFLAGS	=
DSONAME		=
LDFLAGS		=
LIBS		=	-lm -lz
RANLIB		=	ranlib
VERSION		=	1.1.0
prefix		=	/usr/local


# Base rules
.SUFFIXES:	.c .h .o
.c.o:
	echo Compiling $<...
	$(CC) $(CFLAGS) $(COMMONFLAGS) -c $< -o $@


# Files
PUBHEADERS	=	\
			src/pdfio.h \
			src/pdfio-content.h
PUBOBJS		=	\
			src/pdfio-aes.o \
			src/pdfio-array.o \
			src/pdfio-common.o \
			src/pdfio-content.o \
			src/pdfio-crypto.o \
			src/pdfio-dict.o \
			src/pdfio-file.o \
			src/pdfio-md5.o \
			src/pdfio-object.o \
			src/pdfio-page.o \
			src/pdfio-rc4.o \
			src/pdfio-sha256.o \
			src/pdfio-stream.o \
			src/pdfio-string.o \
			src/pdfio-token.o \
			src/pdfio-value.o
LIBOBJS		=	\
			$(PUBOBJS) \
			src/ttf.o
OBJS		=	\
			$(LIBOBJS) \
			src/pdfiototext.o \
			src/testpdfio.o
TARGETS		=	\
			$(DSONAME) \
			libpdfio.a \
			pdfiototext \
			testpdfio


# Make everything
all:		$(TARGETS)

all-shared:
	if test `uname` = Darwin; then \
		$(MAKE) DSONAME="libpdfio.1.dylib" -$(MAKEFLAGS) all; \
	else \
		$(MAKE) COMMONFLAGS="-g -Os -fPIC" DSONAME="libpdfio.so.1" -$(MAKEFLAGS) all; \
	fi

debug:
	$(MAKE) -$(MAKEFLAGS) COMMONFLAGS="-g -fsanitize=address -DDEBUG=1" clean all

macos:
	$(MAKE) -$(MAKEFLAGS) COMMONFLAGS="-Os -mmacosx-version-min=10.14 -arch x86_64 -arch arm64" clean all


# Clean everything
clean:
	rm -f $(TARGETS) $(OBJS)


# Install everything
install:	$(TARGETS)
	echo Installing header files to $(DESTDIR)$(prefix)/include...
	-mkdir -p $(DESTDIR)$(prefix)/include
	cp $(PUBHEADERS) $(DESTDIR)$(prefix)/include
	echo Installing library files to $(DESTDIR)$(prefix)/lib...
	-mkdir -p $(DESTDIR)$(prefix)/lib
	cp libpdfio.a $(DESTDIR)$(prefix)/lib
	$(RANLIB) $(DESTDIR)$(prefix)/lib/libpdfio.a
	if test "x$(DSONAME)" = xlibpdfio.so.1; then \
		cp $(DSONAME) $(DESTDIR)$(prefix)/lib; \
		ln -sf libpdfio.so.1 $(DESTDIR)$(prefix)/lib/libpdfio.so; \
	elif test "x$(DSONAME)" = xlibpdfio.1.dylib; then \
		cp $(DSONAME) $(DESTDIR)$(prefix)/lib; \
		codesign -s "$(CODESIGN_IDENTITY)" -o runtime --timestamp $(DESTDIR)$(prefix)/lib/libpdfio.1.dylib; \
		ln -sf libpdfio.1.dylib $(DESTDIR)$(prefix)/lib/libpdfio.dylib; \
	fi
	echo Installing pkg-config files to $(DESTDIR)$(prefix)/lib/pkgconfig...
	-mkdir -p $(DESTDIR)$(prefix)/lib/pkgconfig
	echo 'prefix="$(prefix)"' >$(DESTDIR)$(prefix)/lib/pkgconfig/pdfio.pc
	echo 'Version: $(VERSION)' >>$(DESTDIR)$(prefix)/lib/pkgconfig/pdfio.pc
	cat pdfio.pc.in >>$(DESTDIR)$(prefix)/lib/pkgconfig/pdfio.pc
	echo Installing documentation to $(DESTDIR)$(prefix)/share/doc/pdfio...
	-mkdir -p $(DESTDIR)$(prefix)/share/doc/pdfio
	cp doc/pdfio.html doc/pdfio-512.png LICENSE NOTICE $(DESTDIR)$(prefix)/share/doc/pdfio
	echo Installing man page to $(DESTDIR)$(prefix)/share/man/man3...
	-mkdir -p $(DESTDIR)$(prefix)/share/man/man3
	cp doc/pdfio.3 $(DESTDIR)$(prefix)/share/man/man3

install-shared:
	if test `uname` = Darwin; then \
		$(MAKE) DSONAME="libpdfio.1.dylib" -$(MAKEFLAGS) install; \
	else \
		$(MAKE) DSONAME="libpdfio.so.1" -$(MAKEFLAGS) install; \
	fi


# Test everything
test: testpdfio
	./testpdfio

valgrind: testpdfio
	valgrind --leak-check=full ./testpdfio


# pdfio library
libpdfio.a: $(LIBOBJS)
	echo Archiving $@...
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@

libpdfio.so.1: $(LIBOBJS)
	echo Linking $@...
	$(CC) $(DSOFLAGS) $(COMMONFLAGS) -shared -o $@ -Wl,-soname,$@ $(LIBOBJS) $(LIBS)

libpdfio.1.dylib: $(LIBOBJS)
	echo Linking $@...
	$(CC) $(DSOFLAGS) $(COMMONFLAGS) -dynamiclib -o $@ -install_name $(prefix)/lib/$@ -current_version $(VERSION) -compatibility_version 1.0 $(LIBOBJS) $(LIBS)


# pdfio1.def (Windows DLL exports file...)
#
# I'd love to use __declspec(dllexport) but MS puts it before the function
# declaration instead of after like everyone else, and it breaks Codedoc and
# other tools I rely on...
pdfio1.def: $(LIBOBJS) Makefile
	echo Generating $@...
	echo "LIBRARY pdfio1" >$@
	echo "VERSION 1.0" >>$@
	echo "EXPORTS" >>$@
	nm $(LIBOBJS) 2>/dev/null | grep "T _" | awk '{print $$3}' | \
		grep -v '^_ttf' | sed -e '1,$$s/^_//' | sort >>$@


# pdfio text extraction (demo, doesn't handle a lot of things yet)
pdfiototext: src/pdfiototext.o libpdfio.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(COMMONFLAGS) -o $@ $^ $(LIBS)


# pdfio test program
testpdfio: src/testpdfio.o libpdfio.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(COMMONFLAGS) -o $@ $^ $(LIBS)


# Dependencies
$(OBJS): src/pdfio.h src/pdfio-private.h Makefile
src/pdfio-content.o: src/pdfio-content.h src/ttf.h
src/ttf.o: src/ttf.h

# Make documentation using Codedoc <https://www.msweet.org/codedoc>
DOCFLAGS	=	\
			--author "Michael R Sweet" \
			--copyright "Copyright (c) 2021-2022 by Michael R Sweet" \
			--docversion $(VERSION)

.PHONY: doc
doc:
	echo Generating documentation...
	codedoc $(DOCFLAGS) --title "PDFio Programming Manual v$(VERSION)" $(PUBHEADERS) $(PUBOBJS:.o=.c) --body doc/pdfio.md --coverimage doc/pdfio-512.png pdfio.xml >doc/pdfio.html
	codedoc $(DOCFLAGS) --title "PDFio Programming Manual v$(VERSION)" --body doc/pdfio.md --coverimage doc/pdfio-epub.png pdfio.xml --epub doc/pdfio.epub
	codedoc $(DOCFLAGS) --title "pdf read/write library" --man pdfio --section 3 --body doc/pdfio.md pdfio.xml >doc/pdfio.3
	rm -f pdfio.xml


# Fuzz-test the library <>
.PHONY: afl
afl:
	$(MAKE) -$(MAKEFLAGS) CC="afl-clang-fast" COMMONFLAGS="-g" clean all
	test afl-output || rm -rf afl-output
	afl-fuzz -x afl-pdf.dict -i afl-input -o afl-output -V 600 -e pdf -t 5000 ./testpdfio @@


# Analyze code with the Clang static analyzer <https://clang-analyzer.llvm.org>
clang:
	clang $(CPPFLAGS) --analyze $(OBJS:.o=.c) 2>clang.log
	rm -rf $(OBJS:.o=.plist)
	test -s clang.log && (echo "$(GHA_ERROR)Clang detected issues."; echo ""; cat clang.log; exit 1) || exit 0


# Analyze code using Cppcheck <http://cppcheck.sourceforge.net>
cppcheck:
	cppcheck $(CPPFLAGS) --template=gcc --addon=cert.py --suppressions-list=.cppcheck $(OBJS:.o=.c) 2>cppcheck.log
	test -s cppcheck.log && (echo "$(GHA_ERROR)Cppcheck detected issues."; echo ""; cat cppcheck.log; exit 1) || exit 0
