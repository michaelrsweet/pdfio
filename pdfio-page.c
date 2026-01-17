//
// PDF page functions for PDFio.
//
// Copyright © 2021-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pdfio-private.h"


//
// Local functions...
//

static _pdfio_value_t	*get_contents(pdfio_obj_t *page);
static _pdfio_value_t	*get_page_value(pdfio_obj_t *page, const char *key);


//
// 'pdfioPageCopy()' - Copy a page to a PDF file.
//

bool					// O - `true` on success, `false` on failure
pdfioPageCopy(pdfio_file_t *pdf,	// I - PDF file
              pdfio_obj_t  *srcpage)	// I - Source page
{
  pdfio_obj_t	*dstpage;		// Destination page object


  PDFIO_DEBUG("pdfioPageCopy(pdf=%p, srcpage=%p(%p))\n", (void *)pdf, (void *)srcpage, srcpage ? (void *)srcpage->pdf : NULL);

  // Range check input
  if (!pdf || !srcpage || srcpage->value.type != PDFIO_VALTYPE_DICT)
  {
    if (pdf)
    {
      if (!srcpage)
        _pdfioFileError(pdf, "NULL page object specified.");
      else
        _pdfioFileError(pdf, "Object is not a page.");
    }

    return (false);
  }

  // Copy the page object and add it to the pages array...
  if ((dstpage = pdfioObjCopy(pdf, srcpage)) == NULL)
    return (false);
  else
    return (_pdfioFileAddPage(pdf, dstpage));
}


//
// 'pdfioPageGetArray()' - Get an array value from the page dictionary.
//
// This function looks up an array value in the page dictionary, either in the
// specified object or one of its parents.
//
// @since PDFio 1.7@
//

pdfio_array_t *				// O - Array or `NULL` if none
pdfioPageGetArray(pdfio_obj_t *page,	// I - Page object
                  const char  *key)	// I - Dictionary key
{
  _pdfio_value_t *v = get_page_value(page, key);
					// Dictionary value


  if (v && v->type == PDFIO_VALTYPE_ARRAY)
    return (v->value.array);
  else
    return (NULL);
}


//
// 'pdfioPageGetBinary()' - Get a binary value from the page dictionary.
//
// This function looks up a binary value in the page dictionary, either in the
// specified object or one of its parents.
//
// @since PDFio 1.7@
//

unsigned char *				// O - Pointer to binary data or `NULL` if none
pdfioPageGetBinary(pdfio_obj_t *page,	// I - Page object
                   const char  *key,	// I - Dictionary key
                   size_t      *length)	// O - Length of value
{
  _pdfio_value_t *v = get_page_value(page, key);
					// Dictionary value


  if (v && v->type == PDFIO_VALTYPE_BINARY)
  {
    if (length)
      *length = v->value.binary.datalen;

    return (v->value.binary.data);
  }
  else
  {
    return (NULL);
  }
}


//
// 'pdfioPageGetBoolean()' - Get a boolean value from the page dictionary.
//
// This function looks up a boolean value in the page dictionary, either in the
// specified object or one of its parents.
//
// @since PDFio 1.7@
//

bool					// O - Boolean value or `false` if none
pdfioPageGetBoolean(pdfio_obj_t *page,	// I - Page object
                    const char  *key)	// I - Dictionary key
{
  _pdfio_value_t *v = get_page_value(page, key);
					// Dictionary value


  if (v && v->type == PDFIO_VALTYPE_ARRAY)
    return (v->value.boolean);
  else
    return (NULL);
}


//
// 'pdfioPageGetDate()' - Get a date value from the page dictionary.
//
// This function looks up a date value in the page dictionary, either in the
// specified object or one of its parents.
//
// @since PDFio 1.7@
//

time_t					// O - Date/time or `0` if none
pdfioPageGetDate(pdfio_obj_t *page,	// I - Page object
                 const char  *key)	// I - Dictionary key
{
  _pdfio_value_t *v = get_page_value(page, key);
					// Dictionary value


  if (v && v->type == PDFIO_VALTYPE_DATE)
    return (v->value.date);
  else
    return (0);
}


