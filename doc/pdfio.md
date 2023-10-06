Introduction
============

PDFio is a simple C library for reading and writing PDF files.  The primary
goals of pdfio are:

- Read and write any version of PDF file
- Provide access to pages, objects, and streams within a PDF file
- Support reading and writing of encrypted PDF files
- Extract or embed useful metadata (author, creator, page information, etc.)
- "Filter" PDF files, for example to extract a range of pages or to embed fonts
  that are missing from a PDF
- Provide access to objects used for each page

PDFio is *not* concerned with rendering or viewing a PDF file, although a PDF
RIP or viewer could be written using it.

PDFio is Copyright © 2021-2023 by Michael R Sweet and is licensed under the
Apache License Version 2.0 with an (optional) exception to allow linking against
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

    sudo make macos install


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

On Windows, you need to link to the `PDFIO1.LIB` (DLL) library and include the
`zlib_native` NuGet package dependency.  You can also use the published
`pdfio_native` NuGet package.


Header Files
------------

PDFio provides a primary header file that is always used:

```c
#include <pdfio.h>
```

PDFio also provides [PDF content helper functions](@) for producing PDF content
that are defined in a separate header file:

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

You open an existing PDF file using the [`pdfioFileOpen`](@@) function:

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

Each PDF file contains one or more pages.  The [`pdfioFileGetNumPages`](@@)
function returns the number of pages in the file while the
[`pdfioFileGetPage`](@@) function gets the specified page in the PDF file:

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

Each page is represented by a "page tree" object (what [`pdfioFileGetPage`](@@)
returns) that specifies information about the page and one or more "content"
objects that contain the images, fonts, text, and graphics that appear on the
page.  Use the [`pdfioPageGetNumStreams`](@@) and [`pdfioPageOpenStream`](@@)
functions to access the content streams for each page.

The [`pdfioFileClose`](@@) function closes a PDF file and frees all memory that
was used for it:

```c
pdfioFileClose(pdf);
```


Writing PDF Files
-----------------

You create a new PDF file using the [`pdfioFileCreate`](@@) function:

```c
pdfio_rect_t media_box = { 0.0, 0.0, 612.0, 792.0 };  // US Letter
pdfio_rect_t crop_box = { 36.0, 36.0, 576.0, 756.0 }; // w/0.5" margins

pdfio_file_t *pdf = pdfioFileCreate("myoutputfile.pdf", "2.0", &media_box, &crop_box, error_cb, error_data);
```

where the six arguments to the function are the filename ("myoutputfile.pdf"),
PDF version ("2.0"), media box (`media_box`), crop box (`crop_box`), an optional
error callback function (`error_cb`), and an optional pointer value for the
error callback function (`error_data`).  The units for the media and crop boxes
are points (1/72nd of an inch).

Alternately you can stream a PDF file using the [`pdfioFileCreateOutput`](@@)
function:

```c
pdfio_rect_t media_box = { 0.0, 0.0, 612.0, 792.0 };  // US Letter
pdfio_rect_t crop_box = { 36.0, 36.0, 576.0, 756.0 }; // w/0.5" margins

pdfio_file_t *pdf = pdfioFileCreateOutput(output_cb, output_ctx, "2.0", &media_box, &crop_box, error_cb, error_data);
```

Once the file is created, use the [`pdfioFileCreateObj`](@@),
[`pdfioFileCreatePage`](@@), and [`pdfioPageCopy`](@@) functions to create
objects and pages in the file.

Finally, the [`pdfioFileClose`](@@) function writes the PDF cross-reference and
"trailer" information, closes the file, and frees all memory that was used for
it.


PDF Objects
-----------

PDF objects are identified using two numbers - the object number (1 to N) and
the object generation (0 to 65535) that specifies a particular version of an
object.  An object's numbers are returned by the [`pdfioObjGetNumber`](@@) and
[`pdfioObjGetGeneration`](@@) functions.  You can find a numbered object using
the [`pdfioFileFindObj`](@@) function.

Objects contain values (typically dictionaries) and usually an associated data
stream containing images, fonts, ICC profiles, and page content.  PDFio provides several accessor functions to get the value(s) associated with an object:

- [`pdfioObjGetArray`](@@) returns an object's array value, if any
- [`pdfioObjGetDict`](@@) returns an object's dictionary value, if any
- [`pdfioObjGetLength`](@@) returns the length of the data stream, if any
- [`pdfioObjGetSubtype`](@@) returns the sub-type name of the object, for
  example "Image" for an image object.
