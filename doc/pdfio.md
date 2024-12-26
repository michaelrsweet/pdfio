Introduction
============

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


Installing PDFio
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
pdfio_file_t *pdf =
    pdfioFileOpen("myinputfile.pdf", password_cb, password_data, error_cb,
                  error_data);

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

pdfio_file_t *pdf = pdfioFileCreate("myoutputfile.pdf", "2.0", &media_box, &crop_box,
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

pdfio_file_t *pdf = pdfioFileCreateOutput(output_cb, output_ctx, "2.0", &media_box,
                                          &crop_box, error_cb, error_data);
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

- [`pdfioArrayCreateColorFromICCObj`](@@) creates a color array for an ICC color
  profile object
- [`pdfioArrayCreateColorFromMatrix`](@@) creates a color array using a CIE XYZ
  color transform matrix, a gamma value, and a CIE XYZ white point
- [`pdfioArrayCreateColorFromPalette`](@@) creates an indexed color array from
  an array of sRGB values
- [`pdfioArrayCreateColorFromPrimaries`](@@) creates a color array using CIE XYZ
  primaries and a gamma value
- [`pdfioArrayCreateColorFromStandard`](@@) creates a color array for a standard
  color space

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
    pdfioFileCreateImageObjFromData(pdf, data, /*width*/1024, /*height*/1024,
                                    /*num_colors*/3, /*color_data*/NULL,
                                    /*alpha*/true, /*interpolate*/false);
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
                                    pdfioAdobeRGBMatrix, pdfioAdobeRGBWhitePoint);

// Create a 1024x1024 RGBA image using AdobeRGB
unsigned char data[1024 * 1024 * 4]; // 1024x1024 RGBA image data
pdfio_obj_t *img =
    pdfioFileCreateImageObjFromData(pdf, data, /*width*/1024, /*height*/1024,
                                    /*num_colors*/3, /*color_data*/adobe_rgb,
                                    /*alpha*/true, /*interpolate*/false);
```

The "interpolate" argument specifies whether the colors in the image should be
smoothed/interpolated when scaling.  This is most useful for photographs but
should be `false` for screenshot and barcode images.

If you have a JPEG or PNG file, use the [`pdfioFileCreateImageObjFromFile`](@@)
function to copy the image into a PDF image object, for example:

```c
pdfio_file_t *pdf = pdfioFileCreate(...);
pdfio_obj_t *img =
    pdfioFileCreateImageObjFromFile(pdf, "myphoto.jpg", /*interpolate*/true);
```

> Note: Currently `pdfioFileCreateImageObjFromFile` does not support 12 bit JPEG
> files or PNG files with an alpha channel.


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

PDFio includes several example programs that are typically installed to the
`/usr/share/doc/pdfio/examples` or `/usr/local/share/doc/pdfio/examples`
directories.  A makefile is included to build them.


Read PDF Metadata
-----------------

The `pdfioinfo.c` example program opens a PDF file and prints the title, author,
creation date, and number of pages:

```c
#include <pdfio.h>
#include <time.h>


int                                     // O - Exit status
main(int  argc,                         // I - Number of command-line arguments
     char *argv[])                      // Command-line arguments
{
  const char    *filename;              // PDF filename
  pdfio_file_t  *pdf;                   // PDF file
  time_t        creation_date;          // Creation date
  struct tm     *creation_tm;           // Creation date/time information
  char          creation_text[256];     // Creation date/time as a string


  // Get the filename from the command-line...
  if (argc != 2)
  {
    fputs("Usage: ./pdfioinfo FILENAME.pdf\n", stderr);
    return (1);
  }

  filename = argv[1];

  // Open the PDF file with the default callbacks...
  pdf = pdfioFileOpen(filename, /*password_cb*/NULL, /*password_cbdata*/NULL,
                      /*error_cb*/NULL, /*error_cbdata*/NULL);
  if (pdf == NULL)
    return (1);

  // Get the creation date and convert to a string...
  creation_date = pdfioFileGetCreationDate(pdf);
  creation_tm   = localtime(&creation_date);
  strftime(creation_text, sizeof(creation_text), "%c", creation_tm);

  // Print file information to stdout...
  printf("%s:\n", filename);
  printf("         Title: %s\n", pdfioFileGetTitle(pdf));
  printf("        Author: %s\n", pdfioFileGetAuthor(pdf));
  printf("    Created On: %s\n", creation_text);
  printf("  Number Pages: %u\n", (unsigned)pdfioFileGetNumPages(pdf));

  // Close the PDF file...
  pdfioFileClose(pdf);

  return (0);
}
```


Extract Text from PDF File
--------------------------

The `pdf2text.c` example code extracts non-Unicode text from a PDF file by
scanning each page for strings and text drawing commands.  Since it doesn't
look at the font encoding or support Unicode text, it is really only useful to
extract plain ASCII text from a PDF file.  And since it writes text in the order
it appears in the page stream, it may not come out in the same order as appears
on the page.

The [`pdfioStreamGetToken`](@@) function is used to read individual tokens from
the page streams.  Tokens starting with the open parenthesis are text strings,
while PDF operators are left as-is.  We use some simple logic to make sure that
we include spaces between text strings and add newlines for the text operators
that start a new line in a text block:

```c
pdfio_stream_t *st;              // Page stream
bool           first = true;     // First string on line?
char           buffer[1024];     // Token buffer

