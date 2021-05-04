//
// Common support functions for pdfio.
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

static bool	fill_buffer(pdfio_file_t *pdf);
static ssize_t	read_buffer(pdfio_file_t *pdf, char *buffer, size_t bytes);
static bool	write_buffer(pdfio_file_t *pdf, const void *buffer, size_t bytes);


//
// '_pdfioFileClearTokens()' - Clear the token stack.
//

void
_pdfioFileClearTokens(pdfio_file_t *pdf)// I - PDF file
{
  PDFIO_DEBUG("_pdfioFileClearTokens(pdf=%p)\n", pdf);

  while (pdf->num_tokens > 0)
  {
    pdf->num_tokens --;
    free(pdf->tokens[pdf->num_tokens]);
    pdf->tokens[pdf->num_tokens] = NULL;
  }
}


//
// '_pdfioFileConsume()' - Consume bytes from the file.
//

bool					// O - `true` on sucess, `false` on EOF
_pdfioFileConsume(pdfio_file_t *pdf,	// I - PDF file
                  size_t       bytes)	// I - Bytes to consume
{
  PDFIO_DEBUG("_pdfioFileConsume(pdf=%p, bytes=%u)\n", pdf, (unsigned)bytes);

  if ((size_t)(pdf->bufend - pdf->bufptr) > bytes)
    pdf->bufptr += bytes;
  else if (_pdfioFileSeek(pdf, (off_t)bytes, SEEK_CUR) < 0)
    return (false);

  return (true);
}


//
// '_pdfioFileDefaultError()' - Default error callback.
//
// The default error callback writes the error message to stderr and returns
// `false` to halt.
//

bool					// O - `false` to stop
_pdfioFileDefaultError(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *message,		// I - Error message
    void         *data)			// I - Callback data (unused)
{
  (void)data;

  fprintf(stderr, "%s: %s\n", pdf->filename, message);

  return (false);
}


//
// '_pdfioFileError()' - Display an error message.
//

bool					// O - `true` to continue, `false` to stop
_pdfioFileError(pdfio_file_t *pdf,	// I - PDF file
                const char   *format,	// I - `printf`-style format string
                ...)			// I - Additional arguments as needed
{
  char		buffer[8192];		// Message buffer
  va_list	ap;			// Argument pointer


  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  return ((pdf->error_cb)(pdf, buffer, pdf->error_data));
}


//
// '_pdfioFileFlush()' - Flush any pending write data.
//

bool					// O - `true` on success, `false` on failure
_pdfioFileFlush(pdfio_file_t *pdf)	// I - PDF file
{
  PDFIO_DEBUG("_pdfioFileFlush(pdf=%p)\n", pdf);

  if (pdf->bufptr > pdf->buffer)
  {
    if (!write_buffer(pdf, pdf->buffer, (size_t)(pdf->bufptr - pdf->buffer)))
      return (false);

    pdf->bufpos += pdf->bufptr - pdf->buffer;
    pdf->bufptr = pdf->buffer;
  }

  return (true);
}


//
// '_pdfioFileGetChar()' - Get a character from a PDF file.
//

int					// O - Character or `-1` on EOF
_pdfioFileGetChar(pdfio_file_t *pdf)	// I - PDF file
{
  // If there is a character ready in the buffer, return it now...
  if (pdf->bufptr < pdf->bufend)
    return (*(pdf->bufptr ++));

  // Otherwise try to fill the read buffer...
  if (!fill_buffer(pdf))
    return (-1);

  // Then return the next character in the buffer...
  return (*(pdf->bufptr ++));
}


//
// '_pdfioFileGetToken()' - Get a token from a PDF file.
//

