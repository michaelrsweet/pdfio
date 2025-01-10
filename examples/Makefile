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
#CFLAGS		=	-g -fsanitize=address $(CPPFLAGS)
CPPFLAGS	=	-I.. -I/usr/local/include
LIBS		=	-L.. -L/usr/local/lib -lpdfio -lz -lm


# Targets
TARGETS		=	\
			code128 \
			image2pdf \
			md2pdf \
			pdf2text \
			pdfioinfo


# Make everything
all:	$(TARGETS)


# Clean everything
clean:
	rm -f $(TARGETS)


# code128
code128:	code128.c
	$(CC) $(CFLAGS) -o $@ code128.c $(LIBS)


# image2pdf
image2pdf:	image2pdf.c
	$(CC) $(CFLAGS) -o $@ image2pdf.c $(LIBS)


# md2pdf
md2pdf:		md2pdf.c mmd.c mmd.h
	$(CC) $(CFLAGS) -o $@ md2pdf.c mmd.c $(LIBS)


# pdfio text extraction (demo, doesn't handle a lot of things yet)
pdf2text:	pdf2text.c
	$(CC) $(CFLAGS) -o $@ pdf2text.c $(LIBS)


# pdfioinfo
pdfioinfo:	pdfioinfo.c
	$(CC) $(CFLAGS) -o $@ pdfioinfo.c $(LIBS)


# Common dependencies...
$(TARGETS):	Makefile ../pdfio.h ../pdfio-content.h
