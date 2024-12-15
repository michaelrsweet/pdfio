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

PDFio is Copyright © 2021-2024 by Michael R Sweet and is licensed under the
Apache License Version 2.0 with an (optional) exception to allow linking against
GPL2/LGPL2 software.  See the files "LICENSE" and "NOTICE" for more information.


Requirements
------------

PDFio requires the following to build the software:

- A C99 compiler such as Clang, GCC, or MS Visual C
- A POSIX-compliant `make` program
- A POSIX-compliant `sh` program
- ZLIB (<https://www.zlib.net>) 1.0 or higher

IDE files for Xcode (macOS/iOS) and Visual Studio (Windows) are also provided.


Installing pdfio
----------------

PDFio comes with a configure script that creates a portable makefile that will
work on any POSIX-compliant system with ZLIB installed.  To make it, run:

    ./configure
    make

To test it, run:

    make test

To install it, run:

    sudo make install

If you want a shared library, run:

    ./configure --enable-shared
    make
    sudo make install

The default installation location is "/usr/local".  Pass the `--prefix` option
to make to install it to another location:

    ./configure --prefix=/some/other/directory

Other configure options can be found using the `--help` option:

    ./configure --help


Visual Studio Project
---------------------

The Visual Studio solution ("pdfio.sln") is provided for Windows developers and
generates both a static library and DLL.


Xcode Project
-------------

There is also an Xcode project ("pdfio.xcodeproj") you can use on macOS which
generates a static library that will be installed under "/usr/local" with:

    sudo xcodebuild install


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


Understanding PDF Files
-----------------------

A PDF file provides data and commands for displaying pages of graphics and text,
and is structured in a way that allows it to be displayed in the same way across
multiple devices and platforms.  The following is a PDF which shows "Hello,
World!" on one page:

```
%PDF-1.0                        % Header starts here
%âãÏÓ
1 0 obj                         % Body starts here
<<
/Kids [2 0 R]
/Count 1
/Type /Pages
>>
endobj
2 0 obj
<<
/Rotate 0
/Parent 1 0 R
/Resources 3 0 R
/MediaBox [0 0 612 792]
/Contents [4 0 R]/Type /Page
>>
endobj
3 0 obj
<<
/Font
<<
/F0
<<
/BaseFont /Times-Italic
/Subtype /Type1
/Type /Font
>>
>>
>>
endobj
4 0 obj
<<
/Length 65
>>
stream
1. 0. 0. 1. 50. 700. cm
BT
  /F0 36. Tf
  (Hello, World!) Tj
ET
endstream
endobj
5 0 obj
<<
/Pages 1 0 R
/Type /Catalog
>>
endobj
xref                            % Cross-reference table starts here
0 6
0000000000 65535 f
0000000015 00000 n
0000000074 00000 n
0000000192 00000 n
0000000291 00000 n
0000000409 00000 n
trailer                         % Trailer starts here
<<
/Root 5 0 R
/Size 6
>>
startxref
459
%%EOF
```


### Header

The header is the first line of a PDF file that specifies the version of the PDF
format that has been used, for example `%PDF-1.0`.

Since PDF files almost always contain binary data, they can become corrupted if
line endings are changed.  For example, if the file is transferred using FTP in
text mode or is edited in Notepad on Windows.  To allow legacy file transfer
programs to determine that the file is binary, the PDF standard recommends
including some bytes with character codes higher than 127 in the header, for
example:

```
%âãÏÓ
```

The percent sign indicates a comment line while the other few bytes are
arbitrary character codes in excess of 127.  So, the whole header in our example
is:

```
%PDF-1.0
%âãÏÓ
```


### Body

The file body consists of a sequence of objects, each preceded by an object
number, generation number, and the obj keyword on one line, and followed by the
endobj keyword on another.  For example:

```
1 0 obj
<<
/Kids [2 0 R]
/Count 1
/Type /Pages
>>
endobj
```

In this example, the object number is 1 and the generation number is 0, meaning
it is the first version of the object.  The content for object 1 is between the
initial `1 0 obj` and trailing `endobj` lines.  In this case, the content is the
dictionary `<</Kids [2 0 R] /Count 1 /Type /Pages>>`.


### Cross-Reference Table

The cross-reference table lists the byte offset of each object in the file body.
This allows random access to objects, meaning they don't have to be read in
order.  Objects that are not used are never read, making the process efficient.
Operations like counting the number of pages in a PDF document are fast, even in
large files.

Each object has an object number and a generation number.  Generation numbers
are used when a cross-reference table entry is reused.  For simplicity, we will
assume generation numbers to be always zero and ignore them.  The
cross-reference table consists of a header line that indicates the number of
entries, a free entry line for object 0, and a line for each of the objects in
the file body.  For example:

```
0 6                             % Six entries in table, starting at 0
0000000000 65535 f              % Free entry for object 0
0000000015 00000 n              % Object 1 is at byte offset 15
0000000074 00000 n              % Object 2 is at byte offset 74
0000000192 00000 n              % etc...
0000000291 00000 n
0000000409 00000 n              % Object 5 is at byte offset 409
```


### Trailer

The first line of the trailer is just the `trailer` keyword.  This is followed
by the trailer dictionary which contains at least the `/Size` entry specifying
the number of entries in the cross-reference table and the `/Root` entry which
references the object for the document catalog which is the root element of the
graph of objects in the body.

There follows a line with just the `startxref` keyword, a line with a single
number specifying the byte offset of the start of the cross-reference table
within the file, and then the line `%%EOF` which signals the end of the PDF
file.

```
trailer                         % Trailer keyword
<<                              % The trailer dictinonary
/Root 5 0 R
/Size 6
>>
startxref                       % startxref keyword
459                             % Byte offset of cross-reference table
%%EOF                           % End-of-file marker
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
pdfio_file_t *pdf = pdfioFileOpen("myinputfile.pdf", password_cb, password_data,
                                  error_cb, error_data);

```

where the five arguments to the function are the filename ("myinputfile.pdf"),
an optional password callback function (`password_cb`) and data pointer value
(`password_data`), and an optional error callback function (`error_cb`) and data
pointer value (`error_data`).  The password callback is called for encrypted PDF
files that are not using the default password, for example:

```c
const char *
password_cb(void *data, const char *filename)
{
  (void)data;     // This callback doesn't use the data pointer
  (void)filename; // This callback doesn't use the filename

  // Return a password string for the file...
  return ("Password42");
}
```

The error callback is called for both errors and warnings and accepts the
`pdfio_file_t` pointer, a message string, and the callback pointer value, for
example:

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
functions to access the content streams for each page, and
[`pdfioObjGetDict`](@@) to get the associated page object dictionary.  For
example, if you want to display the media and crop boxes for a given page:

```c
pdfio_file_t  *pdf;             // PDF file
size_t        i;                // Looping var
size_t        count;            // Number of pages
pdfio_obj_t   *page;            // Current page
pdfio_dict_t  *dict;            // Current page dictionary
pdfio_array_t *media_box;       // MediaBox array
double        media_values[4];  // MediaBox values
pdfio_array_t *crop_box;        // CropBox array
double        crop_values[4];   // CropBox values

// Iterate the pages in the PDF file
for (i = 0, count = pdfioFileGetNumPages(pdf); i < count; i ++)
{
  page = pdfioFileGetPage(pdf, i);
  dict = pdfioObjGetDict(page);

  media_box       = pdfioDictGetArray(dict, "MediaBox");
  media_values[0] = pdfioArrayGetNumber(media_box, 0);
  media_values[1] = pdfioArrayGetNumber(media_box, 1);
  media_values[2] = pdfioArrayGetNumber(media_box, 2);
  media_values[3] = pdfioArrayGetNumber(media_box, 3);

  crop_box       = pdfioDictGetArray(dict, "CropBox");
  crop_values[0] = pdfioArrayGetNumber(crop_box, 0);
  crop_values[1] = pdfioArrayGetNumber(crop_box, 1);
  crop_values[2] = pdfioArrayGetNumber(crop_box, 2);
  crop_values[3] = pdfioArrayGetNumber(crop_box, 3);

  printf("Page %u: MediaBox=[%g %g %g %g], CropBox=[%g %g %g %g]\n",
         (unsigned)(i + 1),
         media_values[0], media_values[1], media_values[2], media_values[3],
         crop_values[0], crop_values[1], crop_values[2], crop_values[3]);
}
```

Page object dictionaries have several (mostly optional) key/value pairs,
including:

- "Annots": An array of annotation dictionaries for the page; use
  [`pdfioDictGetArray`](@@) to get the array
- "CropBox": The crop box as an array of four numbers for the left, bottom,
  right, and top coordinates of the target media; use [`pdfioDictGetArray`](@@)
  to get a pointer to the array of numbers
- "Dur": The number of seconds the page should be displayed; use
  [`pdfioDictGetNumber`](@@) to get the page duration value
- "Group": The dictionary of transparency group values for the page; use
  [`pdfioDictGetDict`](@@) to get a pointer to the resources dictionary
- "LastModified": The date and time when this page was last modified; use
  [`pdfioDictGetDate`](@@) to get the Unix `time_t` value
- "Parent": The parent page tree node object for this page; use
  [`pdfioDictGetObj`](@@) to get a pointer to the object
- "MediaBox": The media box as an array of four numbers for the left, bottom,
  right, and top coordinates of the target media; use [`pdfioDictGetArray`](@@)
  to get a pointer to the array of numbers
- "Resources": The dictionary of resources for the page; use
  [`pdfioDictGetDict`](@@) to get a pointer to the resources dictionary
- "Rotate": A number indicating the number of degrees of counter-clockwise
  rotation to apply to the page when viewing; use [`pdfioDictGetNumber`](@@)
  to get the rotation angle
- "Thumb": A thumbnail image object for the page; use [`pdfioDictGetObj`](@@)
  to get a pointer to the thumbnail image object
- "Trans": The page transition dictionary; use [`pdfioDictGetDict`](@@) to get
  a pointer to the dictionary

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

pdfio_file_t *pdf = pdfioFileCreate("myoutputfile.pdf", "2.0",
                                    &media_box, &crop_box,
                                    error_cb, error_data);
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

pdfio_file_t *pdf = pdfioFileCreateOutput(output_cb, output_ctx, "2.0",
                                          &media_box, &crop_box,
                                          error_cb, error_data);
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

When you are done writing the stream, call [`pdfioStreamClose`](@@) to close
both the stream and the object.


PDF Content Helper Functions
----------------------------

PDFio includes many helper functions for embedding or writing specific kinds of
content to a PDF file.  These functions can be roughly grouped into five
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
pdfio_array_t *adobe_rgb =
    pdfioArrayCreateColorFromStandard(pdf, 3, PDFIO_CS_ADOBE);

// Create an Display P3 color array
pdfio_array_t *display_p3 =
    pdfioArrayCreateColorFromStandard(pdf, 3, PDFIO_CS_P3_D65);

// Create an sRGB color array
pdfio_array_t *srgb =
    pdfioArrayCreateColorFromStandard(pdf, 3, PDFIO_CS_SRGB);
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

Except for Symbol and ZapfDingbats (which use a custom 8-bit character set),
PDFio always uses the Windows CP1252 subset of Unicode for these fonts.

The second function is [`pdfioFileCreateFontObjFromFile`](@@) which creates a
font object from a TrueType/OpenType font file, for example:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);
pdfio_obj_t *arial =
    pdfioFileCreateFontObjFromFile(pdf, "OpenSans-Regular.ttf", false);
