//
// PDF page functions for pdfio.
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