// Read PDF tokens from the page stream...
while (pdfioStreamGetToken(st, buffer, sizeof(buffer)))
{
  if (buffer[0] == '(')
  {
    // Text string using an 8-bit encoding
    if (first)
      first = false;
    else if (buffer[1] != ' ')
      putchar(' ');

    fputs(buffer + 1, stdout);
  }
  else if (!strcmp(buffer, "Td") || !strcmp(buffer, "TD") || !strcmp(buffer, "T*") ||
           !strcmp(buffer, "\'") || !strcmp(buffer, "\""))
  {
    // Text operators that advance to the next line in the block
    putchar('\n');
    first = true;
  }
}

if (!first)
  putchar('\n');
```


Create a PDF File With Text and an Image
----------------------------------------

The `image2pdf.c` example code creates a PDF file containing a JPEG or PNG
image file and optional caption on a single page.  The `create_pdf_image_file`
function creates the PDF file, embeds a base font and the named JPEG or PNG
image file, and then creates a page with the image centered on the page with any
text centered below:

```c
#include <pdfio.h>
#include <pdfio-content.h>
#include <string.h>


bool                                    // O - True on success, false on failure
create_pdf_image_file(
    const char *pdfname,                // I - PDF filename
    const char *imagename,              // I - Image filename
    const char *caption)                // I - Caption filename
{
  pdfio_file_t   *pdf;                  // PDF file
  pdfio_obj_t    *font;                 // Caption font
  pdfio_obj_t    *image;                // Image
  pdfio_dict_t   *dict;                 // Page dictionary
  pdfio_stream_t *page;                 // Page stream
  double         width, height;         // Width and height of image
  double         swidth, sheight;       // Scaled width and height on page
  double         tx, ty;                // Position on page


  // Create the PDF file...
  pdf = pdfioFileCreate(pdfname, /*version*/NULL, /*media_box*/NULL, /*crop_box*/NULL,
                        /*error_cb*/NULL, /*error_cbdata*/NULL);
  if (!pdf)
    return (false);

  // Create a Courier base font for the caption
  font = pdfioFileCreateFontObjFromBase(pdf, "Courier");

  if (!font)
  {
    pdfioFileClose(pdf);
    return (false);
  }

  // Create an image object from the JPEG/PNG image file...
  image = pdfioFileCreateImageObjFromFile(pdf, imagename, true);

  if (!image)
  {
    pdfioFileClose(pdf);
    return (false);
  }

  // Create a page dictionary with the font and image...
  dict = pdfioDictCreate(pdf);
  pdfioPageDictAddFont(dict, "F1", font);
  pdfioPageDictAddImage(dict, "IM1", image);

  // Create the page and its content stream...
  page = pdfioFileCreatePage(pdf, dict);

  // Position and scale the image on the page...
  width  = pdfioImageGetWidth(image);
  height = pdfioImageGetHeight(image);

  // Default media_box is "universal" 595.28x792 points (8.27x11in or 210x279mm).
  // Use margins of 36 points (0.5in or 12.7mm) with another 36 points for the
  // caption underneath...
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

  // Compute the starting point for the text - Courier is monospaced with a
  // nominal width of 0.6 times the text height...
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

  return (true);
}
```


Generate a Code 128 Barcode
---------------------------

One-dimensional barcodes are often rendered using special fonts that map ASCII
characters to sequences of bars that can be read.  The `examples` directory
contains such a font (`code128.ttf`) to create "Code 128" barcodes, with an
accompanying bit of example code in `code128.c`.

The first thing you need to do is prepare the barcode string to use with the
font.  Each barcode begins with a start pattern followed by the characters or
digits you want to encode, a weighted sum digit, and a stop pattern.  The
`make_code128` function creates this string:

```c
static char *                           // O - Output string
make_code128(char       *dst,           // I - Destination buffer
             const char *src,           // I - Source string
             size_t     dstsize)        // I - Size of destination buffer
{
  char          *dstptr,                // Pointer into destination buffer
                *dstend;                // End of destination buffer
  int           sum;                    // Weighted sum
  static const char *code128_chars =    // Code 128 characters
                " !\"#$%&'()*+,-./0123456789:;<=>?"
                "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
                "`abcdefghijklmnopqrstuvwxyz{|}~\303"
                "\304\305\306\307\310\311\312";
  static const char code128_start_code_b = '\314';
                                        // Start code B
  static const char code128_stop = '\316';
                                        // Stop pattern


  // Start a Code B barcode...
  dstptr = dst;
  dstend = dst + dstsize - 3;

  *dstptr++ = code128_start_code_b;
  sum       = code128_start_code_b - 100;

  while (*src && dstptr < dstend)
  {
    if (*src >= ' ' && *src < 0x7f)
    {
      sum       += (dstptr - dst) * (*src - ' ');
      *dstptr++ = *src;
    }

    src ++;
  }

  // Add the weighted sum modulo 103
  *dstptr++ = code128_chars[sum % 103];

  // Add the stop pattern and return...
  *dstptr++ = code128_stop;
  *dstptr   = '\0';

  return (dst);
}
```

The `main` function does the rest of the work.  The barcode font is imported
using the [`pdfioFileCreateFontObjFromFile`](@@) function.  We pass `false`
for the "unicode" argument since we just want the (default) ASCII encoding:

```c
barcode_font = pdfioFileCreateFontObjFromFile(pdf, "code128.ttf", /*unicode*/false);
```

Since barcodes usually have the number or text represented by the barcode
printed underneath it, we also need a regular text font, for which we can choose
one of the standard 14 PostScript base fonts using the
[`pdfioFIleCreateFontObjFromBase`](@@) function:

```c
text_font = pdfioFileCreateFontObjFromBase(pdf, "Helvetica");
```

Once we have these fonts we can measure the barcode and regular text labels
using the [`pdfioContentTextMeasure`](@@) function to determine how large the
PDF page needs to be to hold the barcode and text:

```c
// Compute sizes of the text...
const char *barcode = argv[1];
char barcode_temp[256];

