//
// PDF object functions for PDFio.
//
// Copyright © 2021-2023 by Michael R Sweet.
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

  // Clear the current object pointer...
  obj->pdf->current_obj = NULL;

  if (obj->pdf->mode != _PDFIO_MODE_WRITE)
  {
    // Nothing to do when reading
    return (true);
  }

  // Write what remains for the object...
  if (!obj->offset)
  {
    // Write the object value
    if (!write_obj_header(obj))
      return (false);

    // Write the "endobj" line...
    return (_pdfioFilePuts(obj->pdf, "endobj\n"));
  }
  else if (obj->stream)
  {
    // Close the stream...
    return (pdfioStreamClose(obj->stream));
  }
  else
  {
    // Already closed
    return (true);
  }
}


//
// 'pdfioObjCopy()' - Copy an object to another PDF file.
//

pdfio_obj_t *				// O - New object or `NULL` on error
pdfioObjCopy(pdfio_file_t *pdf,		// I - PDF file
             pdfio_obj_t  *srcobj)	// I - Object to copy
{
  pdfio_obj_t	*dstobj;		// Destination object
  pdfio_stream_t *srcst,		// Source stream
		*dstst;			// Destination stream
  char		buffer[32768];		// Copy buffer
  ssize_t	bytes;			// Bytes read


  PDFIO_DEBUG("pdfioObjCopy(pdf=%p, srcobj=%p(%p))\n", pdf, srcobj, srcobj ? srcobj->pdf : NULL);

  // Range check input
  if (!pdf || !srcobj)
    return (NULL);

  // Load the object value if needed...
  if (srcobj->value.type == PDFIO_VALTYPE_NONE)
    _pdfioObjLoad(srcobj);

  // Create the new object...
  if ((dstobj = _pdfioFileCreateObj(pdf, srcobj->pdf, NULL)) == NULL)
    return (NULL);

  // Add new object to the cache of copied objects...
  if (!_pdfioFileAddMappedObj(pdf, dstobj, srcobj))
    return (NULL);

  // Copy the object's value...
  if (!_pdfioValueCopy(pdf, &dstobj->value, srcobj->pdf, &srcobj->value))
    return (NULL);

  if (dstobj->value.type == PDFIO_VALTYPE_DICT)
    _pdfioDictClear(dstobj->value.value.dict, "Length");

  if (srcobj->stream_offset)
  {
    // Copy stream data...
    if ((srcst = pdfioObjOpenStream(srcobj, false)) == NULL)
    {
      pdfioObjClose(dstobj);
      return (NULL);
    }

    if ((dstst = pdfioObjCreateStream(dstobj, PDFIO_FILTER_NONE)) == NULL)
    {
      pdfioStreamClose(srcst);
      pdfioObjClose(dstobj);
      return (NULL);
    }

    while ((bytes = pdfioStreamRead(srcst, buffer, sizeof(buffer))) > 0)
    {
      if (!pdfioStreamWrite(dstst, buffer, (size_t)bytes))
      {
        bytes = -1;
        break;
      }
    }

    pdfioStreamClose(srcst);
    pdfioStreamClose(dstst);

    if (bytes < 0)
      return (NULL);
  }
  else
    pdfioObjClose(dstobj);

  return (dstobj);
}


//
// 'pdfioObjCreateStream()' - Create an object (data) stream for writing.
//

pdfio_stream_t *			// O - Stream or `NULL` on error
pdfioObjCreateStream(
    pdfio_obj_t    *obj,		// I - Object
    pdfio_filter_t filter)		// I - Type of compression to apply
{
  pdfio_obj_t	*length_obj = NULL;	// Length object, if any


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
    _pdfioFileError(obj->pdf, "Unsupported filter value for PDFioObjCreateStream.");
    return (NULL);
  }

  if (obj->pdf->current_obj)
  {
    _pdfioFileError(obj->pdf, "Another object (%u) is already open.", (unsigned)obj->pdf->current_obj->number);
    return (NULL);
  }

  // Write the header...
  if (!_pdfioDictGetValue(obj->value.value.dict, "Length"))
  {
    if (obj->pdf->output_cb)
    {
      // Streaming via an output callback, so add a placeholder length object
      _pdfio_value_t	length_value;	// Length value

      length_value.type         = PDFIO_VALTYPE_NUMBER;
      length_value.value.number = 0.0f;

      length_obj = _pdfioFileCreateObj(obj->pdf, obj->pdf, &length_value);
      pdfioDictSetObj(obj->value.value.dict, "Length", length_obj);
    }
    else
    {
      // Need a Length key for the stream, add a placeholder that we can fill in
      // later...
      pdfioDictSetNumber(obj->value.value.dict, "Length", 0.0);
    }
  }

  if (!write_obj_header(obj))
    return (NULL);

  if (!_pdfioFilePuts(obj->pdf, "stream\n"))
    return (NULL);

  obj->stream_offset    = _pdfioFileTell(obj->pdf);
  obj->pdf->current_obj = obj;

  // Return the new stream...
  return (_pdfioStreamCreate(obj, length_obj, filter));
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
  {
    PDFIO_DEBUG("pdfioObjGetLength(obj=%p) returning %lu.\n", obj, (unsigned long)length);
    return (length);
  }

  if ((lenobj = pdfioDictGetObj(obj->value.value.dict, "Length")) == NULL)
  {
    _pdfioFileError(obj->pdf, "Unable to get length of stream.");
    return (0);
  }

  if (lenobj->value.type == PDFIO_VALTYPE_NONE)
    _pdfioObjLoad(lenobj);

  if (lenobj->value.type != PDFIO_VALTYPE_NUMBER || lenobj->value.value.number <= 0.0)
  {
    _pdfioFileError(obj->pdf, "Unable to get length of stream.");
    return (0);
  }

  PDFIO_DEBUG("pdfioObjGetLength(obj=%p) returning %lu.\n", obj, (unsigned long)lenobj->value.value.number);

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
// 'pdfioObjGetSubtype()' - Get an object's subtype.
//