//
// 'pdfioPageGetDict()' - Get a dictionary value from the page dictionary.
//
// This function looks up a dictionary value in the page dictionary, either in
// the specified object or one of its parents.
//
// @since PDFio 1.7@
//

pdfio_dict_t *				// O - Dictionary or `NULL` if none
pdfioPageGetDict(pdfio_obj_t *page,	// I - Page object
                 const char  *key)	// I - Dictionary key
{
  _pdfio_value_t *v = get_page_value(page, key);
					// Dictionary value


  if (v && v->type == PDFIO_VALTYPE_DICT)
    return (v->value.dict);
  else
    return (NULL);
}


//
// 'pdfioPageGetName()' - Get a name value from the page dictionary.
//
// This function looks up a name value in the page dictionary, either in the
// specified object or one of its parents.
//
// @since PDFio 1.7@
//

const char *				// O - Name string or `NULL` if none
pdfioPageGetName(pdfio_obj_t *page,	// I - Page object
                 const char  *key)	// I - Dictionary key
{
  _pdfio_value_t *v = get_page_value(page, key);
					// Dictionary value


  if (v && v->type == PDFIO_VALTYPE_NAME)
    return (v->value.name);
  else
    return (NULL);
}


//
// 'pdfioPageGetNumStreams()' - Get the number of content streams for a page object.
//

size_t					// O - Number of streams
pdfioPageGetNumStreams(
    pdfio_obj_t *page)			// I - Page object
{
  _pdfio_value_t *contents = get_contents(page);
					// Contents value


  if (!contents)
    return (0);
  else if (contents->type == PDFIO_VALTYPE_ARRAY)
    return (pdfioArrayGetSize(contents->value.array));
  else
    return (1);
}


//
// 'pdfioPageGetNumber()' - Get a number value from the page dictionary.
//
// This function looks up a number value in the page dictionary, either in the
// specified object or one of its parents.
//
// @since PDFio 1.7@
//

double					// O - Number value or `0.0` if none
pdfioPageGetNumber(pdfio_obj_t *page,	// I - Page object
                   const char  *key)	// I - Dictionary key
{
  _pdfio_value_t *v = get_page_value(page, key);
					// Dictionary value


  if (v && v->type == PDFIO_VALTYPE_NUMBER)
    return (v->value.number);
  else
    return (0.0);
}


//
// 'pdfioPageGetObj()' - Get an indirect object value from the page dictionary.
//
// This function looks up an indirect object value in the page dictionary,
// either in the specified object or one of its parents.
//
// @since PDFio 1.7@
//

pdfio_obj_t *				// O - Object or `NULL` if none
pdfioPageGetObj(pdfio_obj_t *page,	// I - Page object
                const char  *key)	// I - Dictionary key
{
  _pdfio_value_t *v = get_page_value(page, key);
					// Dictionary value


  if (v && v->type == PDFIO_VALTYPE_INDIRECT)
    return (pdfioFileFindObj(page->pdf, v->value.indirect.number));
  else
    return (NULL);
}


//
// 'pdfioPageGetRect()' - Get a rectangle value from the page dictionary.
//
// This function looks up a rectangle value in the page dictionary, either in
// the specified object or one of its parents.
//
// @since PDFio 1.7@
//

pdfio_rect_t *				// O - Rectangle or `NULL` if none
pdfioPageGetRect(pdfio_obj_t  *page,	// I - Page object
                 const char   *key,	// I - Dictionary key
		 pdfio_rect_t *rect)	// O - Rectangle
{
  _pdfio_value_t *v = get_page_value(page, key);
					// Dictionary value


  if (v && v->type == PDFIO_VALTYPE_ARRAY && pdfioArrayGetSize(v->value.array) == 4)
  {
    rect->x1 = pdfioArrayGetNumber(v->value.array, 0);
    rect->y1 = pdfioArrayGetNumber(v->value.array, 1);
    rect->x2 = pdfioArrayGetNumber(v->value.array, 2);
    rect->y2 = pdfioArrayGetNumber(v->value.array, 3);

    return (rect);
  }
  else
  {
    return (NULL);
  }
}


//
// 'pdfioPageGetString()' - Get a string value from the page dictionary.
//
// This function looks up a string value in the page dictionary, either in the
// specified object or one of its parents.
//
// @since PDFio 1.7@
//