if (!(barcode[0] & 0x80))
  barcode = make_code128(barcode_temp, barcode, sizeof(barcode_temp));

double barcode_height = 36.0;
double barcode_width =
    pdfioContentTextMeasure(barcode_font, barcode, barcode_height);

const char *text = argv[2];
double text_height = 0.0;
double text_width = 0.0;

if (text && text_font)
{
  text_height = 9.0;
  text_width  = pdfioContentTextMeasure(text_font, text, text_height);
}

// Compute the size of the PDF page...
pdfio_rect_t media_box;

media_box.x1 = 0.0;
media_box.y1 = 0.0;
media_box.x2 = (barcode_width > text_width ? barcode_width : text_width) + 18.0;
media_box.y2 = barcode_height + text_height + 18.0;
```

Finally, we just need to create a page of the specified size that references the
two fonts:

```c
// Start a page for the barcode...
page_dict = pdfioDictCreate(pdf);

pdfioDictSetRect(page_dict, "MediaBox", &media_box);
pdfioDictSetRect(page_dict, "CropBox", &media_box);

pdfioPageDictAddFont(page_dict, "B128", barcode_font);
if (text_font)
  pdfioPageDictAddFont(page_dict, "TEXT", text_font);

page_st = pdfioFileCreatePage(pdf, page_dict);
```

With the barcode font called "B128" and the text font called "TEXT", we can
use them to draw two strings:

```c
// Draw the page...
pdfioContentSetFillColorGray(page_st, 0.0);

pdfioContentSetTextFont(page_st, "B128", barcode_height);
pdfioContentTextBegin(page_st);
pdfioContentTextMoveTo(page_st, 0.5 * (media_box.x2 - barcode_width),
                       9.0 + text_height);
pdfioContentTextShow(page_st, /*unicode*/false, barcode);
pdfioContentTextEnd(page_st);

if (text && text_font)
{
  pdfioContentSetTextFont(page_st, "TEXT", text_height);
  pdfioContentTextBegin(page_st);
  pdfioContentTextMoveTo(page_st, 0.5 * (media_box.x2 - text_width), 9.0);
  pdfioContentTextShow(page_st, /*unicode*/false, text);
  pdfioContentTextEnd(page_st);
}