```

will embed an OpenSans Regular TrueType font using the Windows CP1252 subset of
Unicode.  Pass `true` for the third argument to embed it as a Unicode CID font
instead, for example:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);
pdfio_obj_t *arial =
    pdfioFileCreateFontObjFromFile(pdf, "NotoSansJP-Regular.otf", true);
```

will embed the NotoSansJP Regular OpenType font with full support for Unicode.

> Note: Not all fonts support Unicode, and most do not contain a full
> complement of Unicode characters.  `pdfioFileCreateFontObjFromFile` does not
> perform any character subsetting, so the entire font file is embedded in the
> PDF file.


### Image Object Functions

PDF supports images with many different color spaces and bit depths with
optional transparency.  PDFio provides two helper functions for creating image
objects that can be referenced in page streams.  The first function is
[`pdfioFileCreateImageObjFromData`](@@) which creates an image object from data
in memory, for example:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);
unsigned char data[1024 * 1024 * 4]; // 1024x1024 RGBA image data
pdfio_obj_t *img =
    pdfioFileCreateImageObjFromData(pdf, data, /*width*/1024,
                                    /*height*/1024, /*num_colors*/3,
                                    /*color_data*/NULL, /*alpha*/true,
                                    /*interpolate*/false);
```

will create an object for a 1024x1024 RGBA image in memory, using the default
color space for 3 colors ("DeviceRGB").  We can use one of the
[color space functions](@) to use a specific color space for this image, for
example:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);

// Create an AdobeRGB color array
pdfio_array_t *adobe_rgb =
    pdfioArrayCreateColorFromMatrix(pdf, 3, pdfioAdobeRGBGamma,
                                    pdfioAdobeRGBMatrix,
                                    pdfioAdobeRGBWhitePoint);

// Create a 1024x1024 RGBA image using AdobeRGB
unsigned char data[1024 * 1024 * 4]; // 1024x1024 RGBA image data
pdfio_obj_t *img =
    pdfioFileCreateImageObjFromData(pdf, data, /*width*/1024,
                                    /*height*/1024, /*num_colors*/3,
                                    /*color_data*/adobe_rgb,
                                    /*alpha*/true,
                                    /*interpolate*/false);
```

