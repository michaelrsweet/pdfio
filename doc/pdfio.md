Introduction
============

PDFio is a simple C library for reading and writing PDF files.  The primary
goals of pdfio are:

- Read any PDF file with or without encryption or linearization
- Write PDF files without encryption or linearization
- Extract or embed useful metadata (author, creator, page information, etc.)
- "Filter" PDF files, for example to extract a range of pages or to embed fonts
  that are missing from a PDF
- Provide access to objects used for each page

PDFio is *not* concerned with rendering or viewing a PDF file, although a PDF
RIP or viewer could be written using it.

PDFio is Copyright © 2021 by Michael R Sweet and is licensed under the Apache
License Version 2.0 with an (optional) exception to allow linking against
GPL2/LGPL2 software.  See the files "LICENSE" and "NOTICE" for more information.


Requirements
------------

PDFio requires the following to build the software:

- A C99 compiler such as Clang, GCC, or MS Visual C
- A POSIX-compliant `make` program
- ZLIB (<https://www.zlib.net>) 1.0 or higher

IDE files for Xcode (macOS/iOS) and Visual Studio (Windows) are also provided.


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
- `DESTDIR` and `DSTROOT`: specifies a root directory when installing
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

The Visual Studio solution ("pdfio.sln") is provided for Windows developers and
generates both a static library and DLL.


Xcode Project
-------------

There is also an Xcode project ("pdfio.xcodeproj") you can use on macOS which
generates a static library that will be installed under "/usr/local" with:

    sudo xcodebuild install

You can reproduce this with the makefile using:

    sudo make 'COMMONFLAGS="-Os -mmacosx-version-min=10.14 -arch x86_64 -arch arm64"' install


Detecting PDFio
---------------

PDFio can be detected using the `pkg-config` command, for example:

    if pkg-config --exists pdfio; then
        ... 
    fi

In a makefile you can add the necessary compiler and linker options with:

```make
CFLAGS  +=      `pkg-config --cflags pdfio`
LIBS    +=      `pkg-config --libs pdfio`
```

On Windows, you need to link to the `PDFIO.LIB` (static) or `PDFIO1.LIB` (DLL)
libraries and include the "zlib" NuGet package dependency.


Header Files
------------

PDFio provides a primary header file that is always used:

```c
#include <pdfio.h>
```

PDFio also provides helper functions for producing PDF content that are defined
in a separate header file:

```c
#include <pdfio-content.h>
```


API Overview
============

PDFio exposes several types:

- `pdfio_file_t`: A PDF file (for reading or writing)
- `pdfio_array_t`: An array of values
- `pdfio_dict_t`: A dictionary of key/value pairs in a PDF file, object, etc.
- `pdfio_obj_t`: An object in a PDF file
- `pdfio_stream_t`: An object stream


Reading PDF Files
-----------------

You open an existing PDF file using the `pdfioFileOpen` function:

```c
pdfio_file_t *pdf = pdfioFileOpen("myinputfile.pdf", error_cb, error_data);

```

where the three arguments to the function are the filename ("myinputfile.pdf"),
an optional error callback function (`error_cb`), and an optional pointer value
for the error callback function (`error_data`).  The error callback is called
for both errors and warnings and accepts the `pdfio_file_t` pointer, a message
string, and the callback pointer value, for example:

```c
bool
error_cb(pdfio_file_t *pdf, const char *message, void *data)
{
  (void)data; // This callback does not use the data pointer

  fprintf(stderr, "%s: %s\n", pdfioFileGetName(pdf), message);

  // Return false to treat warnings as errors
  return (false);
}
```

The default error callback (`NULL`) does the equivalent of the above.

Each PDF file contains one or more pages.  The `pdfioFileGetNumPages` function
returns the number of pages in the file while the `pdfioFileGetPage` function
gets the specified page in the PDF file:

```c
pdfio_file_t *pdf;   // PDF file
size_t       i;      // Looping var
size_t       count;  // Number of pages
pdfio_obj_t  *page;  // Current page

// Iterate the pages in the PDF file
for (i = 0, count = pdfioFileGetNumPages(pdf); i < count; i ++)
{
  page = pdfioFileGetPage(pdf, i);
  // do something with page
}
```

Each page is represented by a "page tree" object (what `pdfioFileGetPage`
returns) that specifies information about the page and one or more "content"
objects that contain the images, fonts, text, and graphics that appear on the
page.

The `pdfioFileClose` function closes a PDF file and frees all memory that was
used for it:

```c
pdfioFileClose(pdf);
```

Writing PDF Files
-----------------

You create a new PDF file using the `pdfioFileCreate` function:

```c
pdfio_rect_t media_box = { 0.0, 0.0, 612.0, 792.0 };  // US Letter
pdfio_rect_t crop_box = { 36.0, 36.0, 576.0, 756.0 }; // 0.5" margins

pdfio_file_t *pdf = pdfioFileCreate("myoutputfile.pdf", "2.0", &media_box, &crop_box, error_cb, error_data);
```

where the six arguments to the function are the filename ("myoutputfile.pdf"),
PDF version ("2.0"), media box (`media_box`), crop box (`crop_box`), an optional
error callback function (`error_cb`), and an optional pointer value for the
error callback function (`error_data`).

Once the file is created, use the `pdfioFileCreateObj`, `pdfioFileCreatePage`,
and `pdfioPageCopy` functions to create objects and pages in the file.

Finally, the `pdfioFileClose` function writes the PDF cross-reference and
"trailer" information, closes the file, and frees all memory that was used for
it.


PDF Objects
-----------

PDF objects are identified using two numbers - the object number (1 to N) and
the object generation (0 to 65535) that specifies a particular version of an
object.  An object's numbers are returned by the `pdfioObjGetNumber` and
`pdfioObjGetGeneration` functions.  You can find a numbered object using the
`pdfioFileFindObj` function.

Objects contain values (typically dictionaries) and usually an associated data
stream containing images, fonts, ICC profiles, and page content.  PDFio provides several accessor functions to get the value(s) associated with an object:

- `pdfioObjGetArray` returns an object's array value, if any
- `pdfioObjGetDict` returns an object's dictionary value, if any
- `pdfioObjGetLength` returns the length of the data stream, if any
- `pdfioObjGetSubtype` returns the sub-type name of the object, for example
  "Image" for an image object.
- `pdfioObjGetType` returns the type name of the object, for example "XObject"
  for an image object.


PDF Streams
-----------


PDF Content Helper Functions
----------------------------