pdfioStreamClose(page_st);
```


Convert Markdown to PDF
-----------------------

Markdown is a simple plain text format that supports things like headings,
links, character styles, tables, and embedded images.  The `md2pdf.c` example
code uses the [mmd](https://www.msweet.org/mmd/) library to convert markdown to
a PDF file that can be distributed.

> Note: The md2pdf example is by far the most complex example code included with
> PDFio and shows how to layout text, add headers and footers, add links, embed
> images, format tables, and add an outline (table of contents) for navigation.

### Managing Document State

The `md2pdf` program needs to maintain three sets of state - one for the
markdown document which is represented by nodes of type `mmd_t` and the others
for the PDF document and current PDF page which are contained in the `docdata_t`
structure:

```c
typedef struct docdata_s                // Document formatting data
{
  // State for the whole document
  pdfio_file_t  *pdf;                   // PDF file
  pdfio_rect_t  media_box;              // Media (page) box
  pdfio_rect_t  crop_box;               // Crop box (for margins)
  pdfio_rect_t  art_box;                // Art box (for markdown content)
  pdfio_obj_t   *fonts[DOCFONT_MAX];    // Embedded fonts
  double        font_space;             // Unit width of a space
  size_t        num_images;             // Number of embedded images
  docimage_t    images[DOCIMAGE_MAX];   // Embedded images
  const char    *title;                 // Document title
  char          *heading;               // Current document heading
  size_t        num_actions;            // Number of actions for this document
  docaction_t   actions[DOCACTION_MAX]; // Actions for this document
  size_t        num_targets;            // Number of targets for this document
  doctarget_t   targets[DOCTARGET_MAX]; // Targets for this document
  size_t        num_toc;                // Number of table-of-contents entries
  doctoc_t      toc[DOCTOC_MAX];        // Table-of-contents entries

  // State for the current page
  pdfio_stream_t *st;                   // Current page stream
  double        y;                      // Current position on page
  docfont_t     font;                   // Current font
  double        fsize;                  // Current font size
  doccolor_t    color;                  // Current color
  pdfio_array_t *annots_array;          // Annotations array (for links)
  pdfio_obj_t   *annots_obj;            // Annotations object (for links)
  size_t        num_links;              // Number of links for this page
  doclink_t     links[DOCLINK_MAX];     // Links for this page
} docdata_t;
```


#### Document State

The output is fixed to the "universal" media size (the intersection of US Letter
and ISO A4) with 1/2 inch margins - the `PAGE_` constants can be changed to
select a different size or margins.  The `media_box` member contains the
"MediaBox" rectangle for the PDF pages, while the `crop_box` and `art_box`
members contain the "CropBox" and "ArtBox" values, respectively.

Four embedded fonts are used:

- `DOCFONT_REGULAR`: the default font used for text,
- `DOCFONT_BOLD`: a boldface font used for heading and strong text,
- `DOCFONT_ITALIC`: an italic/oblique font used for emphasized text, and
- `DOCFONT_MONOSPACE`: a fixed-width font used for code.

By default the code uses the base PostScript fonts Helvetica, Helvetica-Bold,
Helvetica-Oblique, and Courier.  The `USE_TRUETYPE` define can be used to
replace these with the Roboto TrueType fonts.

Embedded JPEG and PNG images are copied into the PDF document, with the `images`
array containing the list of the images and their objects.

The `title` member contains the document title, while the `heading` member
contains the current heading text.

The `actions` array contains a list of action dictionaries for interior document
links that need to be resolved, while the `targets` array keeps track of the
location of the headings in the PDF document.

The `toc` array contains a list of headings and is used to construct the PDF
outlines dictionaries/objects, which provides a table of contents for navigation
in most PDF readers.


#### Page State

The `st` member provides the stream for the current page content.  The `color`,
`font`, `fsize`, and `y` members provide the current graphics state on the page.

The `annots_array`, `annots_obj`, `num_links`, and `links` members contain a
list of hyperlinks on the current page.


### Creating Pages

The `new_page` function is used to start a new page.  Aside from creating the
new page object and stream, it adds a standard header and footer to the page.
It starts by closing the current page if it is open:

```c
// Close the current page...
if (dd->st)
{
  pdfioStreamClose(dd->st);
  add_links(dd);
}
```

The new page needs a dictionary containing any link annotations, the media and
art boxes, the four fonts, and any images:

```c
// Prep the new page...
page_dict = pdfioDictCreate(dd->pdf);

dd->annots_array = pdfioArrayCreate(dd->pdf);
dd->annots_obj   = pdfioFileCreateArrayObj(dd->pdf, dd->annots_array);
pdfioDictSetObj(page_dict, "Annots", dd->annots_obj);

pdfioDictSetRect(page_dict, "MediaBox", &dd->media_box);
pdfioDictSetRect(page_dict, "ArtBox", &dd->art_box);

for (fontface = DOCFONT_REGULAR; fontface < DOCFONT_MAX; fontface ++)
  pdfioPageDictAddFont(page_dict, docfont_names[fontface], dd->fonts[fontface]);

for (i = 0; i < dd->num_images; i ++)
  pdfioPageDictAddImage(page_dict, pdfioStringCreatef(dd->pdf, "I%u", (unsigned)i),
                        dd->images[i].obj);
```

Once the page dictionary is initialized, we create a new page and initialize
the current graphics state:

```c
dd->st    = pdfioFileCreatePage(dd->pdf, page_dict);
dd->color = DOCCOLOR_BLACK;
dd->font  = DOCFONT_MAX;
dd->fsize = 0.0;
dd->y     = dd->art_box.y2;
```

The header consists of a dark gray separating line and the document title.  We
don't show the header on the first page:

```c
// Add header/footer text
set_color(dd, DOCCOLOR_GRAY);
set_font(dd, DOCFONT_REGULAR, SIZE_HEADFOOT);

if (pdfioFileGetNumPages(dd->pdf) > 1 && dd->title)
{
  // Show title in header...
  width = pdfioContentTextMeasure(dd->fonts[DOCFONT_REGULAR], dd->title,
                                  SIZE_HEADFOOT);

  pdfioContentTextBegin(dd->st);
  pdfioContentTextMoveTo(dd->st,
                         dd->crop_box.x1 + 0.5 * (dd->crop_box.x2 -
                             dd->crop_box.x1 - width),
                         dd->crop_box.y2 - SIZE_HEADFOOT);
  pdfioContentTextShow(dd->st, UNICODE_VALUE, dd->title);
  pdfioContentTextEnd(dd->st);

  pdfioContentPathMoveTo(dd->st, dd->crop_box.x1,
                         dd->crop_box.y2 - 2 * SIZE_HEADFOOT * LINE_HEIGHT +
                             SIZE_HEADFOOT);
  pdfioContentPathLineTo(dd->st, dd->crop_box.x2,
                         dd->crop_box.y2 - 2 * SIZE_HEADFOOT * LINE_HEIGHT +
                             SIZE_HEADFOOT);
  pdfioContentStroke(dd->st);
}
```

The footer contains the same dark gray separating line with the current heading
and page number on opposite sides.  The page number is always positioned on the
outer edge for a two-sided print - right justified on odd numbered pages and
left justified on even numbered pages:

```c
// Show page number and current heading...
pdfioContentPathMoveTo(dd->st, dd->crop_box.x1,
                       dd->crop_box.y1 + SIZE_HEADFOOT * LINE_HEIGHT);