const char *				// O - Object subtype
pdfioObjGetSubtype(pdfio_obj_t *obj)	// I - Object
{
  pdfio_dict_t	*dict;			// Object dictionary


  if ((dict = pdfioObjGetDict(obj)) == NULL)
    return (NULL);
  else
    return (pdfioDictGetName(dict, "Subtype"));
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
  char			line[64],	// Line from file
			*ptr;		// Pointer into line
  ssize_t		bytes;		// Bytes read
  _pdfio_token_t	tb;		// Token buffer/stack


  PDFIO_DEBUG("_pdfioObjLoad(obj=%p(%lu)), offset=%lu\n", obj, (unsigned long)obj->number, (unsigned long)obj->offset);

  // Seek to the start of the object and read its header...
  if (_pdfioFileSeek(obj->pdf, obj->offset, SEEK_SET) != obj->offset)
  {
    _pdfioFileError(obj->pdf, "Unable to seek to object %lu.", (unsigned long)obj->number);
    return (false);
  }

  if ((bytes = _pdfioFilePeek(obj->pdf, line, sizeof(line) - 1)) < 0)
  {
    _pdfioFileError(obj->pdf, "Unable to read header for object %lu.", (unsigned long)obj->number);
    return (false);
  }

  line[bytes] = '\0';

  PDFIO_DEBUG("_pdfioObjLoad: Header is '%s'.\n", line);

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

  if (strncmp(ptr, "obj", 3) || (ptr[3] && ptr[3] != '<' && ptr[3] != '[' && !isspace(ptr[3] & 255)))
  {
    _pdfioFileError(obj->pdf, "Bad header for object %lu.", (unsigned long)obj->number);
    return (false);
  }

  ptr += 3;
  while (*ptr && isspace(*ptr & 255))
    ptr ++;

  _pdfioFileConsume(obj->pdf, (size_t)(ptr - line));

  // Then grab the object value...
  _pdfioTokenInit(&tb, obj->pdf, (_pdfio_tconsume_cb_t)_pdfioFileConsume, (_pdfio_tpeek_cb_t)_pdfioFilePeek, obj->pdf);

  if (!_pdfioValueRead(obj->pdf, obj, &tb, &obj->value, 0))
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

  if (tb.bufptr && tb.bufptr < tb.bufend && (*(tb.bufptr) == 0x0d || *(tb.bufptr) == 0x0a))
    tb.bufptr ++;			// Skip trailing CR or LF after token

  _pdfioTokenFlush(&tb);

  if (!strcmp(line, "stream"))
  {
    // Yes, this is an embedded stream so save its location...
    obj->stream_offset = _pdfioFileTell(obj->pdf);
    PDFIO_DEBUG("_pdfioObjLoad: stream_offset=%lu.\n", (unsigned long)obj->stream_offset);
  }

  PDFIO_DEBUG("_pdfioObjLoad: ");
  PDFIO_DEBUG_VALUE(&obj->value);
  PDFIO_DEBUG("\n");

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

  if (obj->pdf->current_obj)
  {
    _pdfioFileError(obj->pdf, "Another object (%u) is already open.", (unsigned)obj->pdf->current_obj->number);
    return (NULL);
  }

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
  obj->pdf->current_obj = obj;

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

  if (!_pdfioValueWrite(obj->pdf, obj, &obj->value, &obj->length_offset))
    return (false);

  return (_pdfioFilePuts(obj->pdf, "\n"));
}
