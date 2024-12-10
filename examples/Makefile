#
# Makefile for PDFio examples.
#
# Copyright © 2024 by Michael R Sweet.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

# POSIX makefile
.POSIX:


# Common options
CFLAGS		=	-g $(CPPFLAGS)
CPPFLAGS	=	-I..
LIBS		=	-L.. -lpdfio -lz


# Targets
TARGETS		=	\
			code128 \
			md2pdf


# Make everything
all:	$(TARGETS)


# Clean everything
clean:
	rm -f $(TARGETS)


# code128
code128:	code128.c
	$(CC) $(CFLAGS) -o $@ code128.c $(LIBS)


# md2pdf
md2pdf:		md2pdf.c mmd.c mmd.h
	$(CC) $(CFLAGS) -o $@ md2pdf.c mmd.c $(LIBS)


# Common dependencies...
$(TARGETS):	Makefile ../pdfio.h ../pdfio-content.h