- [`pdfioObjGetType`](@@) returns the type name of the object, for example
  "XObject" for an image object.


PDF Streams
-----------

Some PDF objects have an associated data stream, such as for pages, images, ICC
color profiles, and fonts.  You access the stream for an existing object using
the [`pdfioObjOpenStream`](@@) function:

```c
pdfio_file_t *pdf = pdfioFileOpen(...);
pdfio_obj_t *obj = pdfioFileFindObj(pdf, number);
pdfio_stream_t *st = pdfioObjOpenStream(obj, true);
```

The first argument is the object pointer.  The second argument is a boolean
value that specifies whether you want to decode (typically decompress) the
stream data or return it as-is.

When reading a page stream you'll use the [`pdfioPageOpenStream`](@@) function
instead:

```c
pdfio_file_t *pdf = pdfioFileOpen(...);
pdfio_obj_t *obj = pdfioFileGetPage(pdf, number);
pdfio_stream_t *st = pdfioPageOpenStream(obj, 0, true);
```

Once you have the stream open, you can use one of several functions to read
from it:

- [`pdfioStreamConsume`](@@) reads and discards a number of bytes in the stream
- [`pdfioStreamGetToken`](@@) reads a PDF token from the stream
- [`pdfioStreamPeek`](@@) peeks at the next stream data without advancing or
  "consuming" it
- [`pdfioStreamRead`](@@) reads a buffer of data

When you are done reading from the stream, call the [`pdfioStreamClose`](@@)
function:

```c
pdfioStreamClose(st);
```

To create a stream for a new object, call the [`pdfioObjCreateStream`](@@)
function:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);
pdfio_obj_t *obj = pdfioFileCreateObj(pdf, ...);
pdfio_stream_t *st = pdfioObjCreateStream(obj, PDFIO_FILTER_FLATE);
```

The first argument is the newly created object.  The second argument is either
`PDFIO_FILTER_NONE` to specify that any encoding is done by your program or
`PDFIO_FILTER_FLATE` to specify that PDFio should Flate compress the stream.

To create a page content stream call the [`pdfioFileCreatePage`](@@) function:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);
pdfio_dict_t *dict = pdfioDictCreate(pdf);
... set page dictionary keys and values ...
pdfio_stream_t *st = pdfioFileCreatePage(pdf, dict);
```

Once you have created the stream, use any of the following functions to write
to the stream:

- [`pdfioStreamPrintf`](@@) writes a formatted string to the stream
- [`pdfioStreamPutChar`](@@) writes a single character to the stream
- [`pdfioStreamPuts`](@@) writes a C string to the stream
- [`pdfioStreamWrite`](@@) writes a buffer of data to the stream

The [PDF content helper functions](@) provide additional functions for writing
specific PDF page stream commands.

When you are done writing the stream, call [`pdfioStreamCLose`](@@) to close
both the stream and the object.


PDF Content Helper Functions
----------------------------

PDFio includes many helper functions for embedding or writing specific kinds of
content to a PDF file.  These functions can be roughly grouped into ???
categories:

- [Color Space Functions](@)
- [Font Object Functions](@)
- [Image Object Functions](@)
- [Page Stream Functions](@)
- [Page Dictionary Functions](@)


### Color Space Functions

PDF color spaces are specified using well-known names like "DeviceCMYK",
"DeviceGray", and "DeviceRGB" or using arrays that define so-called calibrated
color spaces.  PDFio provides several functions for embedding ICC profiles and
creating color space arrays:

- [`pdfioArrayCreateColorFromICCObj`](@@) creates a color array for an ICC color profile object
- [`pdfioArrayCreateColorFromMatrix`](@@) creates a color array using a CIE XYZ color transform matrix, a gamma value, and a CIE XYZ white point
- [`pdfioArrayCreateColorFromPalette`](@@) creates an indexed color array from an array of sRGB values
- [`pdfioArrayCreateColorFromPrimaries`](@@) creates a color array using CIE XYZ primaries and a gamma value
- [`pdfioArrayCreateColorFromStandard`](@@) creates a color array for a standard color space

You can embed an ICC color profile using the
[`pdfioFileCreateICCObjFromFile`](@@) function:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);
pdfio_obj_t *icc = pdfioFileCreateICCObjFromFile(pdf, "filename.icc");
```

where the first argument is the PDF file and the second argument is the filename
of the ICC color profile.

PDFio also includes predefined constants for creating a few standard color
spaces:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);

// Create an AdobeRGB color array
pdfio_array_t *adobe_rgb = pdfioArrayCreateColorFromStandard(pdf, 3, PDFIO_CS_ADOBE);

// Create an Display P3 color array
pdfio_array_t *display_p3 = pdfioArrayCreateColorFromStandard(pdf, 3, PDFIO_CS_P3_D65);

// Create an sRGB color array
pdfio_array_t *srgb = pdfioArrayCreateColorFromStandard(pdf, 3, PDFIO_CS_SRGB);
```


