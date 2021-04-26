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