pdfioContentPathLineTo(dd->st, dd->crop_box.x2,
                       dd->crop_box.y1 + SIZE_HEADFOOT * LINE_HEIGHT);
pdfioContentStroke(dd->st);

pdfioContentTextBegin(dd->st);
snprintf(temp, sizeof(temp), "%u", (unsigned)pdfioFileGetNumPages(dd->pdf));
if (pdfioFileGetNumPages(dd->pdf) & 1)
{
  // Page number on right...
  width = pdfioContentTextMeasure(dd->fonts[DOCFONT_REGULAR], temp, SIZE_HEADFOOT);
  pdfioContentTextMoveTo(dd->st, dd->crop_box.x2 - width, dd->crop_box.y1);
}
else
{
  // Page number on left...
  pdfioContentTextMoveTo(dd->st, dd->crop_box.x1, dd->crop_box.y1);
}

pdfioContentTextShow(dd->st, UNICODE_VALUE, temp);
pdfioContentTextEnd(dd->st);

if (dd->heading)
{
  pdfioContentTextBegin(dd->st);

  if (pdfioFileGetNumPages(dd->pdf) & 1)
  {
    // Current heading on left...
    pdfioContentTextMoveTo(dd->st, dd->crop_box.x1, dd->crop_box.y1);
  }
  else
  {
    width = pdfioContentTextMeasure(dd->fonts[DOCFONT_REGULAR], dd->heading,
                                    SIZE_HEADFOOT);
    pdfioContentTextMoveTo(dd->st, dd->crop_box.x2 - width, dd->crop_box.y1);
  }

  pdfioContentTextShow(dd->st, UNICODE_VALUE, dd->heading);
  pdfioContentTextEnd(dd->st);
}
```


### Formatting the Markdown Document

Four functions handle the formatting of the markdown document:

- `format_block` formats a single paragraph, heading, or table cell,
- `format_code`: formats a block of code,
- `format_doc`: formats the document as a whole, and
- `format_table`: formats a table.

Formatted content is organized into arrays of `linefrag_t` and `tablerow_t`
structures for a line of content or row of table cells, respectively.


#### High-Level Formatting

The `format_doc` function iterates over the block nodes in the markdown
document.  We map a "thematic break" (horizontal rule) to a page break, which
is implemented by moving the current vertical position to the bottom of the
page:

```c
case MMD_TYPE_THEMATIC_BREAK :
    // Force a page break
    dd->y = dd->art_box.y1;
    break;
```

A block quote is indented and uses the italic font by default:

```c
case MMD_TYPE_BLOCK_QUOTE :
    format_doc(dd, current, DOCFONT_ITALIC, left + BQ_PADDING, right - BQ_PADDING);
    break;
```

Lists have a leading blank line and are indented:

```c
case MMD_TYPE_ORDERED_LIST :
case MMD_TYPE_UNORDERED_LIST :
    if (dd->st)
      dd->y -= SIZE_BODY * LINE_HEIGHT;

    format_doc(dd, current, deffont, left + LIST_PADDING, right);
    break;
```

List items do not have a leading blank line and make use of leader text that is
shown in front of the list text.  The leader text is either the current item
number or a bullet, which then is directly formatted using the `format_block`
function:

```c
case MMD_TYPE_LIST_ITEM :
    if (doctype == MMD_TYPE_ORDERED_LIST)
    {
      snprintf(leader, sizeof(leader), "%d. ", i);
      format_block(dd, current, deffont, SIZE_BODY, left, right, leader);
    }
    else
    {
      format_block(dd, current, deffont, SIZE_BODY, left, right, /*leader*/"• ");
    }
    break;
```

Paragraphs have a leading blank line and are likewise directly formatted:

```c
case MMD_TYPE_PARAGRAPH :
    // Add a blank line before the paragraph...
    dd->y -= SIZE_BODY * LINE_HEIGHT;

    // Format the paragraph...
    format_block(dd, current, deffont, SIZE_BODY, left, right, /*leader*/NULL);
    break;
```

Tables have a leading blank line and are formatted using the `format_table`
function:

```c
case MMD_TYPE_TABLE :
    // Add a blank line before the paragraph...
    dd->y -= SIZE_BODY * LINE_HEIGHT;

    // Format the table...
    format_table(dd, current, left, right);
    break;
```

Code blocks have a leading blank line, are indented slightly (to account for the
padded background), and are formatted using the `format_code` function:

```c
case MMD_TYPE_CODE_BLOCK :
    // Add a blank line before the code block...
    dd->y -= SIZE_BODY * LINE_HEIGHT;

    // Format the code block...
    format_code(dd, current, left + CODE_PADDING, right - CODE_PADDING);
    break;
```

Headings get some extra processing.  First, the current heading is remembered in
the `docdata_t` structure so it can be used in the page footer:

```c
case MMD_TYPE_HEADING_1 :
case MMD_TYPE_HEADING_2 :
case MMD_TYPE_HEADING_3 :
case MMD_TYPE_HEADING_4 :
case MMD_TYPE_HEADING_5 :
case MMD_TYPE_HEADING_6 :
    // Update the current heading
    free(dd->heading);
    dd->heading = mmdCopyAllText(current);
