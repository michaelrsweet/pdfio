pdfio - PDF Read/Write Library
==============================

![Version](https://img.shields.io/github/v/release/michaelrsweet/pdfio?include_prereleases)
![Apache 2.0](https://img.shields.io/github/license/michaelrsweet/pdfio)
[![Build Status](https://img.shields.io/github/actions/workflow/status/michaelrsweet/pdfio/build.yml)](https://github.com/michaelrsweet/pdfio/actions/workflows/build.yml)
[![Coverity Scan Status](https://img.shields.io/coverity/scan/23194.svg)](https://scan.coverity.com/projects/michaelrsweet-pdfio)

PDFio is a simple C library for reading and writing PDF files.  The primary
goals of PDFio are:

- Read and write any version of PDF file
- Provide access to pages, objects, and streams within a PDF file
- Support reading and writing of encrypted PDF files
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
- A POSIX-compliant `sh` program
- libpng (<https://www.libpng.org/>) 1.6 or later for full PNG image support
  (optional)
- libwebp (<https://developers.google.com/speed/webp>) 1.0 or later for WebP
  image support (optional)
- ZLIB (<https://www.zlib.net/>) 1.1 or later

IDE files for Xcode (macOS/iOS) and Visual Studio (Windows) are also provided.


Documentation
-------------

See the man page (`pdfio.3`) and full HTML documentation (`pdfio.html`) for
information on using PDFio.


Installing PDFio
----------------

If you cloned this repository from GitHub, you must initialize the submodules before building:

    git submodule update --init --recursive

PDFio uses a configure script on Unix systems to generate a makefile:

    ./configure

If you want a shared library, run:

    ./configure --enable-shared

The default installation location is "/usr/local".  Pass the `--prefix` option
to make to install it to another location:

    ./configure --prefix=/some/other/directory

Once configured, run the following to make the library:

    make all

To test it, run:

    make test

To install it, run:

    sudo make install


Visual Studio Project
---------------------

The Visual Studio solution ("pdfio.sln") is provided for Windows developers and
generates the PDFIO1 DLL.  You can also use NuGet to install the `pdfio_native`
package.


Xcode Project
-------------

There is also an Xcode project ("pdfio.xcodeproj") you can use on macOS which
generates a static library that will be installed under "/usr/local" with:

    sudo xcodebuild install


Legal Stuff
-----------

PDFio is Copyright © 2021-2026 by Michael R Sweet.

This software is licensed under the Apache License Version 2.0 with an
(optional) exception to allow linking against GPL2/LGPL2 software.  See the
files "LICENSE" and "NOTICE" for more information.
