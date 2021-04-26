//
// PDF object functions for pdfio.
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
// 'pdfioObjClose()' - Close an object, writing any data as needed to the PDF
//                     file.
//

bool					// O - `true` on success, `false` on failure
pdfioObjClose(pdfio_obj_t *obj)		// I - Object
{
  return (false);
}


//
// 'pdfioObjCreateStream()' - Create an object (data) stream for writing.
//

pdfio_stream_t *			// O - Stream or `NULL` on error
pdfioObjCreateStream(
    pdfio_obj_t    *obj,		// I - Object
    pdfio_filter_t filter)		// I - Type of compression to apply
{
}


//
// '_pdfioObjDelete()' - Free memory used by an object.
//

void
_pdfioObjDelete(pdfio_object_t *obj)	// I - Object
{
  if (obj)
    pdfioStreamClose(obj->stream);

  free(obj);
}


//
// 'pdfioObjGetDict()' - Get the dictionary associated with an object.
//

pdfio_dict_t *				// O - Dictionary or `NULL` on error
pdfioObjGetDict(pdfio_obj_t *obj)	// I - Object
{
  // TODO: Implement me
  (void)obj;

  return (NULL);
}


//
// '()' - .
//

int		pdfioObjGetGeneration(pdfio_obj_t *obj)
{
}


//
// '()' - .
//

int		pdfioObjGetNumber(pdfio_obj_t *obj)
{
}


//
// '()' - .
//

const char	*pdfioObjGetType(pdfio_obj_t *obj)
{
}


//
// 'pdfioObjOpenStream()' - Open an object's (data) stream for reading.
//

pdfio_stream_t *			// O - Stream or `NULL` on error
pdfioObjOpenStream(pdfio_obj_t *obj)	// I - Object
{
}
