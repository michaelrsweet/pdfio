#
# Makefile for pdfio.
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
COMMONFLAGS	=	-Os -g
CPPFLAGS	=
DESTDIR		=	$(DSTROOT)
DSO		=	cc
DSOFLAGS	=	
DSONAME		=
LDFLAGS		=
LIBS		=	-lz
RANLIB		=	ranlib
VERSION		=	1.0
prefix		=	/usr/local


# Base rules
.SUFFIXES:	.c .h .o
.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(COMMONFLAGS) -c $<


# Files
LIBOBJS		=	\
			pdfio-array.o \
			pdfio-common.o \
			pdfio-dict.o \
			pdfio-file.o \
			pdfio-object.o \
			pdfio-page.o \
			pdfio-stream.o \
			pdfio-string.o \
			pdfio-token.o \
			pdfio-value.o
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
	else
		$(MAKE) DSONAME="libpdfio.so.1" -$(MAKEFLAGS) all; \
	fi

debug:
	$(MAKE) -$(MAKEFLAGS) COMMONFLAGS="$(COMMONFLAGS) -DDEBUG=1" clean all


# Clean everything
clean:
	rm -f $(TARGETS) $(OBJS)


# Install everything
install:	$(TARGETS)
	-mkdir -p $(DESTDIR)$(prefix)/include
	cp pdfio.h $(DESTDIR)$(prefix)/include
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
	echo '"prefix="$(prefix)"' >>$(DESTDIR)$(prefix)/lib/pkgconfig/pdfio.pc
	cat pdfio.pc.in >$(DESTDIR)$(prefix)/lib/pkgconfig/pdfio.pc
	-mkdir -p $(DESTDIR)$(prefix)/share/doc/pdfio
	cp pdfio.html LICENSE NOTICE $(DESTDIR)$(prefix)/share/doc/pdfio
	-mkdir -p $(DESTDIR)$(prefix)/share/man/man3
	cp pdfio.3 LICENSE NOTICE $(DESTDIR)$(prefix)/share/man/man3

install-shared:
	if test `uname` = Darwin; then \
		$(MAKE) DSONAME="libpdfio.1.dylib" -$(MAKEFLAGS) install; \
	else
		$(MAKE) DSONAME="libpdfio.so.1" -$(MAKEFLAGS) install; \
	fi


# Test everything
test:	testpdfio
	./testpdfio


# pdfio library
libpdfio.a:		$(LIBOBJS)
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@

libpdfio.so.1:		$(LIBOBJS)
	$(CC) $(DSOFLAGS) $(COMMONFLAGS) -shared -o $@ -Wl,soname,$@ $(LIBOBJS) $(LIBS)

libpdfio.1.dylib:	$(LIBOBJS)
	$(CC) $(DSOFLAGS) $(COMMONFLAGS) -dynamiclib -o $@ -install_name $(prefix)/lib/$@ -current_version $(VERSION) -compatibility_version 1.0 $(LIBOBJS) $(LIBS)


# pdfio test program
testpdfio:	testpdfio.o libpdfio.a
	$(CC) $(LDFLAGS) $(COMMONFLAGS) -o $@ testpdfio.o libpdfio.a $(LIBS)


# Dependencies
$(OBJS):	pdfio.h Makefile
$(LIBOBJS):	pdfio-private.h


# Make documentation using Codedoc <https://www.msweet.org/codedoc>
DOCFLAGS	=	\
			--author "Michael R Sweet" \
			--copyright "Copyright (c) 2021 by Michael R Sweet" \
			--docversion $(VERSION)

doc:
	codedoc $(DOCFLAGS) --title "pdfio Programming Manual" pdfio.h $(LIBOBJS:.o=.c) --body pdfio.md pdfio.xml >pdfio.html
	codedoc $(DOCFLAGS) --title "pdf read/write library" --man pdfio --section 3 --body pdfio.md pdfio.xml >pdfio.3
	rm -f pdfio.xml
