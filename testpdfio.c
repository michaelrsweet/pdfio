//
// Test program for PDFio.
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pdfio-private.h"
#include "pdfio-content.h"
#include <math.h>


//
// Local functions...
//

static int	do_test_file(const char *filename);
static int	do_unit_tests(void);
static int	draw_image(pdfio_stream_t *st, const char *name, double x, double y, double w, double h, const char *label);
static bool	error_cb(pdfio_file_t *pdf, const char *message, bool *error);
static ssize_t	token_consume_cb(const char **s, size_t bytes);
static ssize_t	token_peek_cb(const char **s, char *buffer, size_t bytes);
static int	verify_image(pdfio_file_t *pdf, size_t number);
static int	write_color_patch(pdfio_stream_t *st, bool device);
static int	write_color_test(pdfio_file_t *pdf, int number, pdfio_obj_t *font);
static pdfio_obj_t *write_image_object(pdfio_file_t *pdf, _pdfio_predictor_t predictor);
static int	write_images(pdfio_file_t *pdf, int number, pdfio_obj_t *font);
static int	write_page(pdfio_file_t *pdf, int number, pdfio_obj_t *font, pdfio_obj_t *image);
static int	write_text(pdfio_file_t *pdf, int first_page, pdfio_obj_t *font, const char *filename);


//
// 'main()' - Main entry for test program.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  if (argc > 1)
  {
    int	i;				// Looping var

    for (i = 1; i < argc; i ++)
    {
      if (do_test_file(argv[i]))
        return (1);
    }
  }
  else
  {
    do_unit_tests();
  }

  return (0);
}


//
// 'do_test_file()' - Try loading a PDF file and listing pages and objects.
//

static int				// O - Exit status
do_test_file(const char *filename)	// I - PDF filename
{
  bool		error = false;		// Have we shown an error yet?
  pdfio_file_t	*pdf;			// PDF file
  size_t	n,			// Object/page index
		num_objs,		// Number of objects
		num_pages;		// Number of pages
  pdfio_obj_t	*obj;			// Object
  pdfio_dict_t	*dict;			// Object dictionary
  const char	*type;			// Object type


  // Try opening the file...
  printf("pdfioFileOpen(\"%s\", ...): ", filename);
  if ((pdf = pdfioFileOpen(filename, (pdfio_error_cb_t)error_cb, &error)) != NULL)
  {
    puts("PASS");

    // Show basic stats...
    num_objs  = pdfioFileGetNumObjects(pdf);
    num_pages = pdfioFileGetNumPages(pdf);

    printf("    PDF %s, %d pages, %d objects.\n", pdfioFileGetVersion(pdf), (int)num_pages, (int)num_objs);

    // Show a summary of each page...
    for (n = 0; n < num_pages; n ++)
    {
      if ((obj = pdfioFileGetPage(pdf, n)) == NULL)
      {
	printf("%s: Unable to get page #%d.\n", filename, (int)n + 1);
      }
      else
      {
	pdfio_rect_t media_box;	// MediaBox value

	memset(&media_box, 0, sizeof(media_box));
	dict = pdfioObjGetDict(obj);

	if (!pdfioDictGetRect(dict, "MediaBox", &media_box))
	{
	  if ((obj = pdfioDictGetObject(dict, "Parent")) != NULL)
	  {
	    dict = pdfioObjGetDict(obj);
	    pdfioDictGetRect(dict, "MediaBox", &media_box);
	  }
	}

	printf("    Page #%d is %gx%g.\n", (int)n + 1, media_box.x2, media_box.y2);
      }
    }

    // Show the associated value with each object...
    for (n = 0; n < num_objs; n ++)
    {
      if ((obj = pdfioFileGetObject(pdf, n)) == NULL)
      {
	printf("    Unable to get object #%d.\n", (int)n);
      }
      else
      {
	dict = pdfioObjGetDict(obj);

	printf("    %u %u obj dict=%p(%lu pairs)\n", (unsigned)pdfioObjGetNumber(obj), (unsigned)pdfioObjGetGeneration(obj), dict, dict ? (unsigned long)dict->num_pairs : 0UL);
	fputs("        ", stdout);
	_pdfioValueDebug(&obj->value, stdout);
	putchar('\n');
      }
    }

    // Close the file and return success...
    pdfioFileClose(pdf);
    return (0);
  }
  else
  {
    // Error message will already be displayed so just indicate failure...
    return (1);
  }
}


//
// 'do_unit_tests()' - Do unit tests.
//