const char *				// O - String value or `NULL` if none
pdfioPageGetString(pdfio_obj_t *page,	// I - Page object
                   const char  *key)	// I - Dictionary key
{
  _pdfio_value_t *v = get_page_value(page, key);
					// Dictionary value


  if (v && v->type == PDFIO_VALTYPE_STRING)
    return (v->value.string);
  else
    return (NULL);
}


//
// 'pdfioPageOpenStream()' - Open a content stream for a page.
//

pdfio_stream_t *			// O - Stream
pdfioPageOpenStream(
    pdfio_obj_t *page,			// I - Page object
    size_t      n,			// I - Stream index (0-based)
    bool        decode)			// I - `true` to decode/decompress stream
{
  _pdfio_value_t *contents = get_contents(page);
					// Contents value


  PDFIO_DEBUG("pdfioPageOpenStream(page=%p(%lu), n=%lu, decode=%s)\n", (void *)page, page ? (unsigned long)page->number : 0, (unsigned long)n, decode ? "true" : "false");

  if (!contents)
  {
    PDFIO_DEBUG("pdfioPageOpenStream: No contents.\n");
    return (NULL);
  }
  else if (contents->type == PDFIO_VALTYPE_ARRAY && n < pdfioArrayGetSize(contents->value.array))
  {
    PDFIO_DEBUG("pdfioPageOpenStream: Contents is array, opening numbered content stream.\n");
    return (pdfioObjOpenStream(pdfioArrayGetObj(contents->value.array, n), decode));
  }
  else if (n)
  {
    PDFIO_DEBUG("pdfioPageOpenStream: Numbered stream does not exist.\n");
    return (NULL);
  }
  else
  {
    PDFIO_DEBUG("pdfioPageOpenStream: Opening single content stream %d.\n", (int)contents->value.indirect.number);
    return (pdfioObjOpenStream(pdfioFileFindObj(page->pdf, contents->value.indirect.number), decode));
  }
}


//
// 'get_contents()' - Get a page's Contents value.
//

static _pdfio_value_t *			// O - Value or NULL on error
get_contents(pdfio_obj_t *page)		// I - Page object
{
  _pdfio_value_t *contents;		// Contents value
  pdfio_obj_t	*obj;			// Contents object


  // Range check input...
  if (!page)
    return (NULL);

  // Load the page object as needed...
  if (page->value.type == PDFIO_VALTYPE_NONE)
  {
    if (!_pdfioObjLoad(page))
      return (NULL);
  }

  if (page->value.type != PDFIO_VALTYPE_DICT)
    return (NULL);

  contents = _pdfioDictGetValue(page->value.value.dict, "Contents");

  if (contents->type == PDFIO_VALTYPE_INDIRECT)
  {
    // See if the indirect object is a stream or an array of indirect object
    // references...
    if ((obj = pdfioFileFindObj(page->pdf, contents->value.indirect.number)) != NULL)
    {
      if (obj->value.type == PDFIO_VALTYPE_NONE)
      {
	if (!_pdfioObjLoad(obj))
	  return (NULL);
      }

      if (obj->value.type == PDFIO_VALTYPE_ARRAY)
        contents = &(obj->value);
    }
  }

  return (contents);
}


//
// 'get_page_value()' - Get a page dictionary value, including parents.
//

static _pdfio_value_t *			// O - Dictionary value or `NULL` if none
get_page_value(pdfio_obj_t *page,	// I - Page object
               const char  *key)	// I - Dictionary key
{
  _pdfio_value_t	*v = NULL;	// Dictionary value


  while (page != NULL)
  {
    // Load the page object as needed...
    if (page->value.type == PDFIO_VALTYPE_NONE && !_pdfioObjLoad(page))
      break;

    // If there isn't a dictionary for the object, stop...
    if (page->value.type != PDFIO_VALTYPE_DICT)
      break;

    // Lookup the key...
    if ((v = _pdfioDictGetValue(page->value.value.dict, key)) != NULL)
      break;

    page = pdfioDictGetObj(page->value.value.dict, "Parent");
  }

  return (v);
}