```

Then we add a blank line and format the heading with the boldface font at a
larger size using the `format_block` function:

```c
    // Add a blank line before the heading...
    dd->y -= heading_sizes[curtype - MMD_TYPE_HEADING_1] * LINE_HEIGHT;

    // Format the heading...
    format_block(dd, current, DOCFONT_BOLD,
                 heading_sizes[curtype - MMD_TYPE_HEADING_1], left, right,
                 /*leader*/NULL);
```

Once the heading is formatted, we record it in the `toc` array as a PDF outline
item object/dictionary:

```c
    // Add the heading to the table-of-contents...
    if (dd->num_toc < DOCTOC_MAX)
    {
      doctoc_t *t = dd->toc + dd->num_toc;
                                  // New TOC
      pdfio_array_t *dest;	// Destination array

      t->level = curtype - MMD_TYPE_HEADING_1;
      t->dict  = pdfioDictCreate(dd->pdf);
      t->obj   = pdfioFileCreateObj(dd->pdf, t->dict);
      dest     = pdfioArrayCreate(dd->pdf);

      pdfioArrayAppendObj(dest,
          pdfioFileGetPage(dd->pdf, pdfioFileGetNumPages(dd->pdf) - 1));
      pdfioArrayAppendName(dest, "XYZ");
      pdfioArrayAppendNumber(dest, PAGE_LEFT);
      pdfioArrayAppendNumber(dest,
          dd->y + heading_sizes[curtype - MMD_TYPE_HEADING_1] * LINE_HEIGHT);
      pdfioArrayAppendNumber(dest, 0.0);

      pdfioDictSetArray(t->dict, "Dest", dest);
      pdfioDictSetString(t->dict, "Title", pdfioStringCreate(dd->pdf, dd->heading));

      dd->num_toc ++;
    }
```

Finally, we also save the heading's target name and its location in the
`targets` array to allow interior links to work:

```c
    // Add the heading to the list of link targets...
    if (dd->num_targets < DOCTARGET_MAX)
    {
      doctarget_t *t = dd->targets + dd->num_targets;
                                  // New target

      make_target_name(t->name, dd->heading, sizeof(t->name));
      t->page = pdfioFileGetNumPages(dd->pdf) - 1;
      t->y    = dd->y + heading_sizes[curtype - MMD_TYPE_HEADING_1] * LINE_HEIGHT;

      dd->num_targets ++;
    }
    break;
```


#### Formatting Paragraphs, Headings, List Items, and Table Cells

Paragraphs, headings, list items, and table cells all use the same basic
formatting algorithm.  Text, checkboxes, and images are collected until the
nodes in the current block are used up or the content reaches the right margin.

In order to keep adjacent blocks of text together, the formatting algorithm
makes sure that at least 3 lines of text can fit before the bottom edge of the
page:

```c
if (mmdGetNextSibling(block))
  need_bottom = 3.0 * SIZE_BODY * LINE_HEIGHT;
else
  need_bottom = 0.0;
```

Leader text (used for list items) is right justified to the left margin and
becomes the first fragment on the line when present.

```c
if (leader)
{
  // Add leader text on first line...
  frags[0].type     = MMD_TYPE_NORMAL_TEXT;
  frags[0].width    = pdfioContentTextMeasure(dd->fonts[deffont], leader, fsize);
  frags[0].height   = fsize;
  frags[0].x        = left - frags[0].width;
  frags[0].imagenum = 0;
  frags[0].text     = leader;
  frags[0].url      = NULL;
  frags[0].ws       = false;
  frags[0].font     = deffont;
  frags[0].color    = DOCCOLOR_BLACK;

  num_frags  = 1;
  lineheight = fsize * LINE_HEIGHT;
}
else
{
  // No leader text...
  num_frags  = 0;
  lineheight = 0.0;
}

