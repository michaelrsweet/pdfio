pdfio - PDF Read/Write Library
==============================

![Version](https://img.shields.io/github/v/release/michaelrsweet/pdfio?include_prereleases)
![Apache 2.0](https://img.shields.io/github/license/michaelrsweet/pdfio)
![Build](https://github.com/michaelrsweet/pdfio/workflows/Build/badge.svg)
[![Coverity Scan Status](https://img.shields.io/coverity/scan/22385.svg)](https://scan.coverity.com/projects/michaelrsweet-pdfio)
[![LGTM Grade](https://img.shields.io/lgtm/grade/cpp/github/michaelrsweet/pdfio)](https://lgtm.com/projects/g/michaelrsweet/pdfio/context:cpp)
[![LGTM Alerts](https://img.shields.io/lgtm/alerts/github/michaelrsweet/pdfio)](https://lgtm.com/projects/g/michaelrsweet/pdfio/)

PDFio is a simple C library for reading and writing PDF files.  The primary
goals of PDFio are:

- Read and write any version of PDF file
- Provide access to pages, objects, and streams within a PDF file
- Support reading encrypted PDF files
- Support writing PDF files with digital signatures
- Extract or embed useful metadata (author, creator, page information, etc.)
- "Filter" PDF files, for example to extract a range of pages or to embed fonts
  that are missing from a PDF
- Provide access to objects used for each page

PDFio is *not* concerned with rendering or viewing a PDF file, although a PDF
RIP or viewer could be written using it.


Requirements
------------

PDFio requires the following to build the software:

- A C99 compiler such as Clang, GCC, or MS Visual C
- A POSIX-compliant `make` program
- ZLIB (<https://www.zlib.net>) 1.0 or higher

IDE files for Xcode (macOS/iOS) and Visual Studio (Windows) are also provided.


Documentation
-------------

> Note: Documentation is under active development...

See the man page (`pdfio.3`), frequently ask questions (`FAQ.md`), and full HTML
documentation (`pdfio.html`) for information on using PDFio.


Installing pdfio
----------------

PDFio comes with a portable makefile that will work on any POSIX-compliant
system with ZLIB installed.  To make it, run:

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
"${prefix}/share/doc/pdfio".

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


Visual Studio Project
---------------------

> Note: I haven't yet added this...

The Visual Studio solution ("pdfio.sln") is provided for Windows developers and
generates both a static library and DLL.


Xcode Project
-------------

There is also an Xcode project ("pdfio.xcodeproj") you can use on macOS which
generates a static library that will be installed under "/usr/local" with:

    sudo xcodebuild install

You can reproduce this with the makefile using:

    sudo make 'COMMONFLAGS="-Os -mmacosx-version-min=10.14 -arch x86_64 -arch arm64"' install


Legal Stuff
-----------

PDFio is Copyright © 2021 by Michael R Sweet.

This software is licensed under the Apache License Version 2.0 with an
(optional) exception to allow linking against GPL2/LGPL2 software.  See the
files "LICENSE" and "NOTICE" for more information.
