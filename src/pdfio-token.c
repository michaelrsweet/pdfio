//
// PDF token parsing functions for PDFio.
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
// Local functions...
//

static int	get_char(_pdfio_token_t *tb);


//
// '_pdfioTokenClear()' - Clear the token stack.
//

void
_pdfioTokenClear(_pdfio_token_t *tb)	// I - Token buffer/stack
{
  PDFIO_DEBUG("_pdfioTokenClear(tb=%p)\n", tb);

  while (tb->num_tokens > 0)
  {
    tb->num_tokens --;
    free(tb->tokens[tb->num_tokens]);
    tb->tokens[tb->num_tokens] = NULL;
  }
}


//
// '_pdfioTokenFlush()' - Flush (consume) any bytes that have been used.
//

void
_pdfioTokenFlush(_pdfio_token_t *tb)	// I - Token buffer/stack
{
  if (tb->bufptr > tb->buffer)
  {
    size_t remaining = (size_t)(tb->bufend - tb->bufptr);
					// Remaining bytes in buffer

    // Consume what we've used...
    PDFIO_DEBUG("_pdfioTokenFlush: Consuming %d bytes.\n", (int)(tb->bufptr - tb->buffer));
    (tb->consume_cb)(tb->cb_data, (size_t)(tb->bufptr - tb->buffer));

    if (remaining > 0)
    {
      // Shuffle remaining bytes for next call...
      memmove(tb->buffer, tb->bufptr, remaining);
      tb->bufptr = tb->buffer;
      tb->bufend = tb->buffer + remaining;

#ifdef DEBUG
      unsigned char *ptr;		// Pointer into buffer

      PDFIO_DEBUG("_pdfioTokenFlush: Remainder '");
      for (ptr = tb->buffer; ptr < tb->bufend; ptr ++)
      {
	if (*ptr < ' ' || *ptr == 0x7f)
	  PDFIO_DEBUG("\\%03o", *ptr);
	else
	  PDFIO_DEBUG("%c", *ptr);
      }
      PDFIO_DEBUG("'\n");
#endif // DEBUG
    }
    else
    {
      // Nothing left, reset pointers...
      PDFIO_DEBUG("_pdfioTokenFlush: Resetting pointers.\n");
      tb->bufptr = tb->bufend = tb->buffer;
    }
  }
}


//
// '_pdfioTokenGet()' - Get a token.
//

bool					// O - `true` on success, `false` on failure
_pdfioTokenGet(_pdfio_token_t *tb,	// I - Token buffer/stack
	       char           *buffer,	// I - String buffer
	       size_t         bufsize)	// I - Size of string buffer
{
  // See if we have a token waiting on the stack...
  if (tb->num_tokens > 0)
  {
    // Yes, return it...
    size_t len;				// Length of token

    tb->num_tokens --;

    if ((len = strlen(tb->tokens[tb->num_tokens])) > (bufsize - 1))
    {
      // Value too large...
      PDFIO_DEBUG("_pdfioTokenGet(tb=%p, buffer=%p, bufsize=%u): Token '%s' from stack too large.\n", tb, buffer, (unsigned)bufsize, tb->tokens[tb->num_tokens]);
      *buffer = '\0';
      return (false);
    }

    memcpy(buffer, tb->tokens[tb->num_tokens], len);
    buffer[len] = '\0';

    PDFIO_DEBUG("_pdfioTokenGet(tb=%p, buffer=%p, bufsize=%u): Popping '%s' from stack.\n", tb, buffer, (unsigned)bufsize, buffer);

    free(tb->tokens[tb->num_tokens]);
    tb->tokens[tb->num_tokens] = NULL;

    return (true);
  }

  // No, read a new one...
  return (_pdfioTokenRead(tb, buffer, bufsize));
}


//
// '_pdfioTokenInit()' - Initialize a token buffer/stack.
//

void
_pdfioTokenInit(
    _pdfio_token_t       *ts,		// I - Token buffer/stack
    pdfio_file_t         *pdf,		// I - PDF file
    _pdfio_tconsume_cb_t consume_cb,	// I - Consume callback
    _pdfio_tpeek_cb_t    peek_cb,	// I - Peek callback
    void                 *cb_data)	// I - Callback data
{
  // Zero everything out and then initialize key pointers...
  memset(ts, 0, sizeof(_pdfio_token_t));

  ts->pdf        = pdf;
  ts->consume_cb = consume_cb;
  ts->peek_cb    = peek_cb;
  ts->cb_data    = cb_data;
  ts->bufptr     = ts->buffer;
  ts->bufend     = ts->buffer;
}


