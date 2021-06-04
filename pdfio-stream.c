//
// PDF stream functions for PDFio.
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

static unsigned char	stream_paeth(unsigned char a, unsigned char b, unsigned char c);
static ssize_t		stream_read(pdfio_stream_t *st, char *buffer, size_t bytes);
static bool		stream_write(pdfio_stream_t *st, const void *buffer, size_t bytes);


//
// 'pdfioStreamClose()' - Close a (data) stream in a PDF file.
//

bool					// O - `true` on success, `false` on failure
pdfioStreamClose(pdfio_stream_t *st)	// I - Stream
{
  bool ret = true;			// Return value


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
    // Close stream for writing...
    if (st->filter == PDFIO_FILTER_FLATE)
    {
      // Finalize flate compression stream...
      int status;			// Deflate status

      while ((status = deflate(&st->flate, Z_FINISH)) != Z_STREAM_END)
      {
	if (status < Z_OK && status != Z_BUF_ERROR)
	{
	  _pdfioFileError(st->pdf, "Flate compression failed (%d)", status);
	  ret = false;
	  goto done;
	}

	if (!_pdfioFileWrite(st->pdf, st->cbuffer, sizeof(st->cbuffer) - st->flate.avail_out))
	{
	  ret = false;
	  goto done;
	}

	st->flate.next_out  = (Bytef *)st->cbuffer;
	st->flate.avail_out = (uInt)sizeof(st->cbuffer);
      }

      if (st->flate.avail_out < (uInt)sizeof(st->cbuffer))
      {
        // Write any residuals...
	if (!_pdfioFileWrite(st->pdf, st->cbuffer, sizeof(st->cbuffer) - st->flate.avail_out))
	{
	  ret = false;
	  goto done;
	}
      }
    }

    // Save the length of this stream...
    st->obj->stream_length = (size_t)(_pdfioFileTell(st->pdf) - st->obj->stream_offset);

    // End of stream marker...
    if (!_pdfioFilePuts(st->pdf, "\nendstream\nendobj\n"))
    {
      ret = false;
      goto done;
    }

    // Update the length as needed...
    if (st->obj->length_offset)
    {
      // Seek back to the "/Length 9999999999" we wrote...
      if (_pdfioFileSeek(st->pdf, st->obj->length_offset, SEEK_SET) < 0)
      {
        ret = false;
        goto done;
      }

      // Write the updated length value...
      if (!_pdfioFilePrintf(st->pdf, "%-10lu", (unsigned long)st->obj->stream_length))
      {
        ret = false;
        goto done;
      }

      // Seek to the end of the PDF file...
      if (_pdfioFileSeek(st->pdf, 0, SEEK_END) < 0)
      {
        ret = false;
        goto done;
      }
    }
  }

  done:

  free(st->prbuffer);
  free(st->psbuffer);
  free(st);

  return (ret);
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
  pdfio_stream_t	*st;		// Stream


  // Allocate a new stream object...
  if ((st = (pdfio_stream_t *)calloc(1, sizeof(pdfio_stream_t))) == NULL)
  {
    _pdfioFileError(obj->pdf, "Unable to allocate memory for a stream.");
    return (NULL);
  }

  st->pdf    = obj->pdf;
  st->obj    = obj;
  st->filter = compression;

  if (compression == PDFIO_FILTER_FLATE)
  {
    // Flate compression
    pdfio_dict_t *params = pdfioDictGetDict(obj->value.value.dict, "DecodeParms");
					// Decoding parameters
    int bpc = (int)pdfioDictGetNumber(params, "BitsPerComponent");
					// Bits per component
    int colors = (int)pdfioDictGetNumber(params, "Colors");
					// Number of colors
    int columns = (int)pdfioDictGetNumber(params, "Columns");
					// Number of columns
    int predictor = (int)pdfioDictGetNumber(params, "Predictor");
					// Predictory value, if any

    PDFIO_DEBUG("_pdfioStreamCreate: FlateDecode - BitsPerComponent=%d, Colors=%d, Columns=%d, Predictor=%d\n", bpc, colors, columns, predictor);

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

    if ((predictor > 1 && predictor < 10) || predictor > 15)
    {
      _pdfioFileError(st->pdf, "Unsupported Predictor function %d.", predictor);
      free(st);
      return (NULL);
    }
    else if (predictor)
    {
      // Using a PNG predictor function
      st->predictor = (_pdfio_predictor_t)predictor;
      st->pbpixel   = (size_t)(bpc * colors + 7) / 8;
      st->pbsize    = (size_t)(bpc * colors * columns + 7) / 8;
      if (predictor >= 10)
	st->pbsize ++;		// Add PNG predictor byte

      fprintf(stderr, "colors=%d, bpc=%d, pbpixel=%u\n", colors, bpc, (unsigned)st->pbpixel);
      if ((st->prbuffer = calloc(1, st->pbsize - 1)) == NULL || (st->psbuffer = calloc(1, st->pbsize)) == NULL)
      {
	_pdfioFileError(st->pdf, "Unable to allocate %lu bytes for Predictor buffers.", (unsigned long)st->pbsize);
	free(st->prbuffer);
	free(st->psbuffer);
	free(st);
	return (NULL);
      }
    }
    else
      st->predictor = _PDFIO_PREDICTOR_NONE;

    st->flate.next_out  = (Bytef *)st->cbuffer;
    st->flate.avail_out = (uInt)sizeof(st->cbuffer);

    if (deflateInit(&(st->flate), 9) != Z_OK)
    {
      _pdfioFileError(st->pdf, "Unable to start Flate filter.");
      free(st->prbuffer);
      free(st->psbuffer);
      free(st);
      return (NULL);
    }
  }

  return (st);
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
  _pdfio_token_t	tb;		// Token buffer/stack


  // Range check input...
  if (!st || st->pdf->mode != _PDFIO_MODE_READ || !buffer || !bufsize)
    return (false);

  // Read using the token engine...
  _pdfioTokenInit(&tb, st->pdf, (_pdfio_tconsume_cb_t)pdfioStreamConsume, (_pdfio_tpeek_cb_t)pdfioStreamPeek, st);

  return (_pdfioTokenRead(&tb, buffer, bufsize));
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


  // Allocate a new stream object...
  if ((st = (pdfio_stream_t *)calloc(1, sizeof(pdfio_stream_t))) == NULL)
  {
    _pdfioFileError(obj->pdf, "Unable to allocate memory for a stream.");
    return (NULL);
  }

  st->pdf = obj->pdf;
  st->obj = obj;

  _pdfioFileSeek(st->pdf, obj->stream_offset, SEEK_SET);

  if ((st->remaining = pdfioObjGetLength(obj)) == 0)
  {
    free(st);
    return (NULL);
  }

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

        if ((st->prbuffer = calloc(1, st->pbsize - 1)) == NULL || (st->psbuffer = calloc(1, st->pbsize)) == NULL)
        {
          _pdfioFileError(st->pdf, "Unable to allocate %lu bytes for Predictor buffers.", (unsigned long)st->pbsize);
	  free(st->prbuffer);
	  free(st->psbuffer);
	  free(st);
	  return (NULL);
        }
      }
      else
        st->predictor = _PDFIO_PREDICTOR_NONE;

      st->flate.next_in  = (Bytef *)st->cbuffer;
      st->flate.avail_in = (uInt)_pdfioFileRead(st->pdf, st->cbuffer, sizeof(st->cbuffer));

      if (inflateInit(&(st->flate)) != Z_OK)
      {
	_pdfioFileError(st->pdf, "Unable to start Flate filter.");
	free(st->prbuffer);
	free(st->psbuffer);
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
// This function reads data from a stream.  When reading decoded image data
// from a stream, you *must* read whole scanlines.  The
// @link pdfioImageGetBytesPerLine@ function can be used to determine the
// proper read length.
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
        bufptr += rbytes;

      bytes      = 0;
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
  size_t		pbpixel = st->pbpixel,
					// Size of pixel in bytes
      			pbline = st->pbsize - 1,
					// Bytes per line
			remaining;	// Remaining bytes on this line
  const unsigned char	*bufptr = (const unsigned char *)buffer,
					// Pointer into buffer
			*bufsecond = buffer + pbpixel;
					// Pointer to second pixel in buffer
  unsigned char		*sptr,		// Pointer into sbuffer
			*pptr;		// Previous raw buffer


  PDFIO_DEBUG("pdfioStreamWrite(st=%p, buffer=%p, bytes=%lu)\n", st, buffer, (unsigned long)bytes);

  // Range check input...
  if (!st || st->pdf->mode != _PDFIO_MODE_WRITE || !buffer || !bytes)
    return (false);

  // Write it...
  if (st->filter == PDFIO_FILTER_NONE)
  {
    // No filtering so just write it...
    return (_pdfioFileWrite(st->pdf, buffer, bytes));
  }

  if (st->predictor == _PDFIO_PREDICTOR_NONE)
  {
    // No predictor, just write it out straight...
    return (stream_write(st, buffer, bytes));
  }
  else if ((bytes % pbline) != 0)
  {
    _pdfioFileError(st->pdf, "Write buffer size must be a multiple of a complete row.");
    return (false);
  }

  while (bytes > 0)
  {
    // Store the PNG predictor in the first byte of the buffer...
    if (st->predictor == _PDFIO_PREDICTOR_PNG_AUTO)
      st->psbuffer[0] = 4;		// Use Paeth predictor for auto...
    else
      st->psbuffer[0] = (unsigned char)(st->predictor - 10);

    // Then process the current line using the specified PNG predictor...
    sptr = st->psbuffer + 1;
    pptr = st->prbuffer;

    switch (st->predictor)
    {
      default :
          // Anti-compiler-warning code (NONE is handled above, TIFF is not supported for writing)
	  return (false);

      case _PDFIO_PREDICTOR_PNG_NONE :
          // No PNG predictor...
          memcpy(sptr, buffer, pbline);
          break;

      case _PDFIO_PREDICTOR_PNG_SUB :
	  // Encode the difference from the previous column
	  for (remaining = pbline; remaining > 0; remaining --, bufptr ++, sptr ++)
	  {
	    if (bufptr >= bufsecond)
	      *sptr = *bufptr - bufptr[-pbpixel];
	    else
	      *sptr = *bufptr;
	  }
	  break;

      case _PDFIO_PREDICTOR_PNG_UP :
	  // Encode the difference from the previous line
	  for (remaining = pbline; remaining > 0; remaining --, bufptr ++, sptr ++, pptr ++)
	  {
	    *sptr = *bufptr - *pptr;
	  }
	  break;

      case _PDFIO_PREDICTOR_PNG_AVERAGE :
          // Encode the difference with the average of the previous column and line
	  for (remaining = pbline; remaining > 0; remaining --, bufptr ++, sptr ++, pptr ++)
	  {
	    if (bufptr >= bufsecond)
	      *sptr = *bufptr - (bufptr[-pbpixel] + *pptr) / 2;
	    else
	      *sptr = *bufptr - *pptr / 2;
	  }
	  break;

      case _PDFIO_PREDICTOR_PNG_PAETH :
      case _PDFIO_PREDICTOR_PNG_AUTO :
          // Encode the difference with a linear transform function
	  for (remaining = pbline; remaining > 0; remaining --, bufptr ++, sptr ++, pptr ++)
	  {
	    if (bufptr >= bufsecond)
	      *sptr = *bufptr - stream_paeth(bufptr[-pbpixel], *pptr, pptr[-pbpixel]);
	    else
	      *sptr = *bufptr - stream_paeth(0, *pptr, 0);
	  }
	  break;
    }

    // Write the encoded line...
    if (!stream_write(st, st->psbuffer, st->pbsize))
      return (false);

    memcpy(st->prbuffer, buffer, pbline);
    bytes -= pbline;
  }

  return (true);
}