static int				// O - Exit status
do_unit_tests(void)
{
  int			i;		// Looping var
  char			filename[256];	// PDF filename
  pdfio_file_t		*pdf,		// Test PDF file
			*outpdf;	// Output PDF file
  bool			error = false;	// Error callback data
  _pdfio_token_t	tb;		// Token buffer
  const char		*s;		// String buffer
  _pdfio_value_t	value;		// Value
  pdfio_obj_t		*color_jpg,	// color.jpg image
			*gray_jpg,	// gray.jpg image
			*helvetica,	// Helvetica font
			*page;		// Page from test PDF file
  size_t		first_image,	// First image object
			num_pages;	// Number of pages written
  static const char	*complex_dict =	// Complex dictionary value
    "<</Annots 5457 0 R/Contents 5469 0 R/CropBox[0 0 595.4 842]/Group 725 0 R"
    "/MediaBox[0 0 595.4 842]/Parent 23513 0 R/Resources<</ColorSpace<<"
    "/CS0 21381 0 R/CS1 21393 0 R>>/ExtGState<</GS0 21420 0 R>>/Font<<"
    "/TT0 21384 0 R/TT1 21390 0 R/TT2 21423 0 R/TT3 21403 0 R/TT4 21397 0 R>>"
    "/ProcSet[/PDF/Text/ImageC]/Properties<</MC0 5472 0 R/MC1 5473 0 R>>"
    "/XObject<</E3Dp0QGN3h9EZL2X 23690 0 R/E6DU0TGl3s9NZT2C 23691 0 R"
    "/ENDB06GH3u9tZT2N 21391 0 R/ENDD0NGM339cZe2F 23692 0 R"
    "/ENDK00GK3c9DZN2n 23693 0 R/EPDB0NGN3Q9GZP2t 23695 0 R"
    "/EpDA0kG03o9rZX21 23696 0 R/Im0 5475 0 R>>>>/Rotate 0/StructParents 2105"
    "/Tabs/S/Type/Page>>";


  setbuf(stdout, NULL);

  // First open the test PDF file...
  fputs("pdfioFileOpen(\"testfiles/testpdfio.pdf\"): ", stdout);
  if ((pdf = pdfioFileOpen("testfiles/testpdfio.pdf", (pdfio_error_cb_t)error_cb, &error)) != NULL)
    puts("PASS");
  else
    return (1);

  // TODO: Test for known values in this test file.

  // Test the value parsers for edge cases...
  fputs("_pdfioValueRead(complex_dict): ", stdout);
  s = complex_dict;
  _pdfioTokenInit(&tb, pdf, (_pdfio_tconsume_cb_t)token_consume_cb, (_pdfio_tpeek_cb_t)token_peek_cb, &s);
  if (_pdfioValueRead(pdf, &tb, &value))
  {
    // TODO: Check value...
    puts("PASS");
  }
  else
    return (1);

  // Create a new PDF file...
  fputs("pdfioFileCreate(\"testpdfio-out.pdf\", ...): ", stdout);
  if ((outpdf = pdfioFileCreate("testpdfio-out.pdf", NULL, NULL, NULL, (pdfio_error_cb_t)error_cb, &error)) != NULL)
    puts("PASS");
  else
    return (1);

  // Create some image objects...
  fputs("pdfioFileCreateImageObject(\"testfiles/color.jpg\"): ", stdout);
  if ((color_jpg = pdfioFileCreateImageObject(outpdf, "testfiles/color.jpg", true)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioFileCreateImageObject(\"testfiles/gray.jpg\"): ", stdout);
  if ((gray_jpg = pdfioFileCreateImageObject(outpdf, "testfiles/gray.jpg", true)) != NULL)
    puts("PASS");
  else
    return (1);

  // Create fonts...
  fputs("pdfioFileCreateBaseFontObject(\"Helvetica\"): ", stdout);
  if ((helvetica = pdfioFileCreateBaseFontObject(outpdf, "Helvetica")) != NULL)
    puts("PASS");
  else
    return (1);

  // Copy the first page from the test PDF file...
  fputs("pdfioFileGetPage(0): ", stdout);
  if ((page = pdfioFileGetPage(pdf, 0)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageCopy(first page): ", stdout);
  if (pdfioPageCopy(outpdf, page))
    puts("PASS");
  else
    return (1);

  // Write a page with a color image...
  if (write_page(outpdf, 2, helvetica, color_jpg))
    return (1);

  // Copy the third page from the test PDF file...
  fputs("pdfioFileGetPage(2): ", stdout);
  if ((page = pdfioFileGetPage(pdf, 2)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageCopy(third page): ", stdout);
  if (pdfioPageCopy(outpdf, page))
    puts("PASS");
  else
    return (1);

  // Write a page with a grayscale image...
  if (write_page(outpdf, 4, helvetica, gray_jpg))
    return (1);

  // Write a page that tests multiple color spaces...
  if (write_color_test(outpdf, 5, helvetica))
    return (1);

  // Write a page with test images...
  first_image = pdfioFileGetNumObjects(outpdf);
  if (write_images(outpdf, 6, helvetica))
    return (1);

  // Print this text file...
  if (write_text(outpdf, 7, helvetica, "README.md"))
    return (1);

  // Close the test PDF file...
  fputs("pdfioFileClose(\"testfiles/testpdfio.pdf\"): ", stdout);
  if (pdfioFileClose(pdf))
    puts("PASS");
  else
    return (1);

  fputs("pdfioFileGetNumPages: ", stdout);
  if ((num_pages = pdfioFileGetNumPages(outpdf)) > 0)
  {
    printf("PASS (%lu)\n", (unsigned long)num_pages);
  }
  else
  {
    puts("FAIL");
    return (1);
  }

  // Close the new PDF file...
  fputs("pdfioFileClose(\"testpdfio-out.pdf\"): ", stdout);
  if (pdfioFileClose(outpdf))
    puts("PASS");
  else
    return (1);

  // Open the new PDF file to read it...
  fputs("pdfioFileOpen(\"testpdfio-out.pdf\", ...): ", stdout);
  if ((pdf = pdfioFileOpen("testpdfio-out.pdf", (pdfio_error_cb_t)error_cb, &error)) != NULL)
    puts("PASS");
  else
    return (1);

  // Verify the number of pages is the same...
  fputs("pdfioFileGetNumPages: ", stdout);
  if (num_pages == pdfioFileGetNumPages(pdf))
  {
    puts("PASS");
  }
  else
  {
    printf("FAIL (%lu != %lu)\n", (unsigned long)num_pages, (unsigned long)pdfioFileGetNumPages(pdf));
    return (1);
  }

  // Verify the images
  for (i = 0; i < 7; i ++)
    verify_image(pdf, (size_t)(first_image + i));

  // Close the new PDF file...
  fputs("pdfioFileClose(\"testpdfio-out.pdf\"): ", stdout);
  if (pdfioFileClose(pdf))
    puts("PASS");
  else
    return (1);

  return (0);
}


//
// 'draw_image()' - Draw an image with a label.
//

static int				// O - 1 on failure, 0 on success
draw_image(pdfio_stream_t *st,
           const char     *name,	// I - Name
           double         x,		// I - X offset
           double         y,		// I - Y offset
           double         w,		// I - Image width
           double         h,		// I - Image height
           const char     *label)	// I - Label
{
  printf("pdfioContentDrawImage(name=\"%s\", x=%g, y=%g, w=%g, h=%g): ", name, x, y, w, h);
  if (pdfioContentDrawImage(st, name, x, y, w, h))
    puts("PASS");
  else
    return (1);

  fputs("pdfioContentTextBegin(): ", stdout);
  if (pdfioContentTextBegin(st))
    puts("PASS");
  else
    return (1);

  fputs("pdfioContentSetTextFont(\"F1\", 18.0): ", stdout);
  if (pdfioContentSetTextFont(st, "F1", 18.0))
    puts("PASS");
  else
    return (1);

  printf("pdfioContentTextMoveTo(%g, %g): ", x, y + h + 9);
  if (pdfioContentTextMoveTo(st, x, y + h + 9))
    puts("PASS");
  else
    return (1);

  printf("pdfioContentTextShow(\"%s\"): ", label);
  if (pdfioContentTextShow(st, label))
    puts("PASS");
  else
    return (1);

  fputs("pdfioContentTextEnd(): ", stdout);
  if (pdfioContentTextEnd(st))
    puts("PASS");
  else
    return (1);

  return (0);
}


//
// 'error_cb()' - Display an error message during a unit test.
//

static bool				// O  - `true` to stop, `false` to continue
error_cb(pdfio_file_t *pdf,		// I  - PDF file
         const char   *message,		// I  - Error message
	 bool         *error)		// IO - Have we displayed an error? 
{
  (void)pdf;

  if (!*error)
  {
    // First error, so show a "FAIL" indicator
    *error = true;

    puts("FAIL");
  }

  // Indent error messages...
  printf("    %s\n", message);

  // Continue to catch more errors...
  return (false);
}


//
// 'token_consume_cb()' - Consume bytes from a test string.
//

static ssize_t				// O  - Number of bytes consumed
token_consume_cb(const char **s,	// IO - Test string
                 size_t     bytes)	// I  - Bytes to consume
{
  size_t	len = strlen(*s);	// Number of bytes remaining


  // "Consume" bytes by incrementing the string pointer, limiting to the
  // remaining length...
  if (bytes > len)
    bytes = len;

  *s += bytes;

  return ((ssize_t)bytes);
}


//
// 'token_peek_cb()' - Peek bytes from a test string.
//

static ssize_t				// O  - Number of bytes peeked
token_peek_cb(const char **s,		// IO - Test string
              char       *buffer,	// I  - Buffer
              size_t     bytes)		// I  - Bytes to peek
{
  size_t	len = strlen(*s);	// Number of bytes remaining


  // Copy bytes from the test string as possible...
  if (bytes > len)
    bytes = len;

  if (bytes > 0)
    memcpy(buffer, *s, bytes);

  return ((ssize_t)bytes);
}


//
// 'verify_image()' - Verify an image object.
//

static int				// O - 1 on failure, 0 on success
verify_image(pdfio_file_t *pdf,		// I - PDF file
             size_t       number)	// I - Object number
{
  pdfio_obj_t	*obj;			// Image object
  const char	*type,			// Object type
		*subtype;		// Object subtype
  double	width,			// Width of object
		height;			// Height of object
  pdfio_stream_t *st;			// Stream
  int		x, y;			// Coordinates in image
  unsigned char	buffer[768],		// Expected data
		*bufptr,		// Pointer into buffer
		line[768];		// Line from file
  ssize_t	bytes;			// Bytes read from stream


  printf("pdfioFileFindObject(%lu): ", (unsigned long)number);
  if ((obj = pdfioFileFindObject(pdf, number)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioObjGetType: ", stdout);
  if ((type = pdfioObjGetType(obj)) != NULL && !strcmp(type, "XObject"))
  {
    puts("PASS");
  }
  else
  {
    printf("FAIL (got %s, expected XObject)\n", type);
    return (1);
  }

  fputs("pdfioObjGetSubtype: ", stdout);
  if ((subtype = pdfioObjGetSubtype(obj)) != NULL && !strcmp(subtype, "Image"))
  {
    puts("PASS");
  }
  else
  {
    printf("FAIL (got %s, expected Image)\n", subtype);
    return (1);
  }

  fputs("pdfioImageGetWidth: ", stdout);
  if ((width = pdfioImageGetWidth(obj)) == 256.0)
  {
    puts("PASS");
  }
  else
  {
    printf("FAIL (got %g, expected 256)\n", width);
    return (1);
  }

  fputs("pdfioImageGetHeight: ", stdout);
  if ((height = pdfioImageGetHeight(obj)) == 256.0)
  {
    puts("PASS");
  }
  else
  {
    printf("FAIL (got %g, expected 256)\n", height);
    return (1);
  }

  // Open the image stream, read the image, and verify it matches expectations...
  fputs("pdfioObjOpenStream: ", stdout);
  if ((st = pdfioObjOpenStream(obj, PDFIO_FILTER_FLATE)) != NULL)
    puts("PASS");
  else
    return (1);

  for (y = 0; y < 256; y ++)
  {
    for (x = 0, bufptr = buffer; x < 256; x ++, bufptr += 3)
    {
      bufptr[0] = y;
      bufptr[1] = y + x;
      bufptr[2] = y - x;
    }

    if ((bytes = pdfioStreamRead(st, line, sizeof(line))) != (ssize_t)sizeof(line))
    {
      printf("pdfioStreamRead: FAIL (got %d for line %d, expected 768)\n", y, (int)bytes);
      pdfioStreamClose(st);
      return (1);
    }

    if (memcmp(buffer, line, sizeof(buffer)))
    {
      printf("pdfioStreamRead: FAIL (line %d doesn't match expectations)\n", y);
      pdfioStreamClose(st);
      return (1);
    }
  }

  pdfioStreamClose(st);

  return (0);
}


//
// 'write_color_patch()' - Write a color patch...
//

static int				// O - 1 on failure, 0 on success
write_color_patch(pdfio_stream_t *st,	// I - Content stream
                  bool           device)// I - Use device color?
{
  int		col,			// Current column
		row;			// Current row
  double	x, y,			// Relative position
		r,			// Radius
		hue,			// Hue angle
		sat,			// Saturation
		cc,			// Computed color
		red,			// Red value
		green,			// Green value
		blue;			// Blue value


  // Draw an 11x11 patch...
  for (col = 0; col < 21; col ++)
  {
    for (row = 0; row < 21; row ++)
    {
      // Compute color in patch...
      x = 0.1 * (col - 10);
      y = 0.1 * (row - 10);
      r = sqrt(x * x + y * y);

      if (r == 0.0)
      {
        red = green = blue = 1.0;
      }
      else
      {
	sat = pow(r, 1.5);

        x   /= r;
        y   /= r;
	hue = 3.0 * atan2(y, x) / M_PI;
	if (hue < 0.0)
	  hue += 6.0;

	cc = sat * (1.0 - fabs(fmod(hue, 2.0) - 1.0)) + 1.0 - sat;

	if (hue < 1.0)
	{
	  red   = 1.0;
	  green = cc;
	  blue  = 1.0 - sat;
	}
	else if (hue < 2.0)
	{
	  red   = cc;
	  green = 1.0;
	  blue  = 1.0 - sat;
	}
	else if (hue < 3.0)
	{
	  red   = 1.0 - sat;
	  green = 1.0;
	  blue  = cc;
	}
	else if (hue < 4.0)
	{
	  red   = 1.0 - sat;
	  green = cc;
	  blue  = 1.0;
	}
	else if (hue < 5.0)
	{
	  red   = cc;
	  green = 1.0 - sat;
	  blue  = 1.0;
	}
	else
	{
	  red   = 1.0;
	  green = 1.0 - sat;
	  blue  = cc;
	}
      }

      // Draw it...
      if (device)
      {
        // Use device CMYK color instead of a calibrated RGB space...
        double cyan = 1.0 - red;	// Cyan color
        double magenta = 1.0 - green;	// Magenta color
        double yellow = 1.0 - blue;	// Yellow color
        double black = cyan;		// Black color

        if (black > magenta)
          black = magenta;
        if (black > yellow)
          black = yellow;

        cyan    -= black;
        magenta -= black;
        yellow  -= black;

	printf("pdfioContentSetFillColorDeviceCMYK(c=%g, m=%g, y=%g, k=%g): ", cyan, magenta, yellow, black);
	if (pdfioContentSetFillColorDeviceCMYK(st, cyan, magenta, yellow, black))
	  puts("PASS");
	else
	  return (1);
      }
      else
      {
        // Use calibrate RGB space...
	printf("pdfioContentSetFillColorRGB(r=%g, g=%g, b=%g): ", red, green, blue);
	if (pdfioContentSetFillColorRGB(st, red, green, blue))
	  puts("PASS");
	else
	  return (1);
      }

      printf("pdfioContentPathRect(x=%g, y=%g, w=%g, h=%g): ", col * 9.0, row * 9.0, 9.0, 9.0);
      if (pdfioContentPathRect(st, col * 9.0, row * 9.0, 9.0, 9.0))
	puts("PASS");
      else
	return (1);

      fputs("pdfioContentFill(even_odd=false): ", stdout);
      if (pdfioContentFill(st, false))
	puts("PASS");
      else
	return (1);
    }
  }

  return (0);
}


//
// 'write_color_test()' - Write a color test page...
//

static int				// O - 1 on failure, 0 on success
write_color_test(pdfio_file_t *pdf,	// I - PDF file
                 int          number,	// I - Page number
                 pdfio_obj_t  *font)	// I - Text font
{
  pdfio_dict_t		*dict;		// Page dictionary
  pdfio_stream_t	*st;		// Page contents stream
  pdfio_array_t		*cs;		// Color space array


  fputs("pdfioDictCreate: ", stdout);
  if ((dict = pdfioDictCreate(pdf)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioArrayCreateCalibratedColorFromMatrix(AdobeRGB): ", stdout);
  if ((cs = pdfioArrayCreateCalibratedColorFromMatrix(pdf, 3, pdfioAdobeRGBGamma, pdfioAdobeRGBMatrix, pdfioAdobeRGBWhitePoint)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddColorSpace(AdobeRGB): ", stdout);
  if (pdfioPageDictAddColorSpace(dict, "AdobeRGB", cs))
    puts("PASS");
  else
    return (1);

  fputs("pdfioArrayCreateCalibratedColorFromMatrix(DisplayP3): ", stdout);
  if ((cs = pdfioArrayCreateCalibratedColorFromMatrix(pdf, 3, pdfioDisplayP3Gamma, pdfioDisplayP3Matrix, pdfioDisplayP3WhitePoint)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddColorSpace(DisplayP3): ", stdout);
  if (pdfioPageDictAddColorSpace(dict, "DisplayP3", cs))
    puts("PASS");
  else
    return (1);

  fputs("pdfioArrayCreateCalibratedColorFromMatrix(sRGB): ", stdout);
  if ((cs = pdfioArrayCreateCalibratedColorFromMatrix(pdf, 3, pdfioSRGBGamma, pdfioSRGBMatrix, pdfioSRGBWhitePoint)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddColorSpace(sRGB): ", stdout);
  if (pdfioPageDictAddColorSpace(dict, "sRGB", cs))
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddFont(F1): ", stdout);
  if (pdfioPageDictAddFont(dict, "F1", font))
    puts("PASS");
  else
    return (1);

  printf("pdfioFileCreatePage(%d): ", number);

  if ((st = pdfioFileCreatePage(pdf, dict)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioContentSetFillColorDeviceGray(0.0): ", stdout);
  if (pdfioContentSetFillColorDeviceGray(st, 0.0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextBegin(): ", stdout);
  if (pdfioContentTextBegin(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSetTextFont(\"F1\", 12.0): ", stdout);
  if (pdfioContentSetTextFont(st, "F1", 12.0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(550.0, 36.0): ", stdout);
  if (pdfioContentTextMoveTo(st, 550.0, 36.0))
    puts("PASS");
  else
    goto error;

  printf("pdfioContentTextShowf(\"%d\"): ", number);
  if (pdfioContentTextShowf(st, "%d", number))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextEnd(): ", stdout);
  if (pdfioContentTextEnd(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextBegin(): ", stdout);
  if (pdfioContentTextBegin(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSetTextFont(\"F1\", 18.0): ", stdout);
  if (pdfioContentSetTextFont(st, "F1", 18.0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(82, 360): ", stdout);
  if (pdfioContentTextMoveTo(st, 82, 360))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextShow(\"AdobeRGB\"): ", stdout);
  if (pdfioContentTextShow(st, "AdobeRGB"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(234, 0): ", stdout);
  if (pdfioContentTextMoveTo(st, 234, 0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextShow(\"DisplayP3\"): ", stdout);
  if (pdfioContentTextShow(st, "DisplayP3"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(-234, 252): ", stdout);
  if (pdfioContentTextMoveTo(st, -234, 252))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextShow(\"sRGB\"): ", stdout);
  if (pdfioContentTextShow(st, "sRGB"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(234, 0): ", stdout);
  if (pdfioContentTextMoveTo(st, 234, 0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextShow(\"DeviceCMYK\"): ", stdout);
  if (pdfioContentTextShow(st, "DeviceCMYK"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextEnd(): ", stdout);
  if (pdfioContentTextEnd(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSave(): ", stdout);
  if (pdfioContentSave(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSetFillColorSpace(AdobeRGB): ", stdout);
  if (pdfioContentSetFillColorSpace(st, "AdobeRGB"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentMatrixTranslate(82, 162): ", stdout);
  if (pdfioContentMatrixTranslate(st, 82, 162))
    puts("PASS");
  else
    goto error;

  if (write_color_patch(st, false))
    goto error;

  fputs("pdfioContentRestore(): ", stdout);
  if (pdfioContentRestore(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSave(): ", stdout);
  if (pdfioContentSave(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSetFillColorSpace(DisplayP3): ", stdout);
  if (pdfioContentSetFillColorSpace(st, "DisplayP3"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentMatrixTranslate(316, 162): ", stdout);
  if (pdfioContentMatrixTranslate(st, 316, 162))
    puts("PASS");
  else
    goto error;

  if (write_color_patch(st, false))
    goto error;

  fputs("pdfioContentRestore(): ", stdout);
  if (pdfioContentRestore(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSave(): ", stdout);
  if (pdfioContentSave(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSetFillColorSpace(sRGB): ", stdout);
  if (pdfioContentSetFillColorSpace(st, "sRGB"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentMatrixTranslate(82, 414): ", stdout);
  if (pdfioContentMatrixTranslate(st, 82, 414))
    puts("PASS");
  else
    goto error;

  if (write_color_patch(st, false))
    goto error;

  fputs("pdfioContentRestore(): ", stdout);
  if (pdfioContentRestore(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSave(): ", stdout);
  if (pdfioContentSave(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentMatrixTranslate(316, 414): ", stdout);
  if (pdfioContentMatrixTranslate(st, 316, 414))
    puts("PASS");
  else
    goto error;

  if (write_color_patch(st, true))
    goto error;

  fputs("pdfioContentRestore(): ", stdout);
  if (pdfioContentRestore(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioStreamClose: ", stdout);
  if (pdfioStreamClose(st))
    puts("PASS");
  else
    return (1);

  return (0);

  error:

  pdfioStreamClose(st);
  return (1);
}


//
// 'write_image_object()' - Write an image object using the specified predictor.
//

static pdfio_obj_t *			// O - Image object
write_image_object(
    pdfio_file_t       *pdf,		// I - PDF file
    _pdfio_predictor_t predictor)	// I - Predictor to use
{
  pdfio_dict_t	*dict,			// Image dictionary
		*decode;		// Decode dictionary
  pdfio_obj_t	*obj;			// Image object
  pdfio_stream_t *st;			// Image stream
  int		x, y;			// Coordinates in image
  unsigned char	buffer[768],		// Buffer for image
		*bufptr;		// Pointer into buffer


  // Create the image dictionary...
  if ((dict = pdfioDictCreate(pdf)) == NULL)
    return (NULL);

  pdfioDictSetName(dict, "Type", "XObject");
  pdfioDictSetName(dict, "Subtype", "Image");
  pdfioDictSetNumber(dict, "Width", 256);
  pdfioDictSetNumber(dict, "Height", 256);
  pdfioDictSetNumber(dict, "BitsPerComponent", 8);
  pdfioDictSetName(dict, "ColorSpace", "DeviceRGB");
  pdfioDictSetName(dict, "Filter", "FlateDecode");

  if ((decode = pdfioDictCreate(pdf)) == NULL)
    return (NULL);

  pdfioDictSetNumber(decode, "BitsPerComponent", 8);
  pdfioDictSetNumber(decode, "Colors", 3);
  pdfioDictSetNumber(decode, "Columns", 256);
  pdfioDictSetNumber(decode, "Predictor", predictor);
  pdfioDictSetDict(dict, "DecodeParms", decode);

  // Create the image object...
  if ((obj = pdfioFileCreateObject(pdf, dict)) == NULL)
    return (NULL);

  // Create the image stream and write the image...
  if ((st = pdfioObjCreateStream(obj, PDFIO_FILTER_FLATE)) == NULL)
    return (NULL);

  for (y = 0; y < 256; y ++)
  {
    for (x = 0, bufptr = buffer; x < 256; x ++, bufptr += 3)
    {
      bufptr[0] = y;
      bufptr[1] = y + x;
      bufptr[2] = y - x;
    }

    if (!pdfioStreamWrite(st, buffer, sizeof(buffer)))
    {
      pdfioStreamClose(st);
      return (NULL);
    }
  }

  pdfioStreamClose(st);

  return (obj);
}


//
// 'write_images()' - Write a series of test images.
//

static int				// O - 1 on failure, 0 on success
write_images(pdfio_file_t *pdf,		// I - PDF file
             int          number,	// I - Page number
             pdfio_obj_t  *font)	// I - Text font
{
  pdfio_dict_t	*dict;			// Page dictionary
  pdfio_stream_t *st;			// Page stream
  _pdfio_predictor_t p;			// Current predictor
  pdfio_obj_t	*noimage,		// No predictor
		*pimages[6];		// Images using PNG predictors
  char		pname[32],		// Image name
		plabel[32];		// Image label


  // Create the images...
  fputs("Create Image (Predictor 1): ", stdout);
  if ((noimage = write_image_object(pdf, _PDFIO_PREDICTOR_NONE)) != NULL)
    puts("PASS");
  else
    return (1);

  for (p = _PDFIO_PREDICTOR_PNG_NONE; p <= _PDFIO_PREDICTOR_PNG_AUTO; p ++)
  {
    printf("Create Image (Predictor %d): ", p);
    if ((pimages[p - _PDFIO_PREDICTOR_PNG_NONE] = write_image_object(pdf, p)) != NULL)
      puts("PASS");
    else
      return (1);
  }

  // Create the page dictionary, object, and stream...
  fputs("pdfioDictCreate: ", stdout);
  if ((dict = pdfioDictCreate(pdf)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddImage(1): ", stdout);
  if (pdfioPageDictAddImage(dict, "IM1", noimage))
    puts("PASS");
  else
    return (1);

  for (p = _PDFIO_PREDICTOR_PNG_NONE; p <= _PDFIO_PREDICTOR_PNG_AUTO; p ++)
  {
    printf("pdfioPageDictAddImage(%d): ", p);
    snprintf(pname, sizeof(pname), "IM%d", p);
    if (pdfioPageDictAddImage(dict, pdfioStringCreate(pdf, pname), pimages[p - _PDFIO_PREDICTOR_PNG_NONE]))
      puts("PASS");
    else
      return (1);
  }

  fputs("pdfioPageDictAddFont(F1): ", stdout);
  if (pdfioPageDictAddFont(dict, "F1", font))
    puts("PASS");
  else
    return (1);

  printf("pdfioFileCreatePage(%d): ", number);

  if ((st = pdfioFileCreatePage(pdf, dict)) != NULL)
    puts("PASS");
  else
    return (1);

  // Show content...
  fputs("pdfioContentSetFillColorDeviceGray(0.0): ", stdout);
  if (pdfioContentSetFillColorDeviceGray(st, 0.0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextBegin(): ", stdout);
  if (pdfioContentTextBegin(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSetTextFont(\"F1\", 12.0): ", stdout);
  if (pdfioContentSetTextFont(st, "F1", 12.0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(550.0, 36.0): ", stdout);
  if (pdfioContentTextMoveTo(st, 550.0, 36.0))
    puts("PASS");
  else
    goto error;

  printf("pdfioContentTextShowf(\"%d\"): ", number);
  if (pdfioContentTextShowf(st, "%d", number))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextEnd(): ", stdout);
  if (pdfioContentTextEnd(st))
    puts("PASS");
  else
    goto error;

  // Draw images
  if (draw_image(st, "IM1", 36, 558, 144, 144, "No Predictor"))
    goto error;

  for (p = _PDFIO_PREDICTOR_PNG_NONE; p <= _PDFIO_PREDICTOR_PNG_AUTO; p ++)
  {
    int i = p - _PDFIO_PREDICTOR_PNG_NONE;

    snprintf(pname, sizeof(pname), "IM%d", p);
    snprintf(plabel, sizeof(plabel), "PNG Predictor %d", p);

    if (draw_image(st, pname, 36 + 180 * (i % 3), 342 - 216 * (i / 3), 144, 144, plabel))
      goto error;
  }

  // Wrap up...
  fputs("pdfioStreamClose: ", stdout);
  if (pdfioStreamClose(st))
    puts("PASS");
  else
    return (1);

  return (0);

  error:

  pdfioStreamClose(st);
  return (1);
}


//
// 'write_page()' - Write a page to a PDF file.
//

static int				// O - 1 on failure, 0 on success
write_page(pdfio_file_t *pdf,		// I - PDF file
           int          number,		// I - Page number
           pdfio_obj_t  *font,		// I - Text font
           pdfio_obj_t  *image)		// I - Image to draw
{
  pdfio_dict_t		*dict;		// Page dictionary
  pdfio_stream_t	*st;		// Page contents stream
  double		width,		// Width of image
			height;		// Height of image
  double		swidth,		// Scaled width
			sheight,	// Scaled height
			tx,		// X offset
			ty;		// Y offset


  // Create the page dictionary, object, and stream...
  fputs("pdfioDictCreate: ", stdout);
  if ((dict = pdfioDictCreate(pdf)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddImage: ", stdout);
  if (pdfioPageDictAddImage(dict, "IM1", image))
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddFont(F1): ", stdout);
  if (pdfioPageDictAddFont(dict, "F1", font))
    puts("PASS");
  else
    return (1);

  printf("pdfioFileCreatePage(%d): ", number);

  if ((st = pdfioFileCreatePage(pdf, dict)) != NULL)
    puts("PASS");
  else
    return (1);

  // Show content...
  fputs("pdfioStreamPuts(...): ", stdout);
  if (pdfioStreamPuts(st,
                      "1 0 0 RG 0 g 5 w\n"
                      "54 54 487 688 re 90 90 415 612 re B*\n"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSetFillColorDeviceGray(0.0): ", stdout);
  if (pdfioContentSetFillColorDeviceGray(st, 0.0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextBegin(): ", stdout);
  if (pdfioContentTextBegin(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSetTextFont(\"F1\", 12.0): ", stdout);
  if (pdfioContentSetTextFont(st, "F1", 12.0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(550.0, 36.0): ", stdout);
  if (pdfioContentTextMoveTo(st, 550.0, 36.0))
    puts("PASS");
  else
    goto error;

  printf("pdfioContentTextShowf(\"%d\"): ", number);
  if (pdfioContentTextShowf(st, "%d", number))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextEnd(): ", stdout);
  if (pdfioContentTextEnd(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSave(): ", stdout);
  if (pdfioContentSave(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioImageGetWidth(): ", stdout);
  if ((width = pdfioImageGetWidth(image)) > 0.0)
    puts("PASS");
  else
    goto error;

  fputs("pdfioImageGetHeight(): ", stdout);
  if ((height = pdfioImageGetHeight(image)) > 0.0)
    puts("PASS");
  else
    goto error;

  swidth  = 400.0;
  sheight = swidth * height / width;
  if (sheight > 600.0)
  {
    sheight = 600.0;
    swidth  = sheight * width / height;
  }

  tx = 0.5 * (595.28 - swidth);
  ty = 0.5 * (792 - sheight);

  printf("pdfioContentDrawImage(\"IM1\", x=%g, y=%g, w=%g, h=%g): ", tx, ty, swidth, sheight);
  if (pdfioContentDrawImage(st, "IM1", tx, ty, swidth, sheight))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentRestore(): ", stdout);
  if (pdfioContentRestore(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioStreamClose: ", stdout);
  if (pdfioStreamClose(st))
    puts("PASS");
  else
    return (1);

  return (0);

  error:

  pdfioStreamClose(st);
  return (1);
}


//
// 'write_text()' - Print a plain text file.
//

static int				// O - 0 on success, 1 on failure
write_text(pdfio_file_t *pdf,		// I - PDF file
           int          first_page,	// I - First page number
           pdfio_obj_t  *font,		// I - Page number font
           const char   *filename)	// I - File to print
{
  pdfio_obj_t		*courier;	// Courier font
  pdfio_dict_t		*dict;		// Page dictionary
  FILE			*fp;		// Print file
  char			line[1024];	// Line from file
  int			page,		// Current page number
			plinenum,	// Current line number on page
			flinenum;	// Current line number in file
  pdfio_stream_t	*st = NULL;	// Page contents stream


  // Create text font...
  fputs("pdfioFileCreateBaseFontObject(\"Courier\"): ", stdout);
  if ((courier = pdfioFileCreateBaseFontObject(pdf, "Courier")) != NULL)
    puts("PASS");
  else
    return (1);

  // Create the page dictionary...
  fputs("pdfioDictCreate: ", stdout);
  if ((dict = pdfioDictCreate(pdf)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddFont(F1): ", stdout);
  if (pdfioPageDictAddFont(dict, "F1", font))
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddFont(F2): ", stdout);
  if (pdfioPageDictAddFont(dict, "F2", courier))
    puts("PASS");
  else
    return (1);

  // Open the print file...
  if ((fp = fopen(filename, "r")) == NULL)
  {
    printf("Unable to open \"%s\": %s\n", filename, strerror(errno));
    return (1);
  }

  page     = first_page;
  plinenum = 0;
  flinenum = 0;

  while (fgets(line, sizeof(line), fp))
  {
    flinenum ++;

    if (plinenum == 0)
    {
      printf("pdfioFileCreatePage(%d): ", page);

      if ((st = pdfioFileCreatePage(pdf, dict)) != NULL)
	puts("PASS");
      else
        goto error;

      // Show the page number
      fputs("pdfioContentSetFillColorDeviceGray(0.0): ", stdout);
      if (pdfioContentSetFillColorDeviceGray(st, 0.0))
	puts("PASS");
      else
	goto error;

      fputs("pdfioContentTextBegin(): ", stdout);
      if (pdfioContentTextBegin(st))
	puts("PASS");
      else
	goto error;

      fputs("pdfioContentSetTextFont(\"F1\", 12.0): ", stdout);
      if (pdfioContentSetTextFont(st, "F1", 12.0))
	puts("PASS");
      else
	goto error;

      fputs("pdfioContentTextMoveTo(36.0, 36.0): ", stdout);
      if (pdfioContentTextMoveTo(st, 36, 36.0))
	puts("PASS");
      else
	goto error;

      printf("pdfioContentTextShowf(\"\\\"%s\\\"\"): ", filename);
      if (pdfioContentTextShowf(st, "\"%s\"", filename))
	puts("PASS");
      else
	goto error;

      fputs("pdfioContentTextMoveTo(514.0, 0.0): ", stdout);
      if (pdfioContentTextMoveTo(st, 514.0, 0.0))
	puts("PASS");
      else
	goto error;

      printf("pdfioContentTextShowf(\"%d\"): ", page);
      if (pdfioContentTextShowf(st, "%d", page))
	puts("PASS");
      else
	goto error;

      fputs("pdfioContentTextEnd(): ", stdout);
      if (pdfioContentTextEnd(st))
	puts("PASS");
      else
	goto error;

      page ++;
      plinenum ++;

      fputs("pdfioContentTextBegin(): ", stdout);
      if (pdfioContentTextBegin(st))
	puts("PASS");
      else
	goto error;

      fputs("pdfioContentSetTextFont(\"F2\", 10.0): ", stdout);
      if (pdfioContentSetTextFont(st, "F2", 10.0))
	puts("PASS");
      else
	goto error;

      fputs("pdfioContentSetTextLeading(12.0): ", stdout);
      if (pdfioContentSetTextLeading(st, 12.0))
	puts("PASS");
      else
	goto error;

      fputs("pdfioContentTextMoveTo(36.0, 756.0): ", stdout);
      if (pdfioContentTextMoveTo(st, 36.0, 756.0))
	puts("PASS");
      else
	goto error;
    }

    if (!pdfioContentSetFillColorDeviceGray(st, 0.75))
      goto error;
    if (!pdfioContentTextShowf(st, "%4d  ", flinenum))
      goto error;
    if (!pdfioContentSetFillColorDeviceGray(st, 0.0))
      goto error;
    if (!pdfioContentTextShow(st, line))
      goto error;

    plinenum ++;
    if (plinenum >= 60)
    {
      fputs("pdfioContentTextEnd(): ", stdout);
      if (pdfioContentTextEnd(st))
	puts("PASS");
      else
	goto error;

      fputs("pdfioStreamClose: ", stdout);
      if (pdfioStreamClose(st))
	puts("PASS");
      else
	goto error;

      st       = NULL;
      plinenum = 0;
    }
  }

  if (plinenum > 0)
  {
    fputs("pdfioContentTextEnd(): ", stdout);
    if (pdfioContentTextEnd(st))
      puts("PASS");
    else
      goto error;

    fputs("pdfioStreamClose: ", stdout);
    if (pdfioStreamClose(st))
      puts("PASS");
    else
      return (1);
  }

  fclose(fp);

  return (0);

  error:

  fclose(fp);
  pdfioStreamClose(st);
  return (1);
}