### Font Object Functions

PDF supports many kinds of fonts, including PostScript Type1, PDF Type3,
TrueType/OpenType, and CID.  PDFio provides two functions for creating font
objects.  The first is [`pdfioFileCreateFontObjFromBase`](@@) which creates a
font object for one of the base PDF fonts:

- "Courier"
- "Courier-Bold"
- "Courier-BoldItalic"
- "Courier-Italic"
- "Helvetica"
- "Helvetica-Bold"
- "Helvetica-BoldOblique"
- "Helvetica-Oblique"
- "Symbol"
- "Times-Bold"
- "Times-BoldItalic"
- "Times-Italic"
- "Times-Roman"
- "ZapfDingbats"

PDFio always uses the Windows CP1252 subset of Unicode for these fonts.

The second function is [`pdfioFileCreateFontObjFromFile`](@@) which creates a
font object from a TrueType/OpenType font file, for example:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);
pdfio_obj_t *arial = pdfioFileCreateFontObjFromFile(pdf, "OpenSans-Regular.ttf", false);
```

will embed an OpenSans Regular TrueType font using the Windows CP1252 subset of
Unicode.  Pass `true` for the third argument to embed it as a Unicode CID font
instead, for example:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);
pdfio_obj_t *arial = pdfioFileCreateFontObjFromFile(pdf, "NotoSansJP-Regular.otf", true);
```

will embed the NotoSansJP Regular OpenType font with full support for Unicode.

Note: Not all fonts support Unicode.


### Image Object Functions

PDF supports images with many different color spaces and bit depths with
optional transparency.  PDFio provides two helper functions for creating image
objects that can be referenced in page streams.  The first function is
[`pdfioFileCreateImageObjFromData`](@@) which creates an image object from data
in memory, for example:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);
unsigned char data[1024 * 1024 * 4]; // 1024x1024 RGBA image data
pdfio_obj_t *img = pdfioFileCreateImageObjFromData(pdf, data, /*width*/1024, /*height*/1024, /*num_colors*/3, /*color_data*/NULL, /*alpha*/true, /*interpolate*/false);
```

will create an object for a 1024x1024 RGBA image in memory, using the default
color space for 3 colors ("DeviceRGB").  We can use one of the
[color space functions](@) to use a specific color space for this image, for
example:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);

// Create an AdobeRGB color array
pdfio_array_t *adobe_rgb = pdfioArrayCreateColorFromMatrix(pdf, 3, pdfioAdobeRGBGamma, pdfioAdobeRGBMatrix, pdfioAdobeRGBWhitePoint);

// Create a 1024x1024 RGBA image using AdobeRGB
unsigned char data[1024 * 1024 * 4]; // 1024x1024 RGBA image data
pdfio_obj_t *img = pdfioFileCreateImageObjFromData(pdf, data, /*width*/1024, /*height*/1024, /*num_colors*/3, /*color_data*/adobe_rgb, /*alpha*/true, /*interpolate*/false);
```

The "interpolate" argument specifies whether the colors in the image should be
smoothed/interpolated when scaling.  This is most useful for photographs but
should be `false` for screenshot and barcode images.