//
// 'stream_paeth()' - PaethPredictor function for PNG decompression filter.
//

static unsigned char			// O - Predictor value
stream_paeth(unsigned char a,		// I - Left pixel
             unsigned char b,		// I - Top pixel
             unsigned char c)		// I - Top-left pixel
{
  int	p = a + b - c;			// Initial estimate
  int	pa = abs(p - a);		// Distance to a
  int	pb = abs(p - b);		// Distance to b
  int	pc = abs(p - c);		// Distance to c

  return ((pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c);
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
      			remaining = st->pbsize - 1;
					// Remaining bytes
      unsigned char	*bufptr = (unsigned char *)buffer,
					// Pointer into buffer
			*bufsecond = (unsigned char *)buffer + pbpixel,
					// Pointer to second pixel in buffer
			*sptr = st->psbuffer + 1,
					// Current (raw) line
			*pptr = st->prbuffer;
					// Previous (raw) line

      PDFIO_DEBUG("stream_read: PNG predictor.\n");

      if (bytes < (st->pbsize - 1))
      {
        _pdfioFileError(st->pdf, "Read buffer too small for stream.");
        return (-1);
      }

      st->flate.next_out  = (Bytef *)sptr - 1;
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
      PDFIO_DEBUG("stream_read: Line %02X %02X %02X %02X %02X.\n", sptr[-1], sptr[0], sptr[0], sptr[2], sptr[3]);

      switch (sptr[-1])
      {
        case 0 : // None
            memcpy(buffer, sptr, remaining);
            break;
        case 1 : // Sub
            for (; bufptr < bufsecond; remaining --, sptr ++)
              *bufptr++ = *sptr;
            for (; remaining > 0; remaining --, sptr ++, bufptr ++)
              *bufptr = *sptr + bufptr[-pbpixel];
            break;
        case 2 : // Up
            for (; remaining > 0; remaining --, sptr ++, pptr ++)
              *bufptr++ = *sptr + *pptr;
            break;
        case 3 : // Average
	    for (; bufptr < bufsecond; remaining --, sptr ++, pptr ++)
	      *bufptr++ = *sptr + *pptr / 2;
	    for (; remaining > 0; remaining --, sptr ++, pptr ++, bufptr ++)
	      *bufptr = *sptr + (bufptr[-pbpixel] + *pptr) / 2;
            break;
        case 4 : // Paeth
            for (; bufptr < bufsecond; remaining --, sptr ++, pptr ++)
              *bufptr++ = *sptr + stream_paeth(0, *pptr, 0);
            for (; remaining > 0; remaining --, sptr ++, pptr ++, bufptr ++)
              *bufptr = *sptr + stream_paeth(bufptr[-pbpixel], *pptr, pptr[-pbpixel]);
            break;

        default :
            _pdfioFileError(st->pdf, "Bad PNG filter %d in data stream.", sptr[-1]);
            return (-1);
      }

      // Copy the computed line and swap buffers...
      memcpy(st->prbuffer, buffer, st->pbsize - 1);

      // Return the number of bytes we copied for this line...
      return ((ssize_t)(st->pbsize - 1));
    }
  }

  // If we get here something bad happened...
  return (-1);
}


//
// 'stream_write()' - Write flate-compressed data...
//

static bool				// O - `true` on success, `false` on failure
stream_write(pdfio_stream_t *st,	// I - Stream
             const void     *buffer,	// I - Buffer to write
             size_t         bytes)	// I - Number of bytes to write
{
  int	status;				// Compression status


  // Flate-compress the buffer...
  st->flate.avail_in = (uInt)bytes;
  st->flate.next_in  = (Bytef *)buffer;

  while (st->flate.avail_in > 0)
  {
    if (st->flate.avail_out < (sizeof(st->cbuffer) / 8))
    {
      // Flush the compression buffer...
      if (!_pdfioFileWrite(st->pdf, st->cbuffer, sizeof(st->cbuffer) - st->flate.avail_out))
        return (false);

      st->flate.next_out  = (Bytef *)st->cbuffer;
      st->flate.avail_out = sizeof(st->cbuffer);
    }

    // Deflate what we can this time...
    status = deflate(&st->flate, Z_NO_FLUSH);

    if (status < Z_OK && status != Z_BUF_ERROR)
    {
      _pdfioFileError(st->pdf, "Flate compression failed (%d)", status);
      return (false);
    }
  }

  return (true);
}
