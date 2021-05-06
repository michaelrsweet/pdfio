//
// PDF stream functions for pdfio.
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
// 'pdfioStreamClose()' - Close a (data) stream in a PDF file.
//

bool					// O - `true` on success, `false` on failure
pdfioStreamClose(pdfio_stream_t *st)	// I - Stream
{
  // TODO: Implement me
  (void)st;
  return (false);
}


//
// '_pdfioStreamCreate()' - Create a stream for writing.
//
// Note: pdfioObjCreateStream handles writing the object and its dictionary.
//

pdfio_stream_t *			// O - Stream or `NULL` on error
_pdfioStreamCreate(
    pdfio_obj_t    *obj,		// I - Object
    pdfio_filter_t compression)		// I - Compression to apply
{
  // TODO: Implement me
  (void)obj;
  (void)compression;

  return (NULL);
}


//
// 'pdfioStreamConsume()' - Consume bytes from the stream.
//

bool					// O - `true` on success, `false` on EOF
pdfioStreamConsume(pdfio_stream_t *st,	// I - Stream
                   size_t         bytes)// I - Number of bytes to consume
{
  // TODO: Implement me
  (void)st;
  (void)bytes;
  return (false);
}


//
// '_pdfioStreamDelete()' - Free all memory used by a stream.
//

void
_pdfioStreamDelete(pdfio_stream_t *st)	// I - Stream
{
  if (st->filter == PDFIO_FILTER_FLATE)
  {
    // Free memory used for flate compression/decompression...
    if (st->pdf->mode == _PDFIO_MODE_READ)
      inflateEnd(&st->flate);
    else // mode == _PDFIO_MODE_WRITE
      deflateEnd(&st->flate);
  }

  free(st);
}


//
// 'pdfioStreamGetToken()' - Read a single PDF token from a stream.
//

bool					// O - `true` on success, `false` on EOF
pdfioStreamGetToken(
    pdfio_stream_t *st,			// I - Stream
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  return (_pdfioTokenRead(st->pdf, buffer, bufsize, (_pdfio_tpeek_cb_t)pdfioStreamPeek, (_pdfio_tconsume_cb_t)pdfioStreamConsume, st));
}


//
// '_pdfioStreamOpen()' - Create a stream for reading.
//
// Note: pdfioObjOpenStream handles loading the object's dictionary and
// getting the start of the stream data.
//

pdfio_stream_t *			// O - Stream or `NULL` on error
_pdfioStreamOpen(pdfio_obj_t *obj,	// I - Object
                 bool        decode)	// I - Decode/decompress the stream?
{
  pdfio_stream_t	*st;		// Stream


  // Allocate a new stream object...
  if ((st = (pdfio_stream_t *)calloc(1, sizeof(pdfio_stream_t))) == NULL)
  {
    _pdfioFileError(obj->pdf, "Unable to allocate memory for a stream.");
    return (NULL);
  }

  st->pdf = obj->pdf;
  st->obj = obj;

  _pdfioFileSeek(st->pdf, obj->stream_offset, SEEK_SET);

  if (decode)
  {
    // Try to decode/decompress the contents of this object...
    pdfio_dict_t *dict = pdfioObjGetDict(obj);
					// Object dictionary
    const char	*filter = pdfioDictGetName(dict, "Filter");
					// Filter value

    if (!filter)
    {
      // No single filter name, do we have a compound filter?
      if (pdfioDictGetArray(dict, "Filter"))
      {
	// TODO: Implement compound filters...
	_pdfioFileError(st->pdf, "Unsupported compound stream filter.");
	free(st);
	return (NULL);
      }

      // No filter, read as-is...
      st->filter = PDFIO_FILTER_NONE;
    }
    else if (!strcmp(filter, "FlateDecode"))
    {
      // Flate compression
      int bpc = (int)pdfioDictGetNumber(dict, "BitsPerComponent");
					// Bits per component
      int colors = (int)pdfioDictGetNumber(dict, "Colors");
					// Number of colors
      int columns = (int)pdfioDictGetNumber(dict, "Columns");
					// Number of columns
      int predictor = (int)pdfioDictGetNumber(dict, "Predictor");
					// Predictory value, if any

      st->filter = PDFIO_FILTER_FLATE;
    }
    else if (!strcmp(filter, "LZWDecode"))
    {
      // LZW compression
      st->filter = PDFIO_FILTER_LZW;
    }
    else
    {
      // Something else we don't support
      _pdfioFileError(st->pdf, "Unsupported stream filter '/%s'.", filter);
      free(st);
      return (NULL);
    }
  }
  else
  {
    // Just return the stream data as-is...
    st->filter = PDFIO_FILTER_NONE;
  }

  return (st);
}


//
// 'pdfioStreamPeek()' - Peek at data in a stream.
//

ssize_t					// O - Bytes returned or `-1` on error
pdfioStreamPeek(pdfio_stream_t *st,	// I - Stream
                void           *buffer,	// I - Buffer
                size_t         bytes)	// I - Size of buffer
{
  // TODO: Implement me
  (void)st;
  (void)buffer;
  (void)bytes;

  return (-1);
}


//
// 'pdfioStreamPrintf()' - Write a formatted string to a stream.
//

bool					// O - `true` on success, `false` on failure
pdfioStreamPrintf(
    pdfio_stream_t *st,			// I - Stream
    const char     *format,		// I - `printf`-style format string
    ...)				// I - Additional arguments as needed
{
  char		buffer[8192];		// String buffer
  va_list	ap;			// Argument pointer


  // Format the string...
  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  // Write the string...
  return (pdfioStreamWrite(st, buffer, strlen(buffer)));
}


//
// 'pdfioStreamPuts()' - Write a literal string to a stream.
//

bool					// O - `true` on success, `false` on failure
pdfioStreamPuts(pdfio_stream_t *st,	// I - Stream
                const char     *s)	// I - Literal string
{
  if (!st || !s)
    return (false);
  else
    return (pdfioStreamWrite(st, s, strlen(s)));
}


//
// 'pdfioStreamRead()' - Read data from a stream.
//

ssize_t					// O - Number of bytes read or `-1` on error
pdfioStreamRead(
    pdfio_stream_t *st,			// I - Stream
    void           *buffer,		// I - Buffer
    size_t         bytes)		// I - Bytes to read
{
  // TODO: Implement me
  (void)st;
  (void)buffer;
  (void)bytes;

  return (-1);
}


//
// 'pdfioStreamWrite()' - Write data to a stream.
//

bool					// O - `true` on success or `false` on failure
pdfioStreamWrite(
    pdfio_stream_t *st,			// I - Stream
    const void     *buffer,		// I - Data to write
    size_t         bytes)		// I - Number of bytes to write
{
  // TODO: Implement me
  (void)st;
  (void)buffer;
  (void)bytes;

  return (false);
}
