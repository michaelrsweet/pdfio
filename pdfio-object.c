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
// Local functions...
//

static bool	write_obj_header(pdfio_obj_t *obj);


//
// 'pdfioObjClose()' - Close an object, writing any data as needed to the PDF
//                     file.
//

bool					// O - `true` on success, `false` on failure
pdfioObjClose(pdfio_obj_t *obj)		// I - Object
{
  // Range check input
  if (!obj)
    return (false);

  if (obj->pdf->mode != _PDFIO_MODE_WRITE)
    return (true);			// Nothing to do when reading

  // Write what remains for the object...
  if (!obj->offset)
  {
    // Write the object value
    if (!write_obj_header(obj))
      return (false);
  }
  else if (obj->stream)
  {
    // Close the stream...
    if (!pdfioStreamClose(obj->stream))
      return (false);
  }
  else
  {
    // Already closed
    return (true);
  }

  // If we get here we wrote the object header or closed the stream and still
  // need to write the "endobj" line...
  return (_pdfioFilePuts(obj->pdf, "endobj\n"));
}


//
// 'pdfioObjCopy()' - Copy an object to another PDF file.
//

pdfio_obj_t *				// O - New object or `NULL` on error
pdfioObjCopy(pdfio_file_t *pdf,		// I - PDF file
             pdfio_obj_t  *srcobj)	// I - Object to copy
{
  // TODO: Implement pdfioObjCopy
  (void)pdf;
  (void)srcobj;

  return (NULL);
}


//
// 'pdfioObjCreateStream()' - Create an object (data) stream for writing.
//

pdfio_stream_t *			// O - Stream or `NULL` on error
pdfioObjCreateStream(
    pdfio_obj_t    *obj,		// I - Object
    pdfio_filter_t filter)		// I - Type of compression to apply
{
  // Range check input
  if (!obj || obj->pdf->mode != _PDFIO_MODE_WRITE || obj->value.type != PDFIO_VALTYPE_DICT)
    return (NULL);

  if (obj->offset)
  {
    _pdfioFileError(obj->pdf, "Object has already been written.");
    return (NULL);
  }

  if (filter != PDFIO_FILTER_NONE && filter != PDFIO_FILTER_FLATE)
  {
    _pdfioFileError(obj->pdf, "Unsupported filter value for pdfioObjCreateStream.");
    return (NULL);
  }

  // Write the header...
  if (!_pdfioDictGetValue(obj->value.value.dict, "Length"))
  {
    // Need a Length key for the stream, add a placeholder that we can fill in
    // later...
    pdfioDictSetNumber(obj->value.value.dict, "Length", 0.0f);
  }

  if (!write_obj_header(obj))
    return (NULL);

  if (!_pdfioFilePuts(obj->pdf, "stream\n"))
    return (NULL);

  obj->stream_offset = _pdfioFileTell(obj->pdf);

  // Return the new stream...
  return (_pdfioStreamCreate(obj, filter));
}


//
// '_pdfioObjDelete()' - Free memory used by an object.
//

void
_pdfioObjDelete(pdfio_obj_t *obj)	// I - Object
{
  if (obj)
    pdfioStreamClose(obj->stream);

  free(obj);
}


//
// 'pdfioObjGetArray()' - Get the array associated with an object.
//

pdfio_array_t *				// O - Array or `NULL` on error
pdfioObjGetArray(pdfio_obj_t *obj)	// I - Object
{
  if (!obj)
    return (NULL);

  if (obj->value.type == PDFIO_VALTYPE_NONE)
    _pdfioObjLoad(obj);

  if (obj->value.type == PDFIO_VALTYPE_ARRAY)
    return (obj->value.value.array);
  else
    return (NULL);
}


//
// 'pdfioObjGetDict()' - Get the dictionary associated with an object.
//

pdfio_dict_t *				// O - Dictionary or `NULL` on error
pdfioObjGetDict(pdfio_obj_t *obj)	// I - Object
{
  if (!obj)
    return (NULL);

  if (obj->value.type == PDFIO_VALTYPE_NONE)
    _pdfioObjLoad(obj);

  if (obj->value.type == PDFIO_VALTYPE_DICT)
    return (obj->value.value.dict);
  else
    return (NULL);
}


//
// 'pdfioObjGetGeneration()' - Get the object's generation number.
//

unsigned short				// O - Generation number (0 to 65535)
pdfioObjGetGeneration(pdfio_obj_t *obj)	// I - Object
{
  return (obj ? obj->generation : 0);
}


//
// 'pdfioObjGetLength()' - Get the length of the object's (data) stream.
//

size_t					// O - Length in bytes or `0` for none
pdfioObjGetLength(pdfio_obj_t *obj)	// I - Object
{
  size_t	length;			// Length of stream
  pdfio_obj_t	*lenobj;		// Length object


  // Range check input...
  if (!obj || !obj->stream_offset || obj->value.type != PDFIO_VALTYPE_DICT)
    return (0);

  // Try getting the length, directly or indirectly
  if ((length = (size_t)pdfioDictGetNumber(obj->value.value.dict, "Length")) > 0)
    return (length);

  if ((lenobj = pdfioDictGetObject(obj->value.value.dict, "Length")) == NULL)
  {
    _pdfioFileError(obj->pdf, "Unable to get length of stream.");
    return (0);
  }

  if (lenobj->value.type == PDFIO_VALTYPE_NONE)
    _pdfioObjLoad(lenobj);

  if (lenobj->value.type != PDFIO_VALTYPE_NUMBER || lenobj->value.value.number <= 0.0f)
  {
    _pdfioFileError(obj->pdf, "Unable to get length of stream.");
    return (0);
  }

  return ((size_t)lenobj->value.value.number);
}


