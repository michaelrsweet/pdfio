//
// Test program for PDFio.
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Usage:
//
//   ./testpdfio
//
//   ./testpdfio FILENAME [OBJECT-NUMBER] [FILENAME [OBJECT-NUMBER]] ...
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

static int	do_test_file(const char *filename, int objnum);
static int	do_unit_tests(void);
static int	draw_image(pdfio_stream_t *st, const char *name, double x, double y, double w, double h, const char *label);
static bool	error_cb(pdfio_file_t *pdf, const char *message, bool *error);
static ssize_t	token_consume_cb(const char **s, size_t bytes);
static ssize_t	token_peek_cb(const char **s, char *buffer, size_t bytes);
static int	verify_image(pdfio_file_t *pdf, size_t number);
static int	write_alpha_test(pdfio_file_t *pdf, int number, pdfio_obj_t *font);
static int	write_color_patch(pdfio_stream_t *st, bool device);
static int	write_color_test(pdfio_file_t *pdf, int number, pdfio_obj_t *font);
static int	write_font_test(pdfio_file_t *pdf, int number, pdfio_obj_t *font, bool unicode);
static int	write_header_footer(pdfio_stream_t *st, const char *title, int number);
static pdfio_obj_t *write_image_object(pdfio_file_t *pdf, _pdfio_predictor_t predictor);
static int	write_images_test(pdfio_file_t *pdf, int number, pdfio_obj_t *font);
static int	write_jpeg_test(pdfio_file_t *pdf, const char *title, int number, pdfio_obj_t *font, pdfio_obj_t *image);
static int	write_png_test(pdfio_file_t *pdf, int number, pdfio_obj_t *font);
static int	write_text_test(pdfio_file_t *pdf, int first_page, pdfio_obj_t *font, const char *filename);


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
      if ((i + 1) < argc && isdigit(argv[i + 1][0] & 255))
      {
        // filename.pdf object-number
        if (do_test_file(argv[i], atoi(argv[i + 1])))
	  return (1);

	i ++;
      }
      else if (do_test_file(argv[i], 0))
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
do_test_file(const char *filename,	// I - PDF filename
             int        objnum)		// I - Object number to dump, if any
{
  bool		error = false;		// Have we shown an error yet?
  pdfio_file_t	*pdf;			// PDF file
  size_t	n,			// Object/page index
		num_objs,		// Number of objects
		num_pages;		// Number of pages
  pdfio_obj_t	*obj;			// Object
  pdfio_dict_t	*dict;			// Object dictionary


  // Try opening the file...
  if (!objnum)
    printf("pdfioFileOpen(\"%s\", ...): ", filename);

  if ((pdf = pdfioFileOpen(filename, (pdfio_error_cb_t)error_cb, &error)) != NULL)
  {
    if (objnum)
    {
      const char	*filter;	// Stream filter
      pdfio_stream_t	*st;		// Stream
      char		buffer[8192];	// Read buffer
      ssize_t		bytes;		// Bytes read

      if ((obj = pdfioFileFindObj(pdf, (size_t)objnum)) == NULL)
      {
        puts("Not found.");
        return (1);
      }

      if ((dict = pdfioObjGetDict(obj)) == NULL)
      {
        puts("Not a stream.");
        return (1);
      }

      filter = pdfioDictGetName(dict, "Filter");

      if ((st = pdfioObjOpenStream(obj, (filter && !strcmp(filter, "FlateDecode")) ? PDFIO_FILTER_FLATE : PDFIO_FILTER_NONE)) == NULL)
        return (1);

      while ((bytes = pdfioStreamRead(st, buffer, sizeof(buffer))) > 0)
        fwrite(buffer, 1, (size_t)bytes, stdout);

      pdfioStreamClose(st);

      return (0);
    }
    else
    {
      puts("PASS");

      // Show basic stats...
      num_objs  = pdfioFileGetNumObjs(pdf);
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
	    if ((obj = pdfioDictGetObj(dict, "Parent")) != NULL)
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
	if ((obj = pdfioFileGetObj(pdf, n)) == NULL)
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

  // Set info values...
  fputs("pdfioFileGet/SetAuthor: ", stdout);
  pdfioFileSetAuthor(outpdf, "Michael R Sweet");
  if ((s = pdfioFileGetAuthor(outpdf)) != NULL && !strcmp(s, "Michael R Sweet"))
  {
    puts("PASS");
  }
  else if (s)
  {
    printf("FAIL (got '%s', expected 'Michael R Sweet')\n", s);
    return (1);
  }
  else
  {
    puts("FAIL (got NULL, expected 'Michael R Sweet')");
    return (1);
  }

  fputs("pdfioFileGet/SetCreator: ", stdout);
  pdfioFileSetCreator(outpdf, "testpdfio");
  if ((s = pdfioFileGetCreator(outpdf)) != NULL && !strcmp(s, "testpdfio"))
  {
    puts("PASS");
  }
  else if (s)
  {
    printf("FAIL (got '%s', expected 'testpdfio')\n", s);
    return (1);
  }
  else
  {
    puts("FAIL (got NULL, expected 'testpdfio')");
    return (1);
  }

  fputs("pdfioFileGet/SetKeywords: ", stdout);
  pdfioFileSetKeywords(outpdf, "one fish,two fish,red fish,blue fish");
  if ((s = pdfioFileGetKeywords(outpdf)) != NULL && !strcmp(s, "one fish,two fish,red fish,blue fish"))
  {
    puts("PASS");
  }
  else if (s)
  {
    printf("FAIL (got '%s', expected 'one fish,two fish,red fish,blue fish')\n", s);
    return (1);
  }
  else
  {
    puts("FAIL (got NULL, expected 'one fish,two fish,red fish,blue fish')");
    return (1);
  }

  fputs("pdfioFileGet/SetSubject: ", stdout);
  pdfioFileSetSubject(outpdf, "Unit test document");
  if ((s = pdfioFileGetSubject(outpdf)) != NULL && !strcmp(s, "Unit test document"))
  {
    puts("PASS");
  }
  else if (s)
  {
    printf("FAIL (got '%s', expected 'Unit test document')\n", s);
    return (1);
  }
  else
  {
    puts("FAIL (got NULL, expected 'Unit test document')");
    return (1);
  }

  fputs("pdfioFileGet/SetTitle: ", stdout);
  pdfioFileSetTitle(outpdf, "Test Document");
  if ((s = pdfioFileGetTitle(outpdf)) != NULL && !strcmp(s, "Test Document"))
  {
    puts("PASS");
  }
  else if (s)
  {
    printf("FAIL (got '%s', expected 'Test Document')\n", s);
    return (1);
  }
  else
  {
    puts("FAIL (got NULL, expected 'Test Document')");
    return (1);
  }

  // Create some image objects...
  fputs("pdfioFileCreateImageObjFromFile(\"testfiles/color.jpg\"): ", stdout);
  if ((color_jpg = pdfioFileCreateImageObjFromFile(outpdf, "testfiles/color.jpg", true)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioFileCreateImageObjFromFile(\"testfiles/gray.jpg\"): ", stdout);
  if ((gray_jpg = pdfioFileCreateImageObjFromFile(outpdf, "testfiles/gray.jpg", true)) != NULL)
    puts("PASS");
  else
    return (1);

  // Create fonts...
  fputs("pdfioFileCreateFontObjFromBase(\"Helvetica\"): ", stdout);
  if ((helvetica = pdfioFileCreateFontObjFromBase(outpdf, "Helvetica")) != NULL)
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
  if (write_jpeg_test(outpdf, "Color JPEG Test", 2, helvetica, color_jpg))
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
  if (write_jpeg_test(outpdf, "Grayscale JPEG Test", 4, helvetica, gray_jpg))
    return (1);

  // Write a page with PNG images...
  if (write_png_test(outpdf, 5, helvetica))
    return (1);

  // Write a page that tests multiple color spaces...
  if (write_color_test(outpdf, 6, helvetica))
    return (1);

  // Write a page with test images...
  first_image = pdfioFileGetNumObjs(outpdf) + 1;
  if (write_images_test(outpdf, 7, helvetica))
    return (1);

  // Write a page width alpha (soft masks)...
  if (write_alpha_test(outpdf, 8, helvetica))
    return (1);

  // Test TrueType fonts...
  if (write_font_test(outpdf, 9, helvetica, false))
    return (1);

  if (write_font_test(outpdf, 10, helvetica, true))
    return (1);

  // Print this text file...
  if (write_text_test(outpdf, 11, helvetica, "README.md"))
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
  {
    if (verify_image(pdf, first_image + (size_t)i))
      return (1);
  }

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
  if (pdfioContentTextShow(st, false, label))
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


  printf("pdfioFileFindObj(%lu): ", (unsigned long)number);
  if ((obj = pdfioFileFindObj(pdf, number)) != NULL)
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
      bufptr[0] = (unsigned char)y;
      bufptr[1] = (unsigned char)(y + x);
      bufptr[2] = (unsigned char)(y - x);
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
// 'write_alpha_test()' - Write a series of test images with alpha channels.
//

static int				// O - 1 on failure, 0 on success
write_alpha_test(
    pdfio_file_t *pdf,			// I - PDF file
    int          number,		// I - Page number
    pdfio_obj_t  *font)			// I - Text font
{
  pdfio_dict_t	*dict;			// Page dictionary
  pdfio_stream_t *st;			// Page stream
  pdfio_obj_t	*images[6];		// Images using PNG predictors
  char		iname[32];		// Image name
  int		i,			// Image number
		x, y;			// Coordinates in image
  unsigned char	buffer[1280 * 256],	// Buffer for image
		*bufptr;		// Pointer into buffer


  // Create the images...
  for (i = 0; i < 6; i ++)
  {
    size_t	num_colors = 0;		// Number of colors

    // Generate test image data...
    switch (i)
    {
      case 0 : // Grayscale
      case 3 : // Grayscale + alpha
          num_colors = 1;
	  for (y = 0, bufptr = buffer; y < 256; y ++)
	  {
	    for (x = 0; x < 256; x ++)
	    {
	      unsigned char r = (unsigned char)y;
	      unsigned char g = (unsigned char)(y + x);
	      unsigned char b = (unsigned char)(y - x);

              *bufptr++ = (unsigned char)((r * 30 + g * 59 + b * 11) / 100);

	      if (i > 2)
	      {
	        // Add alpha channel
	        *bufptr++ = (unsigned char)((x + y) / 2);
	      }
	    }
	  }
          break;

      case 1 : // RGB
      case 4 : // RGB + alpha
          num_colors = 3;
	  for (y = 0, bufptr = buffer; y < 256; y ++)
	  {
	    for (x = 0; x < 256; x ++)
	    {
	      *bufptr++ = (unsigned char)y;
	      *bufptr++ = (unsigned char)(y + x);
	      *bufptr++ = (unsigned char)(y - x);

	      if (i > 2)
	      {
	        // Add alpha channel
	        *bufptr++ = (unsigned char)((x + y) / 2);
	      }
	    }
	  }
          break;
      case 2 : // CMYK
      case 5 : // CMYK + alpha
          num_colors = 4;
	  for (y = 0, bufptr = buffer; y < 256; y ++)
	  {
	    for (x = 0; x < 256; x ++)
	    {
	      unsigned char cc = (unsigned char)y;
	      unsigned char mm = (unsigned char)(y + x);
	      unsigned char yy = (unsigned char)(y - x);
	      unsigned char kk = cc < mm ? cc < yy ? cc : yy : mm < yy ? mm : yy;

              *bufptr++ = (unsigned char)(cc - kk);
              *bufptr++ = (unsigned char)(mm - kk);
              *bufptr++ = (unsigned char)(yy - kk);
              *bufptr++ = (unsigned char)(kk);

	      if (i > 2)
	      {
	        // Add alpha channel
	        *bufptr++ = (unsigned char)((x + y) / 2);
	      }
	    }
	  }
          break;
    }

    // Write the image...
    printf("pdfioFileCreateImageObjFromData(num_colors=%u, alpha=%s): ", (unsigned)num_colors, i > 2 ? "true" : "false");
    if ((images[i] = pdfioFileCreateImageObjFromData(pdf, buffer, 256, 256, num_colors, NULL, i > 2, false)) != NULL)
    {
      puts("PASS");
    }
    else
    {
      puts("FAIL");
      return (1);
    }
  }

  // Create the page dictionary, object, and stream...
  fputs("pdfioDictCreate: ", stdout);
  if ((dict = pdfioDictCreate(pdf)) != NULL)
    puts("PASS");
  else
    return (1);

  for (i = 0; i < 6; i ++)
  {
    printf("pdfioPageDictAddImage(%d): ", i + 1);
    snprintf(iname, sizeof(iname), "IM%d", i + 1);
    if (pdfioPageDictAddImage(dict, pdfioStringCreate(pdf, iname), images[i]))
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

  if (write_header_footer(st, "Image Writing Test", number))
    goto error;

  // Draw images
  for (i = 0; i < 6; i ++)
  {
    static const char *labels[] = {
      "DeviceGray",
      "DeviceRGB",
      "DeviceCMYK",
      "DeviceGray + Alpha",
      "DeviceRGB + Alpha",
      "DeviceCMYK + Alpha"
    };

    snprintf(iname, sizeof(iname), "IM%d", i + 1);

    if (draw_image(st, iname, 36 + 180 * (i % 3), 306 - 216 * (i / 3), 144, 144, labels[i]))
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

      printf("pdfioContentPathRect(x=%g, y=%g, w=%g, h=%g): ", col * 6.0, row * 6.0, 6.0, 6.0);
      if (pdfioContentPathRect(st, col * 6.0, row * 6.0, 6.0, 6.0))
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
  pdfio_obj_t		*prophoto;	// ProPhotoRGB ICC profile object


  fputs("pdfioFileCreateICCObjFromFile(ProPhotoRGB): ", stdout);
  if ((prophoto = pdfioFileCreateICCObjFromFile(pdf, "testfiles/iso22028-2-romm-rgb.icc", 3)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioDictCreate: ", stdout);
  if ((dict = pdfioDictCreate(pdf)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioArrayCreateColorFromMatrix(AdobeRGB): ", stdout);
  if ((cs = pdfioArrayCreateColorFromMatrix(pdf, 3, pdfioAdobeRGBGamma, pdfioAdobeRGBMatrix, pdfioAdobeRGBWhitePoint)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddColorSpace(AdobeRGB): ", stdout);
  if (pdfioPageDictAddColorSpace(dict, "AdobeRGB", cs))
    puts("PASS");
  else
    return (1);

  fputs("pdfioArrayCreateColorFromMatrix(DisplayP3): ", stdout);
  if ((cs = pdfioArrayCreateColorFromMatrix(pdf, 3, pdfioDisplayP3Gamma, pdfioDisplayP3Matrix, pdfioDisplayP3WhitePoint)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddColorSpace(DisplayP3): ", stdout);
  if (pdfioPageDictAddColorSpace(dict, "DisplayP3", cs))
    puts("PASS");
  else
    return (1);

  fputs("pdfioArrayCreateColorFromICCObj(ProPhotoRGB): ", stdout);
  if ((cs = pdfioArrayCreateColorFromICCObj(pdf, prophoto)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddColorSpace(ProPhotoRGB): ", stdout);
  if (pdfioPageDictAddColorSpace(dict, "ProPhotoRGB", cs))
    puts("PASS");
  else
    return (1);

  fputs("pdfioArrayCreateColorFromMatrix(sRGB): ", stdout);
  if ((cs = pdfioArrayCreateColorFromMatrix(pdf, 3, pdfioSRGBGamma, pdfioSRGBMatrix, pdfioSRGBWhitePoint)) != NULL)
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

  if (write_header_footer(st, "Color Space Test", number))
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

  fputs("pdfioContentTextMoveTo(82, 234): ", stdout);
  if (pdfioContentTextMoveTo(st, 82, 234))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextShow(\"AdobeRGB\"): ", stdout);
  if (pdfioContentTextShow(st, false, "AdobeRGB"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(234, 0): ", stdout);
  if (pdfioContentTextMoveTo(st, 234, 0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextShow(\"DisplayP3\"): ", stdout);
  if (pdfioContentTextShow(st, false, "DisplayP3"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(-234, 216): ", stdout);
  if (pdfioContentTextMoveTo(st, -234, 216))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextShow(\"sRGB\"): ", stdout);
  if (pdfioContentTextShow(st, false, "sRGB"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(234, 0): ", stdout);
  if (pdfioContentTextMoveTo(st, 234, 0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextShow(\"ProPhotoRGB\"): ", stdout);
  if (pdfioContentTextShow(st, false, "ProPhotoRGB"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(-234, 216): ", stdout);
  if (pdfioContentTextMoveTo(st, -234, 216))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextShow(\"DeviceCMYK\"): ", stdout);
  if (pdfioContentTextShow(st, false, "DeviceCMYK"))
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

  fputs("pdfioContentMatrixTranslate(82, 90): ", stdout);
  if (pdfioContentMatrixTranslate(st, 82, 90))
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

  fputs("pdfioContentMatrixTranslate(316, 90): ", stdout);
  if (pdfioContentMatrixTranslate(st, 316, 90))
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

  fputs("pdfioContentMatrixTranslate(82, 306): ", stdout);
  if (pdfioContentMatrixTranslate(st, 82, 306))
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

  fputs("pdfioContentSetFillColorSpace(ProPhotoRGB): ", stdout);
  if (pdfioContentSetFillColorSpace(st, "ProPhotoRGB"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentMatrixTranslate(316, 306): ", stdout);
  if (pdfioContentMatrixTranslate(st, 316, 306))
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

  fputs("pdfioContentMatrixTranslate(82, 522): ", stdout);
  if (pdfioContentMatrixTranslate(st, 82, 522))
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
// 'write_font_test()' - Write a font test page.
//

static int				// O - 1 on failure, 0 on success
write_font_test(pdfio_file_t *pdf,	// I - PDF file
                int          number,	// I - Page number
		pdfio_obj_t  *font,	// I - Page number font
		bool         unicode)	// I - Use Unicode font?
{
  pdfio_dict_t		*dict;		// Page dictionary
  pdfio_stream_t	*st;		// Page contents stream
  pdfio_obj_t		*opensans;	// OpenSans-Regular font
  int			i;		// Looping var
  static const char * const welcomes[] =// "Welcome" in many languages
  {
    "Welcome\n",
    "Welkom\n",
    "ḫaṣānu\n",
    "Mayad-ayad nga pad-abot\n",
    "Mir se vjên\n",
    "Mirë se vjen\n",
    "Wellkumma\n",
    "Bienveniu\n",
    "Ghini vinit!\n",
    "Bienveníu\n",
    "Miro peicak\n",
    "Xoş gəlmişsiniz!\n",
    "Salamat datang\n",
    "Сәләм бирем!\n",
    "Menjuah-juah!\n",
    "Še das d' kemma bisd\n",
    "Mwaiseni\n",
    "Maogmáng Pag-abót\n",
    "Welkam\n",
    "Dobrodošli\n",
    "Degemer mat\n",
    "Benvingut\n",
    "Maayong pag-abot\n",
    "Kopisanangan do kinorikatan\n",
    "Bienvenida\n",
    "Bien binidu\n",
    "Bienbenidu\n",
    "Hóʔą\n",
    "Boolkhent!\n",
    "Kopivosian do kinoikatan\n",
    "Malipayeng Pag-abot!\n",
    "Vítej\n",
    "Velkommen\n",
    "Salâm\n",
    "Welkom\n",
    "Emedi\n",
    "Welkumin\n",
    "Tere tulemast\n",
    "Woé zɔ\n",
    "Bienveníu\n",
    "Vælkomin\n",
    "Bula\n",
    "Tervetuloa\n",
    "Bienvenue\n",
    "Wäljkiimen\n",
    "Wäilkuumen\n",
    "Wäilkuumen\n",
    "Wolkom\n",
    "Benvignût\n",
    "Benvido\n",
    "Willkommen\n",
    "Ἀσπάζομαι!\n",
    "Καλώς Ήρθες\n",
    "Tikilluarit\n",
    "Byen venu\n",
    "Sannu da zuwa\n",
    "Aloha\n",
    "Wayakurua\n",
    "Dayón\n",
    "Zoo siab txais tos!\n",
    "Üdvözlet\n",
    "Selamat datai\n",
    "Velkomin\n",
    "Nnọọ\n",
    "Selamat datang\n",
    "Qaimarutin\n",
    "Fáilte\n",
    "Benvenuto\n",
    "Voschata\n",
    "Murakaza neza\n",
    "Mauri\n",
    "Tu be xér hatî ye!\n",
    "Taŋyáŋ yahí\n",
    "Salve\n",
    "Laipni lūdzam\n",
    "Wilkóm\n",
    "Sveiki atvykę\n",
    "Willkamen\n",
    "Mu amuhezwi\n",
    "Tukusanyukidde\n",
    "Wëllkomm\n",
    "Swagatam\n",
    "Tonga soa\n",
    "Selamat datang\n",
    "Merħba\n",
    "B’a’ntulena\n",
    "Failt ort\n",
    "Haere mai\n",
    "mai\n",
    "Pjila’si\n",
    "Benvegnüu\n",
    "Ne y kena\n",
    "Ximopanōltih\n",
    "Yá'át'ééh\n",
    "Siyalemukela\n",
    "Siyalemukela\n",
    "Bures boahtin\n",
    "Re a go amogela\n",
    "Velkommen\n",
    "Benvengut!\n",
    "Bon bini\n",
    "Witam Cię\n",
    "Bem-vindo\n",
    "Haykuykuy!\n",
    "T'aves baxtalo\n",
    "Bainvegni\n",
    "Afio mai\n",
    "Ennidos\n",
    "Walcome\n",
    "Fàilte\n",
    "Mauya\n",
    "Bon vinutu\n",
    "Vitaj\n",
    "Dobrodošli\n",
    "Soo dhowow\n",
    "Witaj\n",
    "Bienvenido\n",
    "Wilujeng sumping\n",
    "Karibu\n",
    "Wamukelekile\n",
    "Välkommen\n",
    "Wilkomme\n",
    "Maligayang pagdating\n",
    "Maeva\n",
    "Räxim itegez\n",
    "Ksolok Bodik Mai\n",
    "Ulu tons mai\n",
    "Welkam\n",
    "Talitali fiefia\n",
    "Lek oy li la tale\n",
    "amogetswe\n",
    "Tempokani\n",
    "Hoş geldin\n",
    "Koş geldiniz\n",
    "Ulufale mai!\n",
    "Xush kelibsiz\n",
    "Benvignùo\n",
    "Tervhen tuldes\n",
    "Hoan nghênh\n",
    "Tere tulõmast\n",
    "Benvnuwe\n",
    "Croeso\n",
    "Merhbe\n",
    "Wamkelekile\n",
    "Märr-ŋamathirri\n",
    "Ẹ ku abọ\n",
    "Kíimak 'oolal\n",
    "Ngiyakwemukela\n"
  };


#if 0
  if (unicode)
  {
    fputs("pdfioFileCreateFontObjFromFile(NotoSansJP-Regular.otf): ", stdout);
    if ((opensans = pdfioFileCreateFontObjFromFile(pdf, "testfiles/NotoSansJP-Regular.otf", true)) != NULL)
      puts("PASS");
    else
      return (1);
  }
  else
#endif // 0
  {
    fputs("pdfioFileCreateFontObjFromFile(OpenSans-Regular.ttf): ", stdout);
    if ((opensans = pdfioFileCreateFontObjFromFile(pdf, "testfiles/OpenSans-Regular.ttf", unicode)) != NULL)
      puts("PASS");
    else
      return (1);
  }

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
  if (pdfioPageDictAddFont(dict, "F2", opensans))
    puts("PASS");
  else
    return (1);

  printf("pdfioFileCreatePage(%d): ", number);

  if ((st = pdfioFileCreatePage(pdf, dict)) != NULL)
    puts("PASS");
  else
    return (1);

  if (write_header_footer(st, unicode ? "Unicode TrueType Font Test" : "CP1252 TrueType Font Test", number))
    goto error;

  fputs("pdfioContentTextBegin(): ", stdout);
  if (pdfioContentTextBegin(st))
    puts("PASS");
  else
    return (1);

  fputs("pdfioContentSetTextFont(\"F2\", 10.0): ", stdout);
  if (pdfioContentSetTextFont(st, "F2", 10.0))
    puts("PASS");
  else
    return (1);

  fputs("pdfioContentSetTextLeading(12.0): ", stdout);
  if (pdfioContentSetTextLeading(st, 12.0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(36.0, 702.0): ", stdout);
  if (pdfioContentTextMoveTo(st, 36.0, 702.0))
    puts("PASS");
  else
    return (1);

  for (i = 0; i < (int)(sizeof(welcomes) / sizeof(welcomes[0])); i ++)
  {
    if (i > 0 && (i % 50) == 0)
    {
      fputs("pdfioContentTextMoveTo(200.0, 600.0): ", stdout);
      if (pdfioContentTextMoveTo(st, 200.0, 600.0))
	puts("PASS");
      else
	return (1);
    }

    printf("pdfioContentTextShow(\"%s\"): ", welcomes[i]);
    if (pdfioContentTextShow(st, unicode, welcomes[i]))
      puts("PASS");
    else
      return (1);
  }

  fputs("pdfioContentTextEnd(): ", stdout);
  if (pdfioContentTextEnd(st))
    puts("PASS");
  else
    return (1);

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
// 'write_header_footer()' - Write common header and footer text.
//

static int				// O - 1 on failure, 0 on success
write_header_footer(
    pdfio_stream_t *st,			// I - Page content stream
    const char     *title,		// I - Title
    int            number)		// I - Page number
{
  fputs("pdfioContentSetFillColorDeviceGray(0.0): ", stdout);
  if (pdfioContentSetFillColorDeviceGray(st, 0.0))
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

  fputs("pdfioContentTextMoveTo(36.0, 738.0): ", stdout);
  if (pdfioContentTextMoveTo(st, 36.0, 738.0))
    puts("PASS");
  else
    return (1);

  printf("pdfioContentTextShow(\"%s\"): ", title);
  if (pdfioContentTextShow(st, false, title))
    puts("PASS");
  else
    return (1);

  fputs("pdfioContentSetTextFont(\"F1\", 12.0): ", stdout);
  if (pdfioContentSetTextFont(st, "F1", 12.0))
    puts("PASS");
  else
    return (1);

  fputs("pdfioContentTextMoveTo(514.0, -702.0): ", stdout);
  if (pdfioContentTextMoveTo(st, 514.0, -702.0))
    puts("PASS");
  else
    return (1);

  printf("pdfioContentTextShowf(\"%d\"): ", number);
  if (pdfioContentTextShowf(st, false, "%d", number))
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

  // DecodeParms dictionary...
  if ((decode = pdfioDictCreate(pdf)) == NULL)
    return (NULL);

  pdfioDictSetNumber(decode, "BitsPerComponent", 8);
  pdfioDictSetNumber(decode, "Colors", 3);
  pdfioDictSetNumber(decode, "Columns", 256);
  pdfioDictSetNumber(decode, "Predictor", predictor);
  pdfioDictSetDict(dict, "DecodeParms", decode);

  // Create the image object...
  if ((obj = pdfioFileCreateObj(pdf, dict)) == NULL)
    return (NULL);

  // Create the image stream and write the image...
  if ((st = pdfioObjCreateStream(obj, PDFIO_FILTER_FLATE)) == NULL)
    return (NULL);

  // This creates a useful criss-cross image that highlights predictor errors...
  for (y = 0; y < 256; y ++)
  {
    for (x = 0, bufptr = buffer; x < 256; x ++, bufptr += 3)
    {
      bufptr[0] = (unsigned char)y;
      bufptr[1] = (unsigned char)(y + x);
      bufptr[2] = (unsigned char)(y - x);
    }

    if (!pdfioStreamWrite(st, buffer, sizeof(buffer)))
    {
      pdfioStreamClose(st);
      return (NULL);
    }
  }

  // Close the object and stream...
  pdfioStreamClose(st);

  return (obj);
}


//
// 'write_images_test()' - Write a series of test images.
//

static int				// O - 1 on failure, 0 on success
write_images_test(
    pdfio_file_t *pdf,			// I - PDF file
    int          number,		// I - Page number
    pdfio_obj_t  *font)			// I - Text font
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

  if (write_header_footer(st, "Image Predictor Test", number))
    goto error;

  // Draw images
  if (draw_image(st, "IM1", 36, 522, 144, 144, "No Predictor"))
    goto error;

  for (p = _PDFIO_PREDICTOR_PNG_NONE; p <= _PDFIO_PREDICTOR_PNG_AUTO; p ++)
  {
    int i = (int)p - _PDFIO_PREDICTOR_PNG_NONE;

    snprintf(pname, sizeof(pname), "IM%d", p);
    snprintf(plabel, sizeof(plabel), "PNG Predictor %d", p);

    if (draw_image(st, pname, 36 + 180 * (i % 3), 306 - 216 * (i / 3), 144, 144, plabel))
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
// 'write_jpeg_test()' - Write a page with a JPEG image to a PDF file.
//

static int				// O - 1 on failure, 0 on success
write_jpeg_test(pdfio_file_t *pdf,	// I - PDF file
                const char   *title,	// I - Page title
		int          number,	// I - Page number
		pdfio_obj_t  *font,	// I - Text font
		pdfio_obj_t  *image)	// I - Image to draw
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

  if (write_header_footer(st, title, number))
    goto error;

  // Calculate the scaled size of the image...
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
  if (sheight > 500.0)
  {
    sheight = 500.0;
    swidth  = sheight * width / height;
  }

  tx = 0.5 * (595.28 - swidth);
  ty = 0.5 * (792 - sheight);

  // Show "raw" content (a bordered box for the image...)
  fputs("pdfioStreamPrintf(...): ", stdout);
  if (pdfioStreamPrintf(st,
                       "1 0 0 RG 0 g 5 w\n"
                       "%g %g %g %g re %g %g %g %g re B*\n", tx - 36, ty - 36, swidth + 72, sheight + 72, tx - 1, ty - 1, swidth + 2, sheight + 2))
    puts("PASS");
  else
    goto error;

  // Draw the image inside the border box...
  printf("pdfioContentDrawImage(\"IM1\", x=%g, y=%g, w=%g, h=%g): ", tx, ty, swidth, sheight);
  if (pdfioContentDrawImage(st, "IM1", tx, ty, swidth, sheight))
    puts("PASS");
  else
    goto error;

  // Close the page stream/object...
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
// 'write_png_test()' - Write a page of PNG test images.
//

static int				// O - 0 on success, 1 on failure
write_png_test(pdfio_file_t *pdf,		// I - PDF file
          int          number,		// I - Page number
          pdfio_obj_t  *font)		// I - Page number font
{
  pdfio_dict_t		*dict;		// Page dictionary
  pdfio_stream_t	*st;		// Page contents stream
  pdfio_obj_t		*color,		// pdfio-color.png
			*gray,		// pdfio-gray.png
			*indexed;	// pdfio-indexed.png


  // Import the PNG test images
  fputs("pdfioFileCreateImageObjFromFile(\"testfiles/pdfio-color.png\"): ", stdout);
  if ((color = pdfioFileCreateImageObjFromFile(pdf, "testfiles/pdfio-color.png", false)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioFileCreateImageObjFromFile(\"testfiles/pdfio-gray.png\"): ", stdout);
  if ((gray = pdfioFileCreateImageObjFromFile(pdf, "testfiles/pdfio-gray.png", false)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioFileCreateImageObjFromFile(\"testfiles/pdfio-indexed.png\"): ", stdout);
  if ((indexed = pdfioFileCreateImageObjFromFile(pdf, "testfiles/pdfio-indexed.png", false)) != NULL)
    puts("PASS");
  else
    return (1);

  // Create the page dictionary, object, and stream...
  fputs("pdfioDictCreate: ", stdout);
  if ((dict = pdfioDictCreate(pdf)) != NULL)
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddImage(color): ", stdout);
  if (pdfioPageDictAddImage(dict, "IM1", color))
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddImage(gray): ", stdout);
  if (pdfioPageDictAddImage(dict, "IM2", gray))
    puts("PASS");
  else
    return (1);

  fputs("pdfioPageDictAddImage(indexed): ", stdout);
  if (pdfioPageDictAddImage(dict, "IM3", indexed))
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

  if (write_header_footer(st, "PNG Image Test Page", number))
    goto error;

  // Show content...
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

  fputs("pdfioContentTextMoveTo(36.0, 342.0): ", stdout);
  if (pdfioContentTextMoveTo(st, 36.0, 342.0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextShow(\"PNG RGB Color\"): ", stdout);
  if (pdfioContentTextShow(st, false, "PNG RGB Color"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(288.0, 0.0): ", stdout);
  if (pdfioContentTextMoveTo(st, 288.0, 0.0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextShow(\"PNG Gray\"): ", stdout);
  if (pdfioContentTextShow(st, false, "PNG Gray"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextMoveTo(-288.0, 288.0): ", stdout);
  if (pdfioContentTextMoveTo(st, -288.0, 288.0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextShow(\"PNG Indexed\"): ", stdout);
  if (pdfioContentTextShow(st, false, "PNG Indexed"))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentTextEnd(): ", stdout);
  if (pdfioContentTextEnd(st))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentDrawImage(\"IM1\"): ", stdout);
  if (pdfioContentDrawImage(st, "IM1", 36, 108, 216, 216))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentDrawImage(\"IM2\"): ", stdout);
  if (pdfioContentDrawImage(st, "IM2", 324, 108, 216, 216))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentDrawImage(\"IM3\"): ", stdout);
  if (pdfioContentDrawImage(st, "IM3", 36, 396, 216, 216))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentSetFillColorDeviceRGB(0, 1, 1): ", stdout);
  if (pdfioContentSetFillColorDeviceRGB(st, 0.0, 1.0, 1.0))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentPathRect(315, 387, 234, 234): ", stdout);
  if (pdfioContentPathRect(st, 315, 387, 234, 234))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentFill(false): ", stdout);
  if (pdfioContentFill(st, false))
    puts("PASS");
  else
    goto error;

  fputs("pdfioContentDrawImage(\"IM3\"): ", stdout);
  if (pdfioContentDrawImage(st, "IM3", 324, 396, 216, 216))
    puts("PASS");
  else
    goto error;

  // Close the object and stream...
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
// 'write_text_test()' - Print a plain text file.
//

static int				// O - 0 on success, 1 on failure
write_text_test(pdfio_file_t *pdf,		// I - PDF file
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
  fputs("pdfioFileCreateFontObjFromBase(\"Courier\"): ", stdout);
  if ((courier = pdfioFileCreateFontObjFromBase(pdf, "Courier")) != NULL)
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

      if (write_header_footer(st, "README.md", page))
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

      fputs("pdfioContentTextMoveTo(36.0, 708.0): ", stdout);
      if (pdfioContentTextMoveTo(st, 36.0, 708.0))
	puts("PASS");
      else
	goto error;
    }

    if (!pdfioContentSetFillColorDeviceGray(st, 0.75))
      goto error;
    if (!pdfioContentTextShowf(st, false, "%3d  ", flinenum))
      goto error;
    if (!pdfioContentSetFillColorDeviceGray(st, 0.0))
      goto error;
    if (strlen(line) > 81)
    {
      char	temp[82];	// Temporary string

      memcpy(temp, line, 80);
      temp[80] = '\n';
      temp[81] = '\0';

      if (!pdfioContentTextShow(st, false, temp))
        goto error;

      if (!pdfioContentTextShowf(st, false, "     %s", line + 80))
        goto error;

      plinenum ++;
    }
    else if (!pdfioContentTextShow(st, false, line))
      goto error;

    plinenum ++;
    if (plinenum >= 56)
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