//
// '_pdfioTokenPush()' - Push a token on the token stack.
//

void
_pdfioTokenPush(_pdfio_token_t *tb,	// I - Token buffer/stack
		const char     *token)	// I - Token to push
{
  if (tb->num_tokens < (sizeof(tb->tokens) / sizeof(tb->tokens[0])))
  {
    if ((tb->tokens[tb->num_tokens ++] = strdup(token)) == NULL)
      tb->num_tokens --;
  }
}


//
// '_pdfioTokenRead()' - Read a token from a file/stream.
//

bool					// O - `true` on success, `false` on failure
_pdfioTokenRead(_pdfio_token_t *tb,	// I - Token buffer/stack
		char           *buffer,	// I - String buffer
		size_t         bufsize)	// I - Size of string buffer
{
  int	ch,				// Character
	parens = 0;			// Parenthesis level
  char	*bufptr,			// Pointer into buffer
	*bufend,			// End of buffer
	state = '\0';			// Current state
  bool	saw_nul = false;		// Did we see a nul character?


  //
  // "state" is:
  //
  // - '\0' for idle
  // - '(' for literal string
  // - '/' for name
  // - '<' for possible hex string or dict
  // - '>' for possible dict
  // - '%' for comment
  // - 'K' for keyword
  // - 'N' for number

  // Read the next token, skipping any leading whitespace...
  bufptr = buffer;
  bufend = buffer + bufsize - 1;

  // Skip leading whitespace...
  while ((ch = get_char(tb)) != EOF)
  {
    if (ch == '%')
    {
      // Skip comment
      while ((ch = get_char(tb)) != EOF)
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
    case '(' : // Literal string
	while ((ch = get_char(tb)) != EOF)
	{
	  if (ch == 0)
	    saw_nul = true;

	  if (ch == '\\')
	  {
	    // Quoted character...
	    int	i;			// Looping var

	    switch (ch = get_char(tb))
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
		    int tch = get_char(tb);	// Next char

		    if (tch >= '0' && tch <= '7')
		    {
		      ch = (char)((ch << 3) | (tch - '0'));
		    }
		    else
		    {
		      tb->bufptr --;
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
	          // Ignore blackslash per PDF spec...
	          break;
	    }
	  }
	  else if (ch == '(')
	  {
	    // Keep track of parenthesis
	    parens ++;
	  }
	  else if (ch == ')')
	  {
	    if (parens == 0)
	      break;

	    parens --;
	  }

	  if (bufptr < bufend)
	  {
	    // Normal character...
	    *bufptr++ = (char)ch;
	  }
	  else
	  {
	    // Out of space
	    _pdfioFileError(tb->pdf, "Token too large.");
	    return (false);
	  }
	}

	if (ch != ')')
	{
	  _pdfioFileError(tb->pdf, "Unterminated string literal.");
	  return (false);
	}

	if (saw_nul)
	{
	  // Convert to a hex (binary) string...
	  char	*litptr,		// Pointer to literal character
		*hexptr;		// Pointer to hex character
	  size_t bytes = (size_t)(bufptr - buffer - 1);
					// Bytes of data...
          static const char *hexchars = "0123456789ABCDEF";
					// Hex digits

          PDFIO_DEBUG("_pdfioTokenRead: Converting nul-containing string to binary.\n");

          if ((2 * (bytes + 1)) > bufsize)
          {
	    // Out of space...
	    _pdfioFileError(tb->pdf, "Token too large.");
	    return (false);
          }

	  *buffer = '<';
	  for (litptr = bufptr - 1, hexptr = buffer + 2 * bytes - 1; litptr > buffer; litptr --, hexptr -= 2)
	  {
	    int litch = *litptr;	// Grab the character

	    hexptr[0] = hexchars[(litch >> 4) & 15];
	    hexptr[1] = hexchars[litch & 15];
	  }
	  bufptr = buffer + 2 * bytes + 1;
	}
	break;

    case 'K' : // keyword
	while ((ch = get_char(tb)) != EOF && !isspace(ch))
	{
	  if (strchr(PDFIO_DELIM_CHARS, ch) != NULL)
	  {
	    // End of keyword...
	    tb->bufptr --;
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
	    _pdfioFileError(tb->pdf, "Token too large.");
	    return (false);
	  }
	}
	break;

    case 'N' : // number
	while ((ch = get_char(tb)) != EOF && !isspace(ch))
	{
	  if (!isdigit(ch) && ch != '.')
	  {
	    // End of number...
	    tb->bufptr --;
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
	    _pdfioFileError(tb->pdf, "Token too large.");
	    return (false);
	  }
	}
	break;

    case '/' : // "/name"
	while ((ch = get_char(tb)) != EOF && !isspace(ch))
	{
	  if (strchr(PDFIO_DELIM_CHARS, ch) != NULL)
	  {
	    // End of keyword...
	    tb->bufptr --;
	    break;
	  }
	  else if (ch == '#')
	  {
	    // Quoted character (#xx) in name...
	    int	i;			// Looping var

	    for (i = 0, ch = 0; i < 2; i ++)
	    {
	      int tch = get_char(tb);

	      if (!isxdigit(tch & 255))
	      {
		_pdfioFileError(tb->pdf, "Bad # escape in name.");
		return (false);
	      }
	      else if (isdigit(tch))
		ch = ((ch & 255) << 4) | (tch - '0');
	      else
		ch = ((ch & 255) << 4) | (tolower(tch) - 'a' + 10);
	    }
	  }

	  if (bufptr < bufend)
	  {
	    *bufptr++ = (char)ch;
	  }
	  else
	  {
	    // Out of space
	    _pdfioFileError(tb->pdf, "Token too large.");
	    return (false);
	  }
	}
	break;

    case '<' : // Potential hex string
	if ((ch = get_char(tb)) == '<')
	{
	  // Dictionary delimiter
	  *bufptr++ = (char)ch;
	  break;
	}
	else if (!isspace(ch & 255) && !isxdigit(ch & 255))
	{
	  _pdfioFileError(tb->pdf, "Syntax error: '<%c'", ch);
	  return (false);
	}

        do
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
	      _pdfioFileError(tb->pdf, "Token too large.");
	      return (false);
	    }
	  }
	  else if (!isspace(ch))
	  {
	    _pdfioFileError(tb->pdf, "Invalid hex string character '%c'.", ch);
	    return (false);
	  }
	}
	while ((ch = get_char(tb)) != EOF && ch != '>');

	if (ch == EOF)
	{
	  _pdfioFileError(tb->pdf, "Unterminated hex string.");
	  return (false);
	}
	break;

    case '>' : // Dictionary
	if ((ch = get_char(tb)) == '>')
	{
	  *bufptr++ = '>';
	}
	else
	{
	  _pdfioFileError(tb->pdf, "Syntax error: '>%c'.", ch);
	  return (false);
	}
	break;
  }

  *bufptr = '\0';

