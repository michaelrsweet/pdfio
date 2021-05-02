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
// {...}                     "{" and "}" are reserved as future delimiters
// /name                     "/" starts a name with any special characters
//                           quoted as "#HH" where HH is the byte value in hex.
// %comment                  "%" starts a comment to the end of a line
// keyword                   A keyword consists of other unreserved characters
// [-+]?[0-9]*(.[0-9]*)?     A number optionally starts with "+" or "-".
//
// Newlines are CR, LF, or CR LF.
//
// Strings and names are returned with the leading delimiter ("(string",
// "<hex-string", "/name") and all escaping/whitespace removal resolved.
// Other delimiters, keywords, and numbers are returned as-is.
//


//
// Constants...
//

#define PDFIO_NUMBER_CHARS	"0123456789-+."
#define PDFIO_DELIM_CHARS	"<>(){}[]/%"


//
// Types...
//

typedef struct _pdfio_tbuffer_s		// Token reading buffer
{
  unsigned char		buffer[32],	// Buffer
			*bufptr,	// Pointer into buffer
			*bufend;	// Last valid byte in buffer
  _pdfio_tpeek_cb_t	peek_cb;	// Peek callback
  _pdfio_tconsume_cb_t	consume_cb;	// Consume callback
  void			*data;		// Callback data
} _pdfio_tbuffer_t;


//
// Local functions...
//

static int	get_char(_pdfio_tbuffer_t *tb);


//
// '_pdfioTokenRead()' - Read a token from a file/stream.
//