The "interpolate" argument specifies whether the colors in the image should be
smoothed/interpolated when scaling.  This is most useful for photographs but
should be `false` for screenshot and barcode images.

If you have a JPEG or PNG file, use the [`pdfioFileCreateImageObjFromFile`](@@)
function to copy the image into a PDF image object, for example:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);
pdfio_obj_t *img =
    pdfioFileCreateImageObjFromFile(pdf, "myphoto.jpg",
                                    /*interpolate*/true);
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
- [`pdfioContentTextNewLine`](@@) moves to the beginning of the next line in a
  text block
- [`pdfioContentTextNewLineShow`](@@) moves to the beginning of the next line in a
  text block and shows literal text with optional word and character spacing
- [`pdfioContentTextNewLineShowf`](@@) moves to the beginning of the next line in a
  text block and shows formatted text with optional word and character spacing
- [`pdfioContentTextShow`](@@) draws a literal string in a text block
- [`pdfioContentTextShowf`](@@) draws a formatted string in a text block
- [`pdfioContentTextShowJustified`](@@) draws an array of literal strings with
  offsets between them


Examples
========

Read PDF Metadata
-----------------

The following example function will open a PDF file and print the title, author,
creation date, and number of pages:

```c
#include <pdfio.h>
#include <time.h>


void
show_pdf_info(const char *filename)
{
  pdfio_file_t *pdf;
  time_t       creation_date;
  struct tm    *creation_tm;
  char         creation_text[256];


  // Open the PDF file with the default callbacks...
  pdf = pdfioFileOpen(filename, /*password_cb*/NULL,
                      /*password_cbdata*/NULL, /*error_cb*/NULL,
                      /*error_cbdata*/NULL);
  if (pdf == NULL)
    return;

  // Get the creation date and convert to a string...
  creation_date = pdfioFileGetCreationDate(pdf);
  creation_tm   = localtime(&creation_date);
  strftime(creation_text, sizeof(creation_text), "%c", &creation_tm);

  // Print file information to stdout...
  printf("%s:\n", filename);
  printf("         Title: %s\n", pdfioFileGetTitle(pdf));
  printf("        Author: %s\n", pdfioFileGetAuthor(pdf));
  printf("    Created On: %s\n", creation_text);
  printf("  Number Pages: %u\n", (unsigned)pdfioFileGetNumPages(pdf));

  // Close the PDF file...
  pdfioFileClose(pdf);
}
```


