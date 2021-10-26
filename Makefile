#
# Makefile for PDFio.
#
# Copyright © 2021 by Michael R Sweet.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

# POSIX makefile
.POSIX:

# Variables...
AR		=	ar
ARFLAGS		=	cr
CC		=	cc
CFLAGS		=
CODESIGN_IDENTITY =	Developer ID
#COMMONFLAGS	=	-Os -g
COMMONFLAGS	=	-O0 -g
CPPFLAGS	=	'-DPDFIO_VERSION="$(VERSION)"'
DESTDIR		=	$(DSTROOT)
DSO		=	cc
DSOFLAGS	=
DSONAME		=
LDFLAGS		=
LIBS		=	-lm -lz
RANLIB		=	ranlib
VERSION		=	1.0.0
prefix		=	/usr/local


# Base rules
.SUFFIXES:	.c .h .o
.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(COMMONFLAGS) -c $<


# Files
PUBHEADERS	=	\
			pdfio.h \
			pdfio-content.h
PUBOBJS		=	\
			pdfio-aes.o \
			pdfio-array.o \
			pdfio-common.o \
			pdfio-content.o \
			pdfio-crypto.o \
			pdfio-dict.o \
			pdfio-file.o \
			pdfio-md5.o \
			pdfio-object.o \
			pdfio-page.o \
			pdfio-rc4.o \
			pdfio-sha256.o \
			pdfio-stream.o \
			pdfio-string.o \
			pdfio-token.o \
			pdfio-value.o
LIBOBJS		=	\
			$(PUBOBJS) \
			ttf.o
OBJS		=	\
			$(LIBOBJS) \
			testpdfio.o
TARGETS		=	\
			$(DSONAME) \
			libpdfio.a \
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


# Clean everything
clean:
	rm -f $(TARGETS) $(OBJS)


# Install everything
install:	$(TARGETS)
	-mkdir -p $(DESTDIR)$(prefix)/include
	cp $(PUBHEADERS) $(DESTDIR)$(prefix)/include
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
	-mkdir -p $(DESTDIR)$(prefix)/lib/pkgconfig
	echo 'prefix="$(prefix)"' >$(DESTDIR)$(prefix)/lib/pkgconfig/pdfio.pc
	echo 'Version: $(VERSION)' >>$(DESTDIR)$(prefix)/lib/pkgconfig/pdfio.pc
	cat pdfio.pc.in >>$(DESTDIR)$(prefix)/lib/pkgconfig/pdfio.pc
	-mkdir -p $(DESTDIR)$(prefix)/share/doc/pdfio
	cp doc/pdfio.html doc/pdfio-512.png LICENSE NOTICE $(DESTDIR)$(prefix)/share/doc/pdfio
	-mkdir -p $(DESTDIR)$(prefix)/share/man/man3
	cp doc/pdfio.3 $(DESTDIR)$(prefix)/share/man/man3

install-shared:
	if test `uname` = Darwin; then \
		$(MAKE) DSONAME="libpdfio.1.dylib" -$(MAKEFLAGS) install; \
	else
		$(MAKE) DSONAME="libpdfio.so.1" -$(MAKEFLAGS) install; \
	fi


# Test everything
test:	testpdfio
	./testpdfio

valgrind:	testpdfio
	valgrind --leak-check=full ./testpdfio


# pdfio library
libpdfio.a:		$(LIBOBJS)
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@

libpdfio.so.1:		$(LIBOBJS)
	$(CC) $(DSOFLAGS) $(COMMONFLAGS) -shared -o $@ -Wl,-soname,$@ $(LIBOBJS) $(LIBS)

libpdfio.1.dylib:	$(LIBOBJS)
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


# pdfio test program
testpdfio:		testpdfio.o libpdfio.a
	$(CC) $(LDFLAGS) $(COMMONFLAGS) -o $@ testpdfio.o libpdfio.a $(LIBS)


# Dependencies
$(OBJS):		pdfio.h Makefile
$(LIBOBJS):		pdfio-private.h
pdfio-content.o:	pdfio-content.h ttf.h
ttf.o:			ttf.h

# Make documentation using Codedoc <https://www.msweet.org/codedoc>
DOCFLAGS	=	\
			--author "Michael R Sweet" \
			--copyright "Copyright (c) 2021 by Michael R Sweet" \
			--docversion $(VERSION)

.PHONY: doc
doc:
	codedoc $(DOCFLAGS) --title "PDFio Programming Manual v$(VERSION)" $(PUBHEADERS) $(PUBOBJS:.o=.c) --body doc/pdfio.md --coverimage doc/pdfio-512.png pdfio.xml >doc/pdfio.html
	codedoc $(DOCFLAGS) --title "PDFio Programming Manual v$(VERSION)" --body doc/pdfio.md --coverimage doc/pdfio-epub.png pdfio.xml --epub doc/pdfio.epub
	codedoc $(DOCFLAGS) --title "pdf read/write library" --man pdfio --section 3 --body doc/pdfio.md pdfio.xml >doc/pdfio.3
	rm -f pdfio.xml


# Analyze code with the Clang static analyzer <https://clang-analyzer.llvm.org>
clang:
	clang $(CPPFLAGS) --analyze $(OBJS:.o=.c) 2>clang.log
	rm -rf $(OBJS:.o=.plist)
	test -s clang.log && (echo "$(GHA_ERROR)Clang detected issues."; echo ""; cat clang.log; exit 1) || exit 0


# Analyze code using Cppcheck <http://cppcheck.sourceforge.net>
cppcheck:
	cppcheck $(CPPFLAGS) --template=gcc --addon=cert.py --suppressions-list=.cppcheck $(OBJS:.o=.c) 2>cppcheck.log
	test -s cppcheck.log && (echo "$(GHA_ERROR)Cppcheck detected issues."; echo ""; cat cppcheck.log; exit 1) || exit 0