bool					// O - `true` on success, `false` on failure
_pdfioFileGetToken(pdfio_file_t *pdf,	// I - PDF file
                   char         *buffer,// I - String buffer
                   size_t       bufsize)// I - Size of string buffer
{
  // See if we have a token waiting on the stack...
  if (pdf->num_tokens > 0)
  {
    // Yes, return it...
    pdf->num_tokens --;
    strncpy(buffer, pdf->tokens[pdf->num_tokens], bufsize - 1);
    buffer[bufsize - 1] = '\0';

    PDFIO_DEBUG("_pdfioFileGetToken(pdf=%p, buffer=%p, bufsize=%u): Popping '%s' from stack.\n", pdf, buffer, (unsigned)bufsize, buffer);

    free(pdf->tokens[pdf->num_tokens]);
    pdf->tokens[pdf->num_tokens] = NULL;
    return (true);
  }

  // No, read a new one...
  return (_pdfioTokenRead(pdf, buffer, bufsize, (_pdfio_tpeek_cb_t)_pdfioFilePeek, (_pdfio_tconsume_cb_t)_pdfioFileConsume, pdf));
}


//
// '_pdfioFileGets()' - Read a line from a PDF file.
//

bool					// O - `true` on success, `false` on error
_pdfioFileGets(pdfio_file_t *pdf,	// I - PDF file
               char         *buffer,	// I - Line buffer
	       size_t       bufsize)	// I - Size of line buffer
{
  bool	eol = false;			// End of line?
  char	*bufptr = buffer,		// Pointer into buffer
	*bufend = buffer + bufsize - 1;	// Pointer to end of buffer


  while (!eol)
  {
    // If there are characters ready in the buffer, use them...
    while (!eol && pdf->bufptr < pdf->bufend && bufptr < bufend)
    {
      char ch = *(pdf->bufptr++);	// Next character in buffer

      if (ch == '\n' || ch == '\r')
      {
        // CR, LF, or CR + LF end a line...
        eol = true;

        if (ch == '\r')
        {
          // Check for a LF after CR
          if (pdf->bufptr >= pdf->bufend)
            fill_buffer(pdf);

	  if (pdf->bufptr < pdf->bufend && *(pdf->bufptr) == '\n')
	    pdf->bufptr ++;
	}
      }
      else
        *bufptr++ = ch;
    }

    // Fill the read buffer as needed...
    if (!eol)
    {
      if (!fill_buffer(pdf))
        break;
    }
  }

  *bufptr = '\0';

  return (eol);
}


//
// '_pdfioFilePeek()' - Peek at upcoming data in a PDF file.
//

ssize_t					// O - Number of bytes returned
_pdfioFilePeek(pdfio_file_t *pdf,	// I - PDF file
               void         *buffer,	// I - Buffer
               size_t       bytes)	// I - Size of bufffer
{
  ssize_t	total;			// Total bytes available


  // See how much data is buffered up...
  if (pdf->bufptr >= pdf->bufend)
  {
    // Fill the buffer...
    if (!fill_buffer(pdf))
      return (-1);
  }

  if ((total = pdf->bufend - pdf->bufptr) < (ssize_t)bytes && total < (ssize_t)(sizeof(pdf->buffer) / 2))
  {
    // Yes, try reading more...
    ssize_t	rbytes;			// Bytes read

    memmove(pdf->buffer, pdf->bufptr, total);
    pdf->bufpos += pdf->bufptr - pdf->buffer;
    pdf->bufptr = pdf->buffer;
    pdf->bufend = pdf->buffer + total;

    // Read until we have bytes or a non-recoverable error...
    while ((rbytes = read(pdf->fd, pdf->bufend, sizeof(pdf->buffer) - (size_t)total)) < 0)
    {
      if (errno != EINTR && errno != EAGAIN)
	break;
    }

    if (rbytes > 0)
    {
      // Expand the buffer...
      pdf->bufend += rbytes;
      total       += rbytes;
    }
  }

  // Copy anything we have to the buffer...
  if (total > (ssize_t)bytes)
    total = (ssize_t)bytes;

  if (total > 0)
    memcpy(buffer, pdf->bufptr, total);

  return (total);
}


//
// '_pdfioFilePrintf()' - Write a formatted string to a PDF file.
//