Create PDF File With Text and Image
-----------------------------------

The following example function will create a PDF file, embed a base font and the
named JPEG or PNG image file, and then creates a page with the image centered on
the page with the text centered below:

```c
#include <pdfio.h>
#include <pdfio-content.h>
#include <string.h>


void
create_pdf_image_file(const char *pdfname, const char *imagename,
                      const char *caption)
{
  pdfio_file_t   *pdf;
  pdfio_obj_t    *font;
  pdfio_obj_t    *image;
  pdfio_dict_t   *dict;
  pdfio_stream_t *page;
  double         width, height;
  double         swidth, sheight;
  double         tx, ty;


  // Create the PDF file...
  pdf = pdfioFileCreate(pdfname, /*version*/NULL, /*media_box*/NULL,
                        /*crop_box*/NULL, /*error_cb*/NULL,
                        /*error_cbdata*/NULL);

  // Create a Courier base font for the caption
  font = pdfioFileCreateFontObjFromBase(pdf, "Courier");

  // Create an image object from the JPEG/PNG image file...
  image = pdfioFileCreateImageObjFromFile(pdf, imagename, true);

  // Create a page dictionary with the font and image...
  dict = pdfioDictCreate(pdf);
  pdfioPageDictAddFont(dict, "F1", font);
  pdfioPageDictAddImage(dict, "IM1", image);

  // Create the page and its content stream...
  page = pdfioFileCreatePage(pdf, dict);

  // Position and scale the image on the page...
  width  = pdfioImageGetWidth(image);
  height = pdfioImageGetHeight(image);

  // Default media_box is "universal" 595.28x792 points (8.27x11in or
  // 210x279mm).  Use margins of 36 points (0.5in or 12.7mm) with another
  // 36 points for the caption underneath...
  swidth  = 595.28 - 72.0;
  sheight = swidth * height / width;
  if (sheight > (792.0 - 36.0 - 72.0))
  {
    sheight = 792.0 - 36.0 - 72.0;
    swidth  = sheight * width / height;
  }

  tx = 0.5 * (595.28 - swidth);
  ty = 0.5 * (792 - 36 - sheight);

  pdfioContentDrawImage(page, "IM1", tx, ty + 36.0, swidth, sheight);

  // Draw the caption in black...
  pdfioContentSetFillColorDeviceGray(page, 0.0);

  // Compute the starting point for the text - Courier is monospaced
  // with a nominal width of 0.6 times the text height...
  tx = 0.5 * (595.28 - 18.0 * 0.6 * strlen(caption));

  // Position and draw the caption underneath...
  pdfioContentTextBegin(page);
  pdfioContentSetTextFont(page, "F1", 18.0);
  pdfioContentTextMoveTo(page, tx, ty);
  pdfioContentTextShow(page, /*unicode*/false, caption);
  pdfioContentTextEnd(page);

  // Close the page stream and the PDF file...
  pdfioStreamClose(page);
  pdfioFileClose(pdf);
}
```
