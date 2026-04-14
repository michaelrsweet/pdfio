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


//
// 'pdfioPageCopy()' - Copy a page to a PDF file.
//

bool					// O - `true` on success, `false` on failure
pdfioPageCopy(pdfio_file_t *pdf,	// I - PDF file
              pdfio_obj_t  *srcpage)	// I - Source page
{
  pdfio_dict_t	*dstdict;		// Destination page dictionary
  _pdfio_value_t dstvalue;		// Destination value
  pdfio_obj_t	*dstpage;		// Destination page object
  pdfio_obj_t	*parent;		// Parent reference
  int		depth = 0;		// Number of parents
  pdfio_dict_t	*srcdict;		// Source page dictionary
  size_t	i;			// Looping var
  _pdfio_pair_t	*srcpair;		// Source page dictionary pair
#ifdef DEBUG
  static const char * const types[] =	// Value types
  {
    "PDFIO_VALTYPE_NONE",		// No value, not set
    "PDFIO_VALTYPE_ARRAY",		// Array
    "PDFIO_VALTYPE_BINARY",		// Binary data
    "PDFIO_VALTYPE_BOOLEAN",		// Boolean
    "PDFIO_VALTYPE_DATE",		// Date/time
    "PDFIO_VALTYPE_DICT",		// Dictionary
    "PDFIO_VALTYPE_INDIRECT",		// Indirect object (N G obj)
    "PDFIO_VALTYPE_NAME",		// Name
    "PDFIO_VALTYPE_NULL",		// Null object
    "PDFIO_VALTYPE_NUMBER",		// Number (integer or real)
    "PDFIO_VALTYPE_STRING"		// String
  };
#endif // DEBUG


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

  // Copy the source page dictionary(s) to a new destination page dictionary...
  if ((dstdict = pdfioDictCreate(pdf)) == NULL)
    return (false);

  parent = srcpage;
  do
  {
    depth ++;

    PDFIO_DEBUG("pdfioPageCopy: parent=%p(%u)\n", parent, (unsigned)parent->number);

    if (parent->value.type != PDFIO_VALTYPE_DICT)
      break;

    srcdict = parent->value.value.dict;
    parent  = NULL;

    for (i = srcdict->num_pairs, srcpair = srcdict->pairs; i > 0; i --, srcpair ++)
    {
      PDFIO_DEBUG("pdfioPageCopy: depth=%d, key=/%s, type=%s\n", depth, srcpair->key, types[srcpair->value.type]);

      if (!strcmp(srcpair->key, "Count") || !strcmp(srcpair->key, "Kids"))
      {
        // Skip keys specific to parent nodes...
        continue;
      }
      else if (!strcmp(srcpair->key, "Parent"))
      {
        // Saw a Parent node, save it for the next loop...
	if (srcpair->value.type == PDFIO_VALTYPE_INDIRECT)
	  parent = pdfioFileFindObj(srcpage->pdf, srcpair->value.value.indirect.number);
      }
      else if (srcdict == srcpage->value.value.dict || !_pdfioDictGetValue(dstdict, srcpair->key))
      {
        // New key/value pair...
        _pdfioDictSetValue(dstdict, pdfioStringCreate(pdf, srcpair->key), _pdfioValueCopy(pdf, &dstvalue, srcpage->pdf, &srcpair->value));
      }
    }
  }
  while (parent != NULL && depth < PDFIO_MAX_DEPTH);

  // Make sure the page dictionary has all of the required keys...
  if (!_pdfioDictGetValue(dstdict, "CropBox"))
    pdfioDictSetRect(dstdict, "CropBox", &srcpage->pdf->crop_box);

  if (!_pdfioDictGetValue(dstdict, "MediaBox"))
    pdfioDictSetRect(dstdict, "MediaBox", &srcpage->pdf->media_box);

  pdfioDictSetObj(dstdict, "Parent", pdf->pages_obj);

  if (!_pdfioDictGetValue(dstdict, "Type"))
    pdfioDictSetName(dstdict, "Type", "Page");

  // Create the page object and add it to the pages array...
  if ((dstpage = pdfioFileCreateObj(pdf, dstdict)) == NULL)
    return (false);

  PDFIO_DEBUG("pdfioPageCopy: dstpage=%p(%u)\n", dstpage, (unsigned)dstpage->number);

  if (pdfioObjClose(dstpage))
    return (_pdfioFileAddPage(pdf, dstpage));
  else
    return (false);
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


  if (!contents)
    return (NULL);
  else if (contents->type == PDFIO_VALTYPE_ARRAY && n < pdfioArrayGetSize(contents->value.array))
    return (pdfioObjOpenStream(pdfioArrayGetObj(contents->value.array, n), decode));
  else if (n)
    return (NULL);
  else
    return (pdfioObjOpenStream(pdfioFileFindObj(page->pdf, contents->value.indirect.number), decode));
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

  if ((contents = _pdfioDictGetValue(page->value.value.dict, "Contents")) == NULL)
    return (NULL);

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