bool					// O - `true` on success, `false` on failure
_pdfioFilePrintf(pdfio_file_t *pdf,	// I - PDF file
                 const char   *format,	// I - `printf`-style format string
                 ...)			// I - Additional arguments as needed
{
  char		buffer[8102];		// String buffer
  va_list	ap;			// Argument list


  // Format the string...
  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  // Write it...
  return (_pdfioFileWrite(pdf, buffer, strlen(buffer)));
}


//
// '()' - Push a token on the token stack.
//

void
_pdfioFilePushToken(pdfio_file_t *pdf,	// I - PDF file
                    const char   *token)// I - Token
{
  if (pdf->num_tokens < (sizeof(pdf->tokens) / sizeof(pdf->tokens[0])))
  {
    if ((pdf->tokens[pdf->num_tokens ++] = strdup(token)) == NULL)
      pdf->num_tokens --;
  }
}


//
// '_pdfioFilePuts()' - Write a literal string to a PDF file.
//

bool					// O - `true` on success, `false` on failure
_pdfioFilePuts(pdfio_file_t *pdf,	// I - PDF file
               const char   *s)		// I - Literal string
{
  // Write it...
  return (_pdfioFileWrite(pdf, s, strlen(s)));
}


//
// '_pdfioFileRead()' - Read from a PDF file.
//

ssize_t					// O - Number of bytes read or `-1` on error
_pdfioFileRead(pdfio_file_t *pdf,	// I - PDF file
               void         *buffer,	// I - Read buffer
               size_t       bytes)	// I - Number of bytes to read
{
  char		*bufptr = (char *)buffer;
					// Pointer into buffer
  ssize_t	total,			// Total bytes read
		rbytes;			// Bytes read this time


  // Loop until we have read all of the requested bytes or hit an error...
  for (total = 0; bytes > 0; total += rbytes, bytes -= (size_t)rbytes, bufptr += rbytes)
  {
    // First read from the file buffer...
    if ((rbytes = pdf->bufend - pdf->bufptr) > 0)
    {
      if ((size_t)rbytes > bytes)
        rbytes = (ssize_t)bytes;

      memcpy(bufptr, pdf->bufptr, rbytes);
      pdf->bufptr += rbytes;
      continue;
    }

    // Nothing buffered...
    if (bytes > 1024)
    {
      // Read directly from the file...
      if ((rbytes = read_buffer(pdf, bufptr, bytes)) > 0)
      {
	pdf->bufpos += rbytes;
	continue;
      }
      else
        break;
    }
    else
    {
      // Fill buffer and try again...
      if (!fill_buffer(pdf))
	break;
    }
  }

  return (total);
}


//
// '_pdfioFileSeek()' - Seek within a PDF file.
//

off_t					// O - New offset from beginning of file or `-1` on error
_pdfioFileSeek(pdfio_file_t *pdf,	// I - PDF file
               off_t        offset,	// I - Offset
               int          whence)	// I - Offset base
{
  // Adjust offset for relative seeks...
  if (whence == SEEK_CUR)
  {
    offset += pdf->bufpos;
    whence = SEEK_SET;
  }

  if (pdf->mode == _PDFIO_MODE_READ)
  {
    // Reading, see if we already have the data we need...
    if (whence != SEEK_END && offset >= pdf->bufpos && offset < (pdf->bufpos + pdf->bufend - pdf->buffer))
    {
      // Yes, seek within existing buffer...
      pdf->bufptr = pdf->buffer + offset - pdf->bufpos;
      return (offset);
    }

    // No, reset the read buffer
    pdf->bufptr = pdf->bufend = NULL;
  }
  else
  {
    // Writing, make sure we write any buffered data...
    if (pdf->bufptr > pdf->buffer)
    {
      if (!write_buffer(pdf, pdf->buffer, (size_t)(pdf->bufptr - pdf->buffer)))
	return (-1);
    }

    pdf->bufptr = pdf->buffer;
  }

  // Seek within the file...
  if ((offset = lseek(pdf->fd, offset, whence)) < 0)
  {
    _pdfioFileError(pdf, "Unable to seek within file - %s", strerror(errno));
    return (-1);
  }

  pdf->bufpos = offset;

  return (offset);
}