frag = frags + num_frags;
```

If the current content fragment won't fit, we call `render_line` to draw what we
have, adjusting the left margin as needed for table cells:

```c
  // See if this node will fit on the current line...
  if ((num_frags > 0 && (x + width + wswidth) >= right) || num_frags == LINEFRAG_MAX)
  {
    // No, render this line and start over...
    if (blocktype == MMD_TYPE_TABLE_HEADER_CELL ||
        blocktype == MMD_TYPE_TABLE_BODY_CELL_CENTER)
      margin_left = 0.5 * (right - x);
    else if (blocktype == MMD_TYPE_TABLE_BODY_CELL_RIGHT)
      margin_left = right - x;
    else
      margin_left = 0.0;

    render_line(dd, margin_left, need_bottom, lineheight, num_frags, frags);

    num_frags   = 0;
    frag        = frags;
    x           = left;
    lineheight  = 0.0;
    need_bottom = 0.0;
```

Block quotes (blocks use a default font of italic) have an orange bar to the
left of the block:

```c
    if (deffont == DOCFONT_ITALIC)
    {
      // Add an orange bar to the left of block quotes...
      set_color(dd, DOCCOLOR_ORANGE);
      pdfioContentSave(dd->st);
      pdfioContentSetLineWidth(dd->st, 3.0);
      pdfioContentPathMoveTo(dd->st, left - 6.0, dd->y - (LINE_HEIGHT - 1.0) * fsize);
      pdfioContentPathLineTo(dd->st, left - 6.0, dd->y + fsize);
      pdfioContentStroke(dd->st);
      pdfioContentRestore(dd->st);
    }
```

Finally, we add the current content fragment to the array:

```c
  // Add the current node to the fragment list
  if (num_frags == 0)
  {
    // No leading whitespace at the start of the line
    ws      = false;
    wswidth = 0.0;
  }

  frag->type       = type;
  frag->x          = x;
  frag->width      = width + wswidth;
  frag->height     = text ? fsize : height;
  frag->imagenum   = imagenum;
  frag->text       = text;
  frag->url        = url;
  frag->ws         = ws;
  frag->font       = font;
  frag->color      = color;

  num_frags ++;
  frag ++;
  x += width + wswidth;
  if (height > lineheight)
    lineheight = height;
```


#### Formatting Code Blocks

Code blocks consist of one or more lines of plain monospaced text.  We draw a
light gray background behind each line with a small bit of padding at the top
and bottom:

```c
// Draw the top padding...
set_color(dd, DOCCOLOR_LTGRAY);
pdfioContentPathRect(dd->st, left - CODE_PADDING, dd->y + SIZE_CODEBLOCK,
                     right - left + 2.0 * CODE_PADDING, CODE_PADDING);
pdfioContentFillAndStroke(dd->st, false);

// Start a code text block...
set_font(dd, DOCFONT_MONOSPACE, SIZE_CODEBLOCK);
pdfioContentTextBegin(dd->st);
pdfioContentTextMoveTo(dd->st, left, dd->y);

for (code = mmdGetFirstChild(block); code; code = mmdGetNextSibling(code))
{
  set_color(dd, DOCCOLOR_LTGRAY);
  pdfioContentPathRect(dd->st, left - CODE_PADDING,
                       dd->y - (LINE_HEIGHT - 1.0) * SIZE_CODEBLOCK,
                       right - left + 2.0 * CODE_PADDING, lineheight);
  pdfioContentFillAndStroke(dd->st, false);

  set_color(dd, DOCCOLOR_RED);
  pdfioContentTextShow(dd->st, UNICODE_VALUE, mmdGetText(code));
  dd->y -= lineheight;

  if (dd->y < dd->art_box.y1)
  {
    // End the current text block...
    pdfioContentTextEnd(dd->st);

    // Start a new page...
    new_page(dd);
    set_font(dd, DOCFONT_MONOSPACE, SIZE_CODEBLOCK);

    dd->y -= lineheight;

    pdfioContentTextBegin(dd->st);
    pdfioContentTextMoveTo(dd->st, left, dd->y);
  }
}

// End the current text block...
pdfioContentTextEnd(dd->st);
dd->y += lineheight;

// Draw the bottom padding...
set_color(dd, DOCCOLOR_LTGRAY);
pdfioContentPathRect(dd->st, left - CODE_PADDING,
                     dd->y - CODE_PADDING - (LINE_HEIGHT - 1.0) * SIZE_CODEBLOCK,
                     right - left + 2.0 * CODE_PADDING, CODE_PADDING);
pdfioContentFillAndStroke(dd->st, false);
```


#### Formatting Tables

Tables are the most difficult to format.  We start by scanning the entire table
and measuring every cell with the `measure_cell` function:

```c
for (num_cols = 0, num_rows = 0, rowptr = rows, current = mmdGetFirstChild(table);
     current && num_rows < TABLEROW_MAX;
     current = next)
{
  next = mmd_walk_next(table, current);
  type = mmdGetType(current);

  if (type == MMD_TYPE_TABLE_ROW)
  {
    // Parse row...
    for (col = 0, current = mmdGetFirstChild(current);
         current && num_cols < TABLECOL_MAX;
         current = mmdGetNextSibling(current), col ++)
    {
      rowptr->cells[col] = current;

      measure_cell(dd, current, cols + col);

      if (col >= num_cols)
        num_cols = col + 1;
    }

    rowptr ++;
    num_rows ++;
  }
}
```

The `measure_cell` function also updates the minimum and maximum width needed
for each column.  To this we add the cell padding to compute the total table
width:

```c
// Figure out the width of each column...
for (col = 0, table_width = 0.0; col < num_cols; col ++)
{
  cols[col].max_width += 2.0 * TABLE_PADDING;

  table_width += cols[col].max_width;
  cols[col].width = cols[col].max_width;
}
```

If the calculated width is more than the available width, we need to adjust the
width of the columns.  The algorithm used here breaks the available width into
N equal-width columns - any columns wider than this will be scaled
proportionately.  This works out as two steps - one to calculate the the base
width of "narrow" columns and a second to distribute the remaining width amongst
the wider columns:

```c
format_width = right - left - 2.0 * TABLE_PADDING * num_cols;

if (table_width > format_width)
{
  // Content too wide, try scaling the widths...
  double      avg_width,              // Average column width
              base_width,             // Base width
              remaining_width,        // Remaining width
              scale_width;            // Width for scaling
  size_t      num_remaining_cols = 0; // Number of remaining columns

  // First mark any columns that are narrower than the average width...
  avg_width = format_width / num_cols;

  for (col = 0, base_width = 0.0, remaining_width = 0.0; col < num_cols; col ++)
  {
    if (cols[col].width > avg_width)
    {
      remaining_width += cols[col].width;
      num_remaining_cols ++;
    }
    else
    {
      base_width += cols[col].width;
    }
  }

  // Then proportionately distribute the remaining width to the other columns...
  format_width -= base_width;

  for (col = 0, table_width = 0.0; col < num_cols; col ++)
  {
    if (cols[col].width > avg_width)
      cols[col].width = cols[col].width * format_width / remaining_width;

    table_width += cols[col].width;
  }
}
```

Now that we have the widths of the columns, we can calculate the left and right
margins of each column for formatting the cell text:

```c
// Calculate the margins of each column in preparation for formatting
for (col = 0, x = left + TABLE_PADDING; col < num_cols; col ++)
{
  cols[col].left  = x;
  cols[col].right = x + cols[col].width;

  x += cols[col].width + 2.0 * TABLE_PADDING;
}
```

Then we re-measure the cells using the final column widths to determine the
height of each cell and row:

```c
// Calculate the height of each row and cell in preparation for formatting
for (row = 0, rowptr = rows; row < num_rows; row ++, rowptr ++)
{
  for (col = 0; col < num_cols; col ++)
  {
    height = measure_cell(dd, rowptr->cells[col], cols + col) + 2.0 * TABLE_PADDING;
    if (height > rowptr->height)
      rowptr->height = height;
  }
}
```

Finally, we render each row in the table:

```c
// Render each table row...
for (row = 0, rowptr = rows; row < num_rows; row ++, rowptr ++)
  render_row(dd, num_cols, cols, rowptr);
```


### Rendering the Markdown Document

The formatted content in arrays of `linefrag_t` and `tablerow_t` structures
are passed to the `render_line` and `render_row` functions respectively to
produce content in the PDF document.


#### Rendering a Line in a Paragraph, Heading, or Table Cell

The `render_line` function adds content from the `linefrag_t` array to a PDF
page.  It starts by determining whether a new page is needed:

```c
if (!dd->st)
{
  new_page(dd);
  margin_top = 0.0;
}

dd->y -= margin_top + lineheight;
if ((dd->y - need_bottom) < dd->art_box.y1)
{
  new_page(dd);

  dd->y -= lineheight;
}
```

We then loops through the fragments for the current line, drawing checkboxes,
images, and text as needed.  When a hyperlink is present, we add the link to the
`links` array in the `docdata_t` structure, mapping "@" and "@@" to an internal
link corresponding to the linked text:

```c
if (frag->url && dd->num_links < DOCLINK_MAX)
{
  doclink_t *l = dd->links + dd->num_links;
                                  // Pointer to this link record

  if (!strcmp(frag->url, "@"))
  {
    // Use mapped text as link target...
    char  targetlink[129];        // Targeted link

    targetlink[0] = '#';
    make_target_name(targetlink + 1, frag->text, sizeof(targetlink) - 1);

    l->url = pdfioStringCreate(dd->pdf, targetlink);
  }
  else if (!strcmp(frag->url, "@@"))
  {
    // Use literal text as anchor...
    l->url = pdfioStringCreatef(dd->pdf, "#%s", frag->text);
  }
  else
  {
    // Use URL as-is...
    l->url = frag->url;
  }

  l->box.x1 = frag->x;
  l->box.y1 = dd->y;
  l->box.x2 = frag->x + frag->width;
  l->box.y2 = dd->y + frag->height;

  dd->num_links ++;
}
```

These are later written as annotations in the `add_links` function.


#### Rendering a Table Row

The `render_row` function takes a row of cells and the corresponding column
definitions.  It starts by drawing the border boxes around body cells:

```c
if (mmdGetType(row->cells[0]) == MMD_TYPE_TABLE_HEADER_CELL)
{
  // Header row, no border...
  deffont = DOCFONT_BOLD;
}
else
{
  // Regular body row, add borders...
  deffont = DOCFONT_REGULAR;

  set_color(dd, DOCCOLOR_GRAY);
  pdfioContentPathRect(dd->st, cols[0].left - TABLE_PADDING, dd->y - row->height,
                       cols[num_cols - 1].right - cols[0].left +
                           2.0 * TABLE_PADDING, row->height);
  for (col = 1; col < num_cols; col ++)
  {
    pdfioContentPathMoveTo(dd->st, cols[col].left - TABLE_PADDING, dd->y);
    pdfioContentPathLineTo(dd->st, cols[col].left - TABLE_PADDING, dd->y - row->height);
  }
  pdfioContentStroke(dd->st);
}
```

Then it formats each cell using the `format_block` function described
previously.  The page `y` value is reset before formatting each cell:

```c
row_y = dd->y;

for (col = 0; col < num_cols; col ++)
{
  dd->y = row_y;

  format_block(dd, row->cells[col], deffont, SIZE_TABLE, cols[col].left,
               cols[col].right, /*leader*/NULL);
}

dd->y = row_y - row->height;
```