bool					// O - `true` on success, `false` on failure
_pdfioTokenRead(
    pdfio_file_t         *pdf,		// I - PDF file
    char                 *buffer,	// I - String buffer
    size_t               bufsize,	// I - Size of string buffer
    _pdfio_tpeek_cb_t    peek_cb,	// I - "peek" callback
    _pdfio_tconsume_cb_t consume_cb,	// I - "consume" callback
    void                 *data)		// I - Callback data
{
  _pdfio_tbuffer_t	tb;		// Token buffer
  int			ch;		// Character
  char			*bufptr,	// Pointer into buffer
			*bufend,	// End of buffer
			state = '\0';	// Current state


  //
  // "state" is:
  //
  // - '\0' for idle
  // - ')' for literal string
  // - '/' for name
  // - '<' for possible hex string or dict
  // - '>' for possible dict
  // - '%' for comment
  // - 'K' for keyword
  // - 'N' for number
  // - 'X' for hex string

  // Read the next token, skipping any leading whitespace...
  memset(&tb, 0, sizeof(tb));
  tb.peek_cb    = peek_cb;
  tb.consume_cb = consume_cb;
  tb.data       = data;

  bufptr = buffer;
  bufend = buffer + bufsize - 1;

  // Skip leading whitespace...
  while ((ch = get_char(&tb)) != EOF)
  {
    if (ch == '%')
    {
      // Skip comment
      while ((ch = get_char(&tb)) != EOF)
      {
	if (ch == '\n' || ch == '\r')
	  break;
      }
    }
    else if (!isspace(ch))
      break;
  }

  if (ch == EOF)
    return (false);

  // Check for delimiters...
  if (strchr(PDFIO_DELIM_CHARS, ch) != NULL)
  {
    *bufptr++ = state = (char)ch;
  }
  else if (strchr(PDFIO_NUMBER_CHARS, ch) != NULL)
  {
    // Number
    state     = 'N';
    *bufptr++ = (char)ch;
  }
  else
  {
    // Keyword
    state     = 'K';
    *bufptr++ = (char)ch;
  }

  switch (state)
  {
    case ')' : // Literal string
	while ((ch = get_char(&tb)) != EOF && ch != ')')
	{
	  if (ch == '\\')
	  {
	    // Quoted character...
	    int	i;			// Looping var

	    switch (ch = get_char(&tb))
	    {
	      case '0' : // Octal character escape
	      case '1' :
	      case '2' :
	      case '3' :
	      case '4' :
	      case '5' :
	      case '6' :
	      case '7' :
		  for (ch -= '0', i = 0; i < 2; i ++)
		  {
		    int tch = get_char(&tb);	// Next char

		    if (tch >= '0' && tch <= '7')
		      ch = (char)((ch << 3) | (tch - '0'));
		    else
		    {
		      tb.bufptr --;
		      break;
		    }
		  }
		  break;

	      case '\\' :
	      case '(' :
	      case ')' :
		  break;

	      case 'n' :
		  ch = '\n';
		  break;

	      case 'r' :
		  ch = '\r';
		  break;

	      case 't' :
		  ch = '\t';
		  break;

	      case 'b' :
		  ch = '\b';
		  break;

	      case 'f' :
		  ch = '\f';
		  break;

	      default :
		  _pdfioFileError(pdf, "Unknown escape '\\%c' in literal string.", ch);
		  return (false);
	    }
	  }

	  if (bufptr < bufend)
	  {
	    // Normal character...
	    *bufptr++ = (char)ch;
	  }
	  else
	  {
	    // Out of space
	    _pdfioFileError(pdf, "Token too large.");
	    return (false);
	  }
	}

	if (ch != ')')
	{
	  _pdfioFileError(pdf, "Unterminated string literal.");
	  return (false);
	}
	break;

    case 'K' : // keyword
	while ((ch = get_char(&tb)) != EOF && !isspace(ch))
	{
	  if (strchr(PDFIO_DELIM_CHARS, ch) != NULL)
	  {
	    // End of keyword...
	    tb.bufptr --;
	    break;
	  }
	  else if (bufptr < bufend)
	  {
	    // Normal character...
	    *bufptr++ = (char)ch;
	  }
	  else
	  {
	    // Out of space...
	    _pdfioFileError(pdf, "Token too large.");
	    return (false);
	  }
	}
	break;

    case 'N' : // number
	while ((ch = get_char(&tb)) != EOF && !isspace(ch))
	{
	  if (!isdigit(ch) && ch != '.')
	  {
	    // End of number...
	    break;
	  }
	  else if (bufptr < bufend)
	  {
	    // Normal character...
	    *bufptr++ = (char)ch;
	  }
	  else
	  {
	    // Out of space...
	    _pdfioFileError(pdf, "Token too large.");
	    return (false);
	  }
	}
	break;

    case '/' : // "/name"
	while ((ch = get_char(&tb)) != EOF && !isspace(ch))
	{
	  if (ch == '#')
	  {
	    // Quoted character (#xx) in name...
	    int	i;			// Looping var

	    for (i = 0, ch = 0; i < 2; i ++)
	    {
	      int tch = get_char(&tb);

	      if (!isxdigit(tch & 255))
	      {
		_pdfioFileError(pdf, "Bad # escape in name.");
		return (false);
	      }
	      else if (isdigit(tch))
		ch = (char)((ch << 4) | (tch - '0'));
	      else
		ch = (char)((ch << 4) | (tolower(tch) - 'a' + 10));
	    }
	  }

	  if (bufptr < bufend)
	  {
	    *bufptr++ = (char)ch;
	  }
	  else
	  {
	    // Out of space
	    _pdfioFileError(pdf, "Token too large.");
	    return (false);
	  }
	}
	break;

    case '<' : // Potential hex string
	if ((ch = get_char(&tb)) == '<')
	{
	  // Dictionary delimiter
	  *bufptr++ = (char)ch;
	  break;
	}
	else if (!isspace(ch & 255) && !isxdigit(ch & 255))
	{
	  _pdfioFileError(pdf, "Syntax error: '<%c'", ch);
	  return (false);
	}

	// Fall through to parse a hex string...

    case 'X' : // Hex string
	while ((ch = get_char(&tb)) != EOF && ch != '>')
	{
	  if (isxdigit(ch))
	  {
	    if (bufptr < bufend)
	    {
	      // Hex digit
	      *bufptr++ = (char)ch;
	    }
	    else
	    {
	      // Too large
	      _pdfioFileError(pdf, "Token too large.");
	      return (false);
	    }
	  }
	  else if (!isspace(ch))
	  {
	    _pdfioFileError(pdf, "Invalid hex string character '%c'.", ch);
	    return (false);
	  }
	}

	if (ch == EOF)
	{
	  _pdfioFileError(pdf, "Unterminated hex string.");
	  return (false);
	}
	break;

    case '>' : // Dictionary
	if ((ch = get_char(&tb)) == '>')
	{
	  *bufptr++ = '>';
	}
	else
	{
	  _pdfioFileError(pdf, "Syntax error: '>%c'.", ch);
	  return (false);
	}
	break;
  }

  while (tb.bufptr < tb.bufend && isspace(*(tb.bufptr)))
    tb.bufptr ++;

  if (tb.bufptr > tb.buffer)
    (consume_cb)(data, (size_t)(tb.bufptr - tb.buffer));

  *bufptr = '\0';

  return (bufptr > buffer);
}


//
// 'get_char()' - Get a character from the token buffer.
//

static int				// O - Character or `EOF` on end-of-file
get_char(_pdfio_tbuffer_t *tb)		// I - Token buffer
{
  ssize_t	bytes;			// Bytes peeked


  // Refill the buffer as needed...
  if (tb->bufptr >= tb->bufend)
  {
    // Consume previous bytes...
    if (tb->bufend > tb->buffer)
      (tb->consume_cb)(tb->data, (size_t)(tb->bufend - tb->buffer));

    // Peek new bytes...
    if ((bytes = (tb->peek_cb)(tb->data, tb->buffer, sizeof(tb->buffer))) < 0)
    {
      tb->bufptr = tb->bufend = tb->buffer;
      return (EOF);
    }

    // Update pointers...
    tb->bufptr = tb->buffer;
    tb->bufend = tb->buffer + bytes;
  }

  // Return the next character...
  return (*(tb->bufptr)++);
}