//
// '_pdfioFileTell()' - Return the offset within a PDF file.
//

off_t					// O - Offset from beginning of file
_pdfioFileTell(pdfio_file_t *pdf)	// I - PDF file
{
  if (pdf->bufptr)
    return (pdf->bufpos + (pdf->bufptr - pdf->buffer));
  else
    return (pdf->bufpos);
}


//
// '_pdfioFileWrite()' - Write to a PDF file.
//

bool					// O - `true` on success and `false` on error
_pdfioFileWrite(pdfio_file_t *pdf,	// I - PDF file
                const void   *buffer,	// I - Write buffer
                size_t       bytes)	// I - Bytes to write
{
  // See if the data will fit in the write buffer...
  if (bytes > (size_t)(pdf->bufend - pdf->bufptr))
  {
    // No room, flush any current data...
    if (!_pdfioFileFlush(pdf))
      return (false);

    if (bytes >= sizeof(pdf->buffer))
    {
      // Write directly...
      if (!write_buffer(pdf, buffer, bytes))
        return (false);

      pdf->bufpos += bytes;

      return (true);
    }
  }

  // Copy data to the buffer and return...
  memcpy(pdf->bufptr, buffer, bytes);
  pdf->bufptr += bytes;

  return (true);
}


//
// 'fill_buffer()' - Fill the read buffer in a PDF file.
//

static bool				// O - `true` on success, `false` on failure
fill_buffer(pdfio_file_t *pdf)		// I - PDF file
{
  ssize_t	bytes;			// Bytes read...


  // Advance current position in file as needed...
  if (pdf->bufend)
    pdf->bufpos += pdf->bufend - pdf->buffer;

  // Try reading from the file...
  if ((bytes = read_buffer(pdf, pdf->buffer, sizeof(pdf->buffer))) <= 0)
  {
    // EOF or hard error...
    pdf->bufptr = pdf->bufend = NULL;
    return (false);
  }
  else
  {
    // Successful read...
    pdf->bufptr = pdf->buffer;
    pdf->bufend = pdf->buffer + bytes;
    return (true);
  }
}


//
// 'read_buffer()' - Read a buffer from a PDF file.
//

static ssize_t				// O - Number of bytes read or -1 on error
read_buffer(pdfio_file_t *pdf,		// I - PDF file
            char         *buffer,	// I - Buffer
            size_t       bytes)		// I - Number of bytes to read
{
  ssize_t	rbytes;			// Bytes read...


  // Read from the file...
  while ((rbytes = read(pdf->fd, buffer, bytes)) < 0)
  {
    // Stop if we have an error that shouldn't be retried...
    if (errno != EINTR && errno != EAGAIN)
      break;
  }

  if (rbytes < 0)
  {
    // Hard error...
    _pdfioFileError(pdf, "Unable to read from file - %s", strerror(errno));
  }

  return (rbytes);
}

  
//
// 'write_buffer()' - Write a buffer to a PDF file.
//

static bool				// O - `true` on success and `false` on error
write_buffer(pdfio_file_t *pdf,		// I - PDF file
	     const void   *buffer,	// I - Write buffer
	     size_t       bytes)	// I - Bytes to write
{
  const char	*bufptr = (const char *)buffer;
					// Pointer into buffer
  ssize_t	wbytes;			// Bytes written...


  // Write to the file...
  while (bytes > 0)
  {
    while ((wbytes = write(pdf->fd, bufptr, bytes)) < 0)
    {
      // Stop if we have an error that shouldn't be retried...
      if (errno != EINTR && errno != EAGAIN)
	break;
    }

    if (wbytes < 0)
    {
      // Hard error...
      _pdfioFileError(pdf, "Unable to write to file - %s", strerror(errno));
      return (false);
    }

    bufptr += wbytes;
    bytes  -= (size_t)wbytes;
  }

  return (true);
}
