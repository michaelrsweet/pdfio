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
// Local functions...
//

static ssize_t	stream_read(pdfio_stream_t *st, char *buffer, size_t bytes);


//
// 'pdfioStreamClose()' - Close a (data) stream in a PDF file.
//

bool					// O - `true` on success, `false` on failure
pdfioStreamClose(pdfio_stream_t *st)	// I - Stream
{
  // Range check input...
  if (!st)
    return (false);

  // Finish reads/writes and free memory...
  if (st->pdf->mode == _PDFIO_MODE_READ)
  {
    if (st->filter == PDFIO_FILTER_FLATE)
      inflateEnd(&(st->flate));
  }
  else
  {
    // TODO: Implement close for writing
    return (false);
  }

  free(st->pbuffers[0]);
  free(st->pbuffers[1]);
  free(st);

  return (true);
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
  size_t	remaining;		// Remaining bytes in buffer
  ssize_t	rbytes;			// Bytes read


  // Range check input...
  if (!st || st->pdf->mode != _PDFIO_MODE_READ || !bytes)
    return (false);

  // Skip bytes in the stream buffer until we've consumed the requested number
  // or get to the end of the stream...
  while ((remaining = (size_t)(st->bufend - st->bufptr)) < bytes)
  {
    bytes -= remaining;

    if ((rbytes = stream_read(st, st->buffer, sizeof(st->buffer))) > 0)
    {
      st->bufptr = st->buffer;
      st->bufend = st->buffer + rbytes;
    }
    else
    {
      st->bufptr = st->bufend = st->buffer;
      return (false);
    }
  }

  st->bufptr += bytes;

  return (true);
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
  // Range check input...
  if (!st || st->pdf->mode != _PDFIO_MODE_READ || !buffer || !bufsize)
    return (false);

  // Read using the token engine...
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
  pdfio_dict_t		*dict = pdfioObjGetDict(obj);
					// Object dictionary
  size_t		length;		// Length of stream


  // Allocate a new stream object...
  if ((st = (pdfio_stream_t *)calloc(1, sizeof(pdfio_stream_t))) == NULL)
  {
    _pdfioFileError(obj->pdf, "Unable to allocate memory for a stream.");
    return (NULL);
  }

  st->pdf = obj->pdf;
  st->obj = obj;

  _pdfioFileSeek(st->pdf, obj->stream_offset, SEEK_SET);

  if ((length = (size_t)pdfioDictGetNumber(dict, "Length")) == 0)
  {
    // Length must be an indirect reference...
    pdfio_obj_t	*lenobj;		// Length object

    if ((lenobj = pdfioDictGetObject(dict, "Length")) == NULL)
    {
      _pdfioFileError(obj->pdf, "Unable to get length of stream.");
      free(st);
      return (NULL);
    }

    if (lenobj->value.type == PDFIO_VALTYPE_NONE)
      _pdfioObjLoad(lenobj);

    if (lenobj->value.type != PDFIO_VALTYPE_NUMBER || lenobj->value.value.number <= 0.0f)
    {
      _pdfioFileError(obj->pdf, "Unable to get length of stream.");
      free(st);
      return (NULL);
    }

    length = (size_t)lenobj->value.value.number;
  }

  st->remaining = length;

  if (decode)
  {
    // Try to decode/decompress the contents of this object...
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
      pdfio_dict_t *params = pdfioDictGetDict(dict, "DecodeParms");
					// Decoding parameters
      int bpc = (int)pdfioDictGetNumber(params, "BitsPerComponent");
					// Bits per component
      int colors = (int)pdfioDictGetNumber(params, "Colors");
					// Number of colors
      int columns = (int)pdfioDictGetNumber(params, "Columns");
					// Number of columns
      int predictor = (int)pdfioDictGetNumber(params, "Predictor");
					// Predictory value, if any

      PDFIO_DEBUG("_pdfioStreamOpen: FlateDecode - BitsPerComponent=%d, Colors=%d, Columns=%d, Predictor=%d\n", bpc, colors, columns, predictor);

      st->filter = PDFIO_FILTER_FLATE;

      if (bpc == 0)
      {
        bpc = 8;
      }
      else if (bpc < 1 || bpc == 3 || (bpc > 4 && bpc < 8) || (bpc > 8 && bpc < 16) || bpc > 16)
      {
        _pdfioFileError(st->pdf, "Unsupported BitsPerColor value %d.", bpc);
        free(st);
        return (NULL);
      }

      if (colors == 0)
      {
        colors = 1;
      }
      else if (colors < 0 || colors > 4)
      {
        _pdfioFileError(st->pdf, "Unsupported Colors value %d.", colors);
        free(st);
        return (NULL);
      }

      if (columns == 0)
      {
        columns = 1;
      }
      else if (columns < 0)
      {
        _pdfioFileError(st->pdf, "Unsupported Columns value %d.", columns);
        free(st);
        return (NULL);
      }

      if ((predictor > 2 && predictor < 10) || predictor > 15)
      {
        _pdfioFileError(st->pdf, "Unsupported Predictor function %d.", predictor);
        free(st);
        return (NULL);
      }
      else if (predictor > 1)
      {
        // Using a predictor function
        st->predictor = (_pdfio_predictor_t)predictor;
        st->pbpixel   = (size_t)(bpc * colors + 7) / 8;
        st->pbsize    = (size_t)(bpc * colors * columns + 7) / 8;
        if (predictor >= 10)
          st->pbsize ++;		// Add PNG predictor byte

        if ((st->pbuffers[0] = calloc(1, st->pbsize)) == NULL || (st->pbuffers[1] = calloc(1, st->pbsize)) == NULL)
        {
          _pdfioFileError(st->pdf, "Unable to allocate %lu bytes for Predictor buffers.", (unsigned long)st->pbsize);
	  free(st->pbuffers[0]);
	  free(st->pbuffers[1]);
	  free(st);
	  return (NULL);
        }
      }
      else
        st->predictor = _PDFIO_PREDICTOR_NONE;

      st->flate.zalloc    = (alloc_func)0;
      st->flate.zfree     = (free_func)0;
      st->flate.opaque    = (voidpf)0;
      st->flate.next_in   = (Bytef *)st->cbuffer;
      st->flate.next_out  = NULL;
      st->flate.avail_in  = (uInt)_pdfioFileRead(st->pdf, st->cbuffer, sizeof(st->cbuffer));
      st->flate.avail_out = 0;

      if (inflateInit(&(st->flate)) != Z_OK)
      {
	_pdfioFileError(st->pdf, "Unable to start Flate filter.");
	free(st->pbuffers[0]);
	free(st->pbuffers[1]);
	free(st);
	return (NULL);
      }

      st->remaining -= st->flate.avail_in;
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
  size_t	remaining;		// Remaining bytes in buffer


  // Range check input...
  if (!st || st->pdf->mode != _PDFIO_MODE_READ || !buffer || !bytes)
    return (-1);

  // See if we have enough bytes in the buffer...
  if ((remaining = (size_t)(st->bufend - st->bufptr)) < bytes)
  {
    // No, shift the buffer and read more
    ssize_t	rbytes;			// Bytes read

    if (remaining > 0)
      memmove(st->buffer, st->bufptr, remaining);

    st->bufptr = st->buffer;
    st->bufend = st->buffer + remaining;

    if ((rbytes = stream_read(st, st->bufptr, sizeof(st->buffer) - remaining)) > 0)
    {
      st->bufend += rbytes;
      remaining  += (size_t)rbytes;
    }
  }

  // Copy bytes from the buffer...
  if (bytes > remaining)
    bytes = remaining;

  memcpy(buffer, st->bufptr, bytes);

  // Return the number of bytes that were copied...
  return ((ssize_t)bytes);
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


  // Range check input...
  if (!st || st->pdf->mode != _PDFIO_MODE_WRITE || !format)
    return (false);

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
  if (!st || st->pdf->mode != _PDFIO_MODE_WRITE || !s)
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
  char		*bufptr = (char *)buffer;
					// Pointer into buffer
  size_t	remaining;		// Remaining bytes in buffer
  ssize_t	rbytes;			// Bytes read


  // Range check input...
  if (!st || st->pdf->mode != _PDFIO_MODE_READ || !buffer || !bytes)
    return (-1);

  // Loop until we have the requested bytes or hit the end of the stream...
  while ((remaining = (size_t)(st->bufend - st->bufptr)) < bytes)
  {
    memcpy(bufptr, st->bufptr, remaining);
    bufptr += remaining;
    bytes -= remaining;

    if (bytes >= sizeof(st->buffer))
    {
      // Read large amounts directly to caller's buffer...
      if ((rbytes = stream_read(st, bufptr, bytes)) > 0)
      {
        bufptr += rbytes;
        bytes  = 0;
      }

      st->bufptr = st->bufend = st->buffer;
      break;
    }
    else if ((rbytes = stream_read(st, st->buffer, sizeof(st->buffer))) > 0)
    {
      st->bufptr = st->buffer;
      st->bufend = st->buffer + rbytes;
    }
    else
    {
      st->bufptr = st->bufend = st->buffer;
      bytes = 0;
      break;
    }
  }

  // Copy any remaining bytes from the stream buffer...
  if (bytes > 0)
  {
    memcpy(bufptr, st->bufptr, bytes);
    bufptr     += bytes;
    st->bufptr += bytes;
  }

  // Return the number of bytes that were read...
  return (bufptr - (char *)buffer);
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
  // Range check input...
  if (!st || st->pdf->mode != _PDFIO_MODE_WRITE || !buffer || !bytes)
    return (false);

  // TODO: Implement me
  return (false);
}


//
// 'stream_read()' - Read data from a stream, including filters.
//

static ssize_t				// O - Number of bytes read or `-1` on error
stream_read(pdfio_stream_t *st,		// I - Stream
            char           *buffer,	// I - Buffer
            size_t         bytes)	// I - Number of bytes to read
{
  ssize_t	rbytes;			// Bytes read


  if (st->filter == PDFIO_FILTER_NONE)
  {
    // No filtering, but limit reads to the length of the stream...
    if (bytes > st->remaining)
      rbytes = _pdfioFileRead(st->pdf, buffer, st->remaining);
    else
      rbytes = _pdfioFileRead(st->pdf, buffer, bytes);

    if (rbytes > 0)
      st->remaining -= (size_t)rbytes;

    return (rbytes);
  }
  else if (st->filter == PDFIO_FILTER_FLATE)
  {
    // Deflate compression...
    int	status;				// Status of decompression

    if (st->predictor == _PDFIO_PREDICTOR_NONE)
    {
      // Decompress into the buffer...
      PDFIO_DEBUG("stream_read: No predictor.\n");

      if (st->flate.avail_in == 0)
      {
	// Read more from the file...
	if (sizeof(st->cbuffer) > st->remaining)
	  rbytes = _pdfioFileRead(st->pdf, st->cbuffer, st->remaining);
	else
	  rbytes = _pdfioFileRead(st->pdf, st->cbuffer, sizeof(st->cbuffer));

	if (rbytes <= 0)
	  return (-1);			// End of file...

	st->remaining      -= (size_t)rbytes;
	st->flate.next_in  = (Bytef *)st->cbuffer;
	st->flate.avail_in = (uInt)rbytes;
      }

      st->flate.next_out  = (Bytef *)buffer;
      st->flate.avail_out = (uInt)bytes;

      if ((status = inflate(&(st->flate), Z_NO_FLUSH)) < Z_OK)
      {
	_pdfioFileError(st->pdf, "Unable to decompress stream data: %d", status);
	return (-1);
      }

      return (st->flate.next_out - (Bytef *)buffer);
    }
    else if (st->predictor == _PDFIO_PREDICTOR_TIFF2)
    {
      // TODO: Implement TIFF2 predictor
    }
    else
    {
      // PNG predictor
      size_t		pbpixel = st->pbpixel,
					// Size of pixel in bytes
      			remaining = st->pbsize - 1,
					// Remaining bytes
			firstcol = remaining - pbpixel;
					// First column bytes remaining
      unsigned char	*bufptr = (unsigned char *)buffer,
					// Pointer into buffer
			*thisptr = st->pbuffers[st->pbcurrent] + 1,
					// Current (raw) line
			*prevptr = st->pbuffers[!st->pbcurrent] + 1;
					// Previous (raw) line

      PDFIO_DEBUG("stream_read: PNG predictor.\n");

      if (bytes < (st->pbsize - 1))
      {
        // TODO: Support partial reads of PNG-encoded streams?
        _pdfioFileError(st->pdf, "Read buffer too small for stream.");
        return (-1);
      }

      st->flate.next_out  = (Bytef *)thisptr - 1;
      st->flate.avail_out = (uInt)st->pbsize;

      while (st->flate.avail_out > 0)
      {
	if (st->flate.avail_in == 0)
	{
	  // Read more from the file...
	  if (sizeof(st->cbuffer) > st->remaining)
	    rbytes = _pdfioFileRead(st->pdf, st->cbuffer, st->remaining);
	  else
	    rbytes = _pdfioFileRead(st->pdf, st->cbuffer, sizeof(st->cbuffer));

	  if (rbytes <= 0)
	    return (-1);			// End of file...

	  st->remaining      -= (size_t)rbytes;
	  st->flate.next_in  = (Bytef *)st->cbuffer;
	  st->flate.avail_in = (uInt)rbytes;
	}

	if ((status = inflate(&(st->flate), Z_NO_FLUSH)) < Z_OK)
	{
	  _pdfioFileError(st->pdf, "Unable to decompress stream data: %d", status);
	  return (-1);
	}
	else if (status == Z_STREAM_END)
	  break;
      }

      if (st->flate.avail_out > 0)
        return (-1);				// Early end of stream

      // Apply predictor for this line
      PDFIO_DEBUG("stream_read: Line %02X %02X %02X %02X %02X.\n", thisptr[-1], thisptr[0], thisptr[0], thisptr[2], thisptr[3]);

      switch (thisptr[-1])
      {
        case 0 : // None
            memcpy(buffer, thisptr, remaining);
            break;
        case 1 : // Sub
            for (; remaining > firstcol; remaining --, thisptr ++)
              *bufptr++ = *thisptr;
            for (; remaining > 0; remaining --, thisptr ++)
              *bufptr++ = *thisptr + thisptr[-pbpixel];
            break;
        case 2 : // Up
            for (; remaining > 0; remaining --, thisptr ++, prevptr ++)
              *bufptr++ = *thisptr + *prevptr;
            break;
        case 3 : // Average
	    for (; remaining > firstcol; remaining --, thisptr ++, prevptr ++)
	      *bufptr++ = *thisptr + *prevptr / 2;
	    for (; remaining > 0; remaining --, thisptr ++, prevptr ++)
	      *bufptr++ = *thisptr + (thisptr[-pbpixel] + *prevptr) / 2;
            break;
        case 4 : // Paeth
            // TODO: Implement Paeth predictor
            memcpy(buffer, thisptr, remaining);
            break;

        default :
            _pdfioFileError(st->pdf, "Bad PNG filter %d in data stream.", thisptr[-1]);
            return (-1);
      }

      // Copy the computed line and swap buffers...
      memcpy(st->pbuffers[st->pbcurrent] + 1, buffer, st->pbsize - 1);
      st->pbcurrent = !st->pbcurrent;

      // Return the number of bytes we copied for this line...
      return ((ssize_t)(st->pbsize - 1));
    }
  }

  // If we get here something bad happened...
  return (-1);
}