If you have a JPEG or PNG file, use the [`pdfioFileCreateImageObjFromFile`](@@)
function to copy the image into a PDF image object, for example:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);
pdfio_obj_t *img = pdfioFileCreateImageObjFromFile(pdf, "myphoto.jpg", /*interpolate*/true);
```


### Page Dictionary Functions

PDF pages each have an associated dictionary to specify the images, fonts, and color spaces used by the page.  PDFio provides functions to add these resources
to the dictionary:

- [`pdfioPageDictAddColorSpace`](@@) adds a named color space to the page dictionary
- [`pdfioPageDictAddFont`](@@) adds a named font to the page dictionary
- [`pdfioPageDictAddImage`](@@) adds a named image to the page dictionary


### Page Stream Functions

PDF page streams contain textual commands for drawing on the page.  PDFio
provides many functions for writing these commands with the correct format and
escaping, as needed:

- [`pdfioContentClip`](@@) clips future drawing to the current path
- [`pdfioContentDrawImage`](@@) draws an image object
- [`pdfioContentFill`](@@) fills the current path
- [`pdfioContentFillAndStroke`](@@) fills and strokes the current path
- [`pdfioContentMatrixConcat`](@@) concatenates a matrix with the current
  transform matrix
- [`pdfioContentMatrixRotate`](@@) concatenates a rotation matrix with the
  current transform matrix
- [`pdfioContentMatrixScale`](@@) concatenates a scaling matrix with the
  current transform matrix
- [`pdfioContentMatrixTranslate`](@@) concatenates a translation matrix with the
  current transform matrix
- [`pdfioContentPathClose`](@@) closes the current path
- [`pdfioContentPathCurve`](@@) appends a Bezier curve to the current path
- [`pdfioContentPathCurve13`](@@) appends a Bezier curve with 2 control points
  to the current path
- [`pdfioContentPathCurve23`](@@) appends a Bezier curve with 2 control points
  to the current path
- [`pdfioContentPathLineTo`](@@) appends a line to the current path
- [`pdfioContentPathMoveTo`](@@) moves the current point in the current path
- [`pdfioContentPathRect`](@@) appends a rectangle to the current path
- [`pdfioContentRestore`](@@) restores a previous graphics state
- [`pdfioContentSave`](@@) saves the current graphics state
- [`pdfioContentSetDashPattern`](@@) sets the line dash pattern
- [`pdfioContentSetFillColorDeviceCMYK`](@@) sets the current fill color using a
  device CMYK color
- [`pdfioContentSetFillColorDeviceGray`](@@) sets the current fill color using a
  device gray color
- [`pdfioContentSetFillColorDeviceRGB`](@@) sets the current fill color using a
  device RGB color
- [`pdfioContentSetFillColorGray`](@@) sets the current fill color using a
  calibrated gray color
- [`pdfioContentSetFillColorRGB`](@@) sets the current fill color using a
  calibrated RGB color
- [`pdfioContentSetFillColorSpace`](@@) sets the current fill color space
- [`pdfioContentSetFlatness`](@@) sets the flatness for curves
- [`pdfioContentSetLineCap`](@@) sets how the ends of lines are stroked
- [`pdfioContentSetLineJoin`](@@) sets how connections between lines are stroked
- [`pdfioContentSetLineWidth`](@@) sets the width of stroked lines
- [`pdfioContentSetMiterLimit`](@@) sets the miter limit for stroked lines
- [`pdfioContentSetStrokeColorDeviceCMYK`](@@) sets the current stroke color
  using a device CMYK color
- [`pdfioContentSetStrokeColorDeviceGray`](@@) sets the current stroke color
  using a device gray color
- [`pdfioContentSetStrokeColorDeviceRGB`](@@) sets the current stroke color
  using a device RGB color
- [`pdfioContentSetStrokeColorGray`](@@) sets the current stroke color
  using a calibrated gray color
- [`pdfioContentSetStrokeColorRGB`](@@) sets the current stroke color
  using a calibrated RGB color
- [`pdfioContentSetStrokeColorSpace`](@@) sets the current stroke color space
- [`pdfioContentSetTextCharacterSpacing`](@@) sets the spacing between
  characters for text
- [`pdfioContentSetTextFont`](@@) sets the font and size for text
- [`pdfioContentSetTextLeading`](@@) sets the line height for text
- [`pdfioContentSetTextMatrix`](@@) concatenates a matrix with the current text
  matrix
- [`pdfioContentSetTextRenderingMode`](@@) sets the text rendering mode
- [`pdfioContentSetTextRise`](@@) adjusts the baseline for text
- [`pdfioContentSetTextWordSpacing`](@@) sets the spacing between words for text
- [`pdfioContentSetTextXScaling`](@@) sets the horizontal scaling for text
- [`pdfioContentStroke`](@@) strokes the current path
- [`pdfioContentTextBegin`](@@) begins a block of text
- [`pdfioContentTextEnd`](@@) ends a block of text
- [`pdfioContentTextMoveLine`](@@) moves to the next line with an offset in a
  text block
- [`pdfioContentTextMoveTo`](@@) moves within the current line in a text block
- [`pdfioContentTextNextLine`](@@) moves to the beginning of the next line in a
  text block
- [`pdfioContentTextShow`](@@) draws a literal string in a text block
- [`pdfioContentTextShowf`](@@) draws a formatted string in a text block
- [`pdfioContentTextShowJustified`](@@) draws an array of literal strings with
  offsets between them
