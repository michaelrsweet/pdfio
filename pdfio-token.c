//
// PDF token parsing functions for pdfio.
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
// This file parses PDF language syntax:
//
// << dict >>                "<<" and ">>" delimit a dictionary
// (string)                  "(" and ")" delimit a string
// [array]                   "[" and "]" delimit an array
// <hex-string>              "<" and ">" delimit a hex string
// {...}                     "{" and "}" delimit ???
// /name                     "/" starts a name with any special characters
//                           quoted as "#HH" where HH is the byte value in hex.
// %comment                  "%" starts a comment to the end of a line
// keyword                   A keyword consists of upper/lowercase letters
// [-+]?[0-9]*(.[0-9]*)?     A number optionally starts with "+" or "-".
//
// Newlines are CR, LF, or CR LF.
//
// Strings and names are returned with the leading delimiter ("(string",
// "<hex-string", "/name") and all escaping/whitespace removal resolved.
// Other delimiters, keywords, and numbers are returned as-is.
//


//
// '_pdfioTokenRead()' - Read a token from a file/stream.
//

bool					// O - `true` on success, `false` on failure
_pdfioTokenRead(
    char              *buffer,		// I - String buffer
    size_t            bufsize,		// I - Size of string buffer
    _pdfio_token_cb_t peek_cb,		// I - "peek" callback
    _pdfio_token_cb_t read_cb,		// I - "read" callback
    void              *data)		// I - Callback data
{
  char		*bufptr,		// Pointer into buffer
		*bufend,		// End of buffer
		temp[256],		// Temporary buffer
		*tempptr,		// Pointer into temporary buffer
		*tempend;		// End of temporary buffer
  ssize_t	bytes;			// Bytes read/peeked
  size_t	len;			// Length of value


  return (false);
}

