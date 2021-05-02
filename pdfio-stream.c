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