//  PDFIO_DEBUG("_pdfioTokenRead: Read '%s'.\n", buffer);

  return (bufptr > buffer);
}


//
// 'get_char()' - Get a character from the token buffer.
//

static int				// O - Character or `EOF` on end-of-file
get_char(_pdfio_token_t *tb)		// I - Token buffer
{
  ssize_t	bytes;			// Bytes peeked


  // Refill the buffer as needed...
  if (tb->bufptr >= tb->bufend)
  {
    // Consume previous bytes...
    if (tb->bufend > tb->buffer)
    {
      PDFIO_DEBUG("get_char: Consuming %d bytes.\n", (int)(tb->bufend - tb->buffer));
      (tb->consume_cb)(tb->cb_data, (size_t)(tb->bufend - tb->buffer));
    }

    // Peek new bytes...
    if ((bytes = (tb->peek_cb)(tb->cb_data, tb->buffer, sizeof(tb->buffer))) <= 0)
    {
      tb->bufptr = tb->bufend = tb->buffer;
      return (EOF);
    }

    // Update pointers...
    tb->bufptr = tb->buffer;
    tb->bufend = tb->buffer + bytes;

#if 0
#ifdef DEBUG
    unsigned char *ptr;			// Pointer into buffer

    PDFIO_DEBUG("get_char: Read '");
    for (ptr = tb->buffer; ptr < tb->bufend; ptr ++)
    {
      if (*ptr < ' ' || *ptr == 0x7f)
        PDFIO_DEBUG("\\%03o", *ptr);
      else
        PDFIO_DEBUG("%c", *ptr);
    }
    PDFIO_DEBUG("'\n");
#endif // DEBUG
#endif // 0
  }

  // Return the next character...
  return (*(tb->bufptr)++);
}
