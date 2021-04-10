pdfio - PDF Read/Write Library
==============================

pdfio is a simple C library for reading and writing PDF files.  The primary
goals of pdfio are:

- Read any PDF file with or without encryption or linearization
- Write PDF files without encryption or linearization
- Extract or embed useful metadata (author, creator, page information, etc.)
- "Filter" PDF files, for example to extract a range of pages or to embed fonts
  that are missing from a PDF
- Provide access to objects used for each page

pdfio is *not* concerned with rendering or viewing a PDF file, although a PDF
RIP or viewer could be written using it.


Installing pdfio
----------------

pdfio comes with a portable makefile that will work on any POSIX-compliant
system with zlib installed.  To make it, run:

    make all

To test it, run:

    make test

To install it, run:

    make install

If you want a shared library, run:

    make all-shared
    make install-shared

The default installation location is "/usr/local".  Pass the `prefix` variable
to make to install it to another location:

    make install prefix=/some/other/directory

The makefile installs the pdfio header to "${prefix}/include", the library to
"${prefix}/lib", the `pkg-config` file to "${prefix}/lib/pkgconfig", the man
page to "${prefix}/share/man/man3", and the documentation to
"${prefix}/share/doc".

The makefile supports the following variables that can be specified in the make
command or as environment variables:

- `AR`: the library archiver (default "ar")
- `ARFLAGS`: options for the library archiver (default "cr")
- `CC`: the C compiler (default "cc")
- `CFLAGS`: options for the C compiler (default "")
- `CODESIGN_IDENTITY`: the identity to use when code signing the shared library
  on macOS (default "Developer ID")
- `COMMONFLAGS`: options for the C compiler and linker (typically architecture
  and optimization options, default is "-Os -g")
- `CPPFLAGS`: options for the C preprocessor (default "")
- `DESTDIR" and "DSTROOT`: specifies a root directory when installing
  (default is "", specify only one)
- `DSOFLAGS`: options for the C compiler when linking the shared library
  (default "")
- `LDFLAGS`: options for the C compiler when linking the test programs
  (default "")
- `LIBS`: library options when linking the test programs (default "-lz")
- `RANLIB`: program that generates a table-of-contents in a library
  (default "ranlib")
- `prefix`: specifies the installation directory (default "/usr/local")


Legal Stuff
-----------

pdfio is Copyright © 2021 by Michael R Sweet.

This software is licensed under the Apache License Version 2.0 with an
(optional) exception to allow linking against GPL2/LGPL2 software.  See the
files "LICENSE" and "NOTICE" for more information.
