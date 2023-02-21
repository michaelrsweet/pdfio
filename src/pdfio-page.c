//
// PDF page functions for PDFio.
//
// Copyright © 2021-2022 by Michael R Sweet.
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

static _pdfio_value_t	*get_contents(pdfio_obj_t *page);


//
// 'pdfioPageCopy()' - Copy a page to a PDF file.
//

bool					// O - `true` on success, `false` on failure
pdfioPageCopy(pdfio_file_t *pdf,	// I - PDF file
              pdfio_obj_t  *srcpage)	// I - Source page
{
  pdfio_obj_t	*dstpage;		// Destination page object


  PDFIO_DEBUG("pdfioPageCopy(pdf=%p, srcpage=%p(%p))\n", pdf, srcpage, srcpage ? srcpage->pdf : NULL);

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

  return (_pdfioDictGetValue(page->value.value.dict, "Contents"));
}
