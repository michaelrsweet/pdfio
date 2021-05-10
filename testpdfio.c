//
// Test program for pdfio.
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


//
// Local functions...
//

static int	do_test_file(const char *filename);
static int	do_unit_tests(void);
static bool	error_cb(bool *error, const char *message);
static ssize_t	token_consume_cb(const char **s, size_t bytes);
static ssize_t	token_peek_cb(const char **s, char *buffer, size_t bytes);


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
      if (!do_test_file(argv[i]))
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
  pdfio_file_t	*pdf;			// PDF file
  size_t	n,			// Object/page index
		num_objs,		// Number of objects
		num_pages;		// Number of pages
  pdfio_obj_t	*obj;			// Object
  pdfio_dict_t	*dict;			// Object dictionary
  const char	*type;			// Object type


  // Try opening the file...
  if ((pdf = pdfioFileOpen(filename, NULL, NULL)) != NULL)
  {
    // Show basic stats...
    num_objs  = pdfioFileGetNumObjects(pdf);
    num_pages = pdfioFileGetNumPages(pdf);

    printf("%s: PDF %s, %d pages, %d objects.\n", filename, pdfioFileGetVersion(pdf), (int)num_pages, (int)num_objs);

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

	printf("%s: Page #%d is %gx%g.\n", filename, (int)n + 1, media_box.x2, media_box.y2);
      }
    }

    // Show the associated value with each object...
    for (n = 0; n < num_objs; n ++)
    {
      if ((obj = pdfioFileGetObject(pdf, n)) == NULL)
      {
	printf("%s: Unable to get object #%d.\n", filename, (int)n);
      }
      else
      {
	size_t 		np;	// Number of pairs
	_pdfio_pair_t	*pair;	// Current pair

	dict = pdfioObjGetDict(obj);

	printf("%s: %u %u obj dict=%p(%lu pairs)\n", filename, (unsigned)pdfioObjGetNumber(obj), (unsigned)pdfioObjGetGeneration(obj), dict, dict ? (unsigned long)dict->num_pairs : 0UL);
	if (dict)
	{
	  // Show a summary of each pair in the dictionary...
	  for (np = dict->num_pairs, pair = dict->pairs; np > 0; np --, pair ++)
	  {
	    switch (pair->value.type)
	    {
	      case PDFIO_VALTYPE_INDIRECT :
		  printf("    /%s %u %u R\n", pair->key, (unsigned)pair->value.value.indirect.number, pair->value.value.indirect.generation);
		  break;
	      case PDFIO_VALTYPE_NAME :
		  printf("    /%s /%s\n", pair->key, pair->value.value.name);
		  break;
	      case PDFIO_VALTYPE_STRING :
		  printf("    /%s (%s)\n", pair->key, pair->value.value.string);
		  break;
	      case PDFIO_VALTYPE_BINARY :
		  {
		    size_t bn;
		    unsigned char *bptr;

		    printf("    /%s <", pair->key);
		    for (bn = pair->value.value.binary.datalen, bptr = pair->value.value.binary.data; bn > 0; bn --, bptr ++)
		      printf("%02X", *bptr);
		    puts(">");
		  }
		  break;
	      case PDFIO_VALTYPE_NUMBER :
		  printf("    /%s %g\n", pair->key, pair->value.value.number);
		  break;
	      case PDFIO_VALTYPE_BOOLEAN :
		  printf("    /%s %s\n", pair->key, pair->value.value.boolean ? "true" : "false");
		  break;
	      case PDFIO_VALTYPE_NULL :
		  printf("    /%s null\n", pair->key);
		  break;
	      case PDFIO_VALTYPE_ARRAY :
		  printf("    /%s [...]\n", pair->key);
		  break;
	      case PDFIO_VALTYPE_DICT :
		  printf("    /%s <<...>>\n", pair->key);
		  break;
	      default :
		  printf("    /%s ...\n", pair->key);
		  break;
	    }
	  }
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
  pdfio_file_t		*pdf;		// PDF file
  bool			error = false;	// Error callback data
  _pdfio_token_t	tb;		// Token buffer
  const char		*s;		// String buffer
  _pdfio_value_t	value;		// Value
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


  // First open the test PDF file...
  fputs("pdfioFileOpen(testpdfio.pdf): ", stdout);
  if ((pdf = pdfioFileOpen("testpdfio.pdf", (pdfio_error_cb_t)error_cb, &error)) != NULL)
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

  // Close the test PDF file...
  fputs("pdfioFileClose: ", stdout);
  if (pdfioFileClose(pdf))
    puts("PASS");
  else
    return (1);

  return (0);
}


//
// 'error_cb()' - Display an error message during a unit test.
//

static bool				// O  - `true` to stop, `false` to continue
error_cb(bool       *error,		// IO - Have we displayed an error?
         const char *message)		// I  - Error message
{
  if (!*error)
  {
    // First error, so show a "FAIL" indicator
    *error = true;

    printf("FAIL (%s)\n", message);
  }
  else
  {
    // Subsequent errors are just indented...
    printf("    %s\n", message);
  }

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