//
// 'pdfioObjGetNumber()' - Get the object's number.
//

size_t					// O - Object number (1 to 9999999999)
pdfioObjGetNumber(pdfio_obj_t *obj)	// I - Object
{
  return (obj ? obj->number : 0);
}


//
// 'pdfioObjGetType()' - Get an object's type.
//

const char *				// O - Object type
pdfioObjGetType(pdfio_obj_t *obj)	// I - Object
{
  pdfio_dict_t	*dict;			// Object dictionary


  if ((dict = pdfioObjGetDict(obj)) == NULL)
    return (NULL);
  else
    return (pdfioDictGetName(dict, "Type"));
}


//
// '_pdfioObjLoad()' - Load an object dictionary/value.
//

bool					// O - `true` on success, `false` otherwise
_pdfioObjLoad(pdfio_obj_t *obj)		// I - Object
{
  char			line[1024],	// Line from file
			*ptr;		// Pointer into line
  _pdfio_token_t	tb;		// Token buffer/stack


  PDFIO_DEBUG("_pdfioObjLoad(obj=%p(%lu)), offset=%lu\n", obj, (unsigned long)obj->number, (unsigned long)obj->offset);

  // Seek to the start of the object and read its header...
  if (_pdfioFileSeek(obj->pdf, obj->offset, SEEK_SET) != obj->offset)
  {
    _pdfioFileError(obj->pdf, "Unable to seek to object %lu.", (unsigned long)obj->number);
    return (false);
  }

  if (!_pdfioFileGets(obj->pdf, line, sizeof(line)))
  {
    _pdfioFileError(obj->pdf, "Unable to read header for object %lu.", (unsigned long)obj->number);
    return (false);
  }

  if (strtoimax(line, &ptr, 10) != (intmax_t)obj->number)
  {
    _pdfioFileError(obj->pdf, "Bad header for object %lu.", (unsigned long)obj->number);
    return (false);
  }

  if (strtol(ptr, &ptr, 10) != (long)obj->generation)
  {
    _pdfioFileError(obj->pdf, "Bad header for object %lu.", (unsigned long)obj->number);
    return (false);
  }

  while (isspace(*ptr & 255))
    ptr ++;

  if (strcmp(ptr, "obj"))
  {
    _pdfioFileError(obj->pdf, "Bad header for object %lu.", (unsigned long)obj->number);
    return (false);
  }

  // Then grab the object value...
  _pdfioTokenInit(&tb, obj->pdf, (_pdfio_tconsume_cb_t)_pdfioFileConsume, (_pdfio_tpeek_cb_t)_pdfioFilePeek, obj->pdf);

  if (!_pdfioValueRead(obj->pdf, &tb, &obj->value))
  {
    _pdfioFileError(obj->pdf, "Unable to read value for object %lu.", (unsigned long)obj->number);
    return (false);
  }

  // Now see if there is an associated stream...
  if (!_pdfioTokenGet(&tb, line, sizeof(line)))
  {
    _pdfioFileError(obj->pdf, "Early end-of-file for object %lu.", (unsigned long)obj->number);
    return (false);
  }

  _pdfioTokenFlush(&tb);

  if (!strcmp(line, "stream"))
  {
    // Yes, save its location...
    obj->stream_offset = _pdfioFileTell(obj->pdf);
  }

  return (true);
}


//
// 'pdfioObjOpenStream()' - Open an object's (data) stream for reading.
//

pdfio_stream_t *			// O - Stream or `NULL` on error
pdfioObjOpenStream(pdfio_obj_t *obj,	// I - Object
                   bool        decode)	// I - Decode/decompress data?
{
  // Range check input...
  if (!obj)
    return (NULL);

  // Make sure we've loaded the object dictionary...
  if (!obj->value.type)
  {
    if (!_pdfioObjLoad(obj))
      return (NULL);
  }

  // No stream if there is no dict or offset to a stream...
  if (obj->value.type != PDFIO_VALTYPE_DICT || !obj->stream_offset)
    return (NULL);

  // Open the stream...
  return (_pdfioStreamOpen(obj, decode));
}


//
// 'write_obj_header()' - Write the object header...
//

static bool				// O - `true` on success, `false` on failure
write_obj_header(pdfio_obj_t *obj)	// I - Object
{
  obj->offset = _pdfioFileTell(obj->pdf);

  if (!_pdfioFilePrintf(obj->pdf, "%lu %u obj\n", (unsigned long)obj->number, obj->generation))
    return (false);

  if (!_pdfioValueWrite(obj->pdf, &obj->value, &obj->length_offset))
    return (false);

  return (_pdfioFilePuts(obj->pdf, "\n"));
}
