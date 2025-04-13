//
// PDF string functions for PDFio.
//
// Copyright © 2021-2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pdfio-private.h"


//
// Local functions...
//

static size_t	find_string(pdfio_file_t *pdf, const char *s, int *rdiff);


//
// '_pdfio_strlcpy()' - Safe string copy.
//

size_t					// O - Length of source string
_pdfio_strlcpy(char       *dst,		// I - Destination string buffer
               const char *src,		// I - Source string
               size_t     dstsize)	// I - Size of destination
{
  size_t	srclen;			// Length of source string


  // Range check input...
  if (!dst || !src || dstsize == 0)
  {
    if (dst)
      *dst = '\0';
    return (0);
  }

  // Figure out how much room is needed...
  dstsize --;

  srclen = strlen(src);

  // Copy the appropriate amount...
  if (srclen <= dstsize)
  {
    // Source string will fit...
    memmove(dst, src, srclen);
    dst[srclen] = '\0';
  }
  else
  {
    // Source string too big, copy what we can and clean up the end...
    char *ptr = dst + dstsize - 1,	// Pointer into string
	*end = ptr + 1;			// Pointer to end of string

    memmove(dst, src, dstsize);
    dst[dstsize] = '\0';

    // Validate last character in destination buffer...
    if (ptr > dst && *ptr & 0x80)
    {
      while ((*ptr & 0xc0) == 0x80 && ptr > dst)
	ptr --;

      if ((*ptr & 0xe0) == 0xc0)
      {
	// Verify 2-byte UTF-8 sequence...
	if ((end - ptr) != 2)
	  *ptr = '\0';
      }
      else if ((*ptr & 0xf0) == 0xe0)
      {
	// Verify 3-byte UTF-8 sequence...
	if ((end - ptr) != 3)
	  *ptr = '\0';
      }
      else if ((*ptr & 0xf8) == 0xf0)
      {
	// Verify 4-byte UTF-8 sequence...
	if ((end - ptr) != 4)
	  *ptr = '\0';
      }
      else if (*ptr & 0x80)
      {
	// Invalid sequence at end...
	*ptr = '\0';
      }
    }
  }

  return (srclen);
}


//
// '_pdfio_strtod()' - Convert a string to a double value.
//
// This function wraps strtod() to avoid locale issues.
//

double					// O - Double value
_pdfio_strtod(pdfio_file_t *pdf,	// I - PDF file
              const char   *s)		// I - String
{
  char	temp[64],			// Temporary buffer
	*tempptr;			// Pointer into temporary buffer


  // See if the locale has a special decimal point string...
  if (!pdf->loc)
    return (strtod(s, NULL));

  // Copy leading sign, numbers, period, and then numbers...
  tempptr                = temp;
  temp[sizeof(temp) - 1] = '\0';

  while (*s && *s != '.')
  {
    if (tempptr < (temp + sizeof(temp) - 1))
      *tempptr++ = *s++;
    else
      return (0.0);
  }

  if (*s == '.')
  {
    // Convert decimal point to locale equivalent...
    size_t	declen = strlen(pdf->loc->decimal_point);
					// Length of decimal point
    s ++;

    if (declen <= (sizeof(temp) - (size_t)(tempptr - temp)))
    {
      memcpy(tempptr, pdf->loc->decimal_point, declen);
      tempptr += declen;
    }
    else
    {
      return (0.0);
    }
  }

  // Copy any remaining characters...
  while (*s)
  {
    if (tempptr < (temp + sizeof(temp) - 1))
      *tempptr++ = *s++;
    else
      return (0.0);
  }

  // Nul-terminate the temporary string and convert the string...
  *tempptr = '\0';

  return (strtod(temp, NULL));
}


//
// '_pdfio_utf16cpy()' - Convert UTF-16 to UTF-8.
//

void
_pdfio_utf16cpy(
    char                *dst,		// I - Destination buffer for UTF-8
    const unsigned char *src,		// I - Source UTF-16
    size_t              srclen,		// I - Length of UTF-16
    size_t              dstsize)	// I - Destination buffer size
{
  char	*dstptr = dst,			// Pointer into buffer
  	*dstend = dst + dstsize - 5;	// End of buffer
  int	ch;				// Unicode character
  bool	is_be = !memcmp(src, "\376\377", 2);
					// Big-endian strings?


  // Loop through the UTF-16 string, converting to Unicode then UTF-8...
  for (src += 2, srclen -= 2; srclen > 1 && dstptr < dstend; src += 2, srclen -= 2)
  {
    // Initial character...
    if (is_be)
      ch = (src[0] << 8) | src[1];
    else
      ch = (src[1] << 8) | src[0];

    if (ch >= 0xd800 && ch <= 0xdbff && srclen > 3)
    {
      // Multi-word UTF-16 char...
      int lch;			// Lower bits

      if (is_be)
	lch = (src[2] << 8) | src[3];
      else
	lch = (src[3] << 8) | src[2];

      if (lch < 0xdc00 || lch >= 0xdfff)
	break;

      ch     = (((ch & 0x3ff) << 10) | (lch & 0x3ff)) + 0x10000;
      src    += 2;
      srclen -= 2;
    }
    else if (ch >= 0xfffe)
    {
      continue;
    }

    // Convert Unicode to UTF-8...
    if (ch < 128)
    {
      // ASCII
      *dstptr++ = (char)ch;
    }
    else if (ch < 4096)
    {
      // 2-byte UTF-8
      *dstptr++ = (char)(0xc0 | (ch >> 6));
      *dstptr++ = (char)(0x80 | (ch & 0x3f));
    }
    else if (ch < 65536)
    {
      // 3-byte UTF-8
      *dstptr++ = (char)(0xe0 | (ch >> 12));
      *dstptr++ = (char)(0x80 | ((ch >> 6) & 0x3f));
      *dstptr++ = (char)(0x80 | (ch & 0x3f));
    }
    else
    {
      // 4-byte UTF-8
      *dstptr++ = (char)(0xe0 | (ch >> 18));
      *dstptr++ = (char)(0x80 | ((ch >> 12) & 0x3f));
      *dstptr++ = (char)(0x80 | ((ch >> 6) & 0x3f));
      *dstptr++ = (char)(0x80 | (ch & 0x3f));
    }
  }

  // Nul-terminate the UTF-8 string...
  *dstptr = '\0';
}


//
// '_pdfio_vsnprintf()' - Format a string.
//
// This function emulates vsnprintf() to avoid locale issues.
//

ssize_t					// O - Number of bytes
_pdfio_vsnprintf(pdfio_file_t *pdf,	// I - PDF file
                 char         *buffer,	// I - Output buffer
                 size_t       bufsize,	// I - Size of output buffer
                 const char   *format,	// I - printf-style format string
                 va_list      ap)	// I - Pointer to additional arguments
{
  char		*bufptr,		// Pointer to position in buffer
		*bufend,		// Pointer to end of buffer
		size,			// Size character (h, l, L)
		type;			// Format type character
  int		width,			// Width of field
		prec;			// Number of characters of precision
  char		tformat[100],		// Temporary format string for snprintf()
		*tptr,			// Pointer into temporary format
		temp[1024],		// Buffer for formatted numbers
		*tempptr;		// Pointer into buffer
  char		*s;			// Pointer to string
  ssize_t	bytes;			// Total number of bytes needed
  const char	*dec = pdf->loc ? pdf->loc->decimal_point : ".";
					// Decimal point string
  char		*decptr;		// Pointer to decimal point


  // Loop through the format string, formatting as needed...
  bufptr = buffer;
  bufend = buffer + bufsize - 1;
  bytes  = 0;

  while (*format)
  {
    if (*format == '%')
    {
      // Format character...
      tptr = tformat;
      *tptr++ = *format++;

      if (*format == '%')
      {
        if (bufptr < bufend)
	  *bufptr++ = *format;
        bytes ++;
        format ++;
	continue;
      }
      else if (strchr(" -+#\'", *format))
      {
        *tptr++ = *format++;
      }

      if (*format == '*')
      {
        // Get width from argument...
	format ++;
	width = va_arg(ap, int);

	snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", width);
	tptr += strlen(tptr);
      }
      else
      {
	width = 0;

	while (isdigit(*format & 255))
	{
	  if (tptr < (tformat + sizeof(tformat) - 1))
	    *tptr++ = *format;

	  width = width * 10 + *format++ - '0';
	}
      }

      if (*format == '.')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *format;

        format ++;

        if (*format == '*')
	{
          // Get precision from argument...
	  format ++;
	  prec = va_arg(ap, int);

	  snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", prec);
	  tptr += strlen(tptr);
	}
	else
	{
	  while (isdigit(*format & 255))
	  {
	    if (tptr < (tformat + sizeof(tformat) - 1))
	      *tptr++ = *format;

            format ++;
	  }
	}
      }

      if (*format == 'l' && format[1] == 'l')
      {
        size = 'L';

	if (tptr < (tformat + sizeof(tformat) - 2))
	{
	  *tptr++ = 'l';
	  *tptr++ = 'l';
	}

	format += 2;
      }
      else if (*format == 'h' || *format == 'l' || *format == 'L')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *format;

        size = *format++;
      }
      else
      {
        size = 0;
      }

      if (!*format)
        break;

      if (tptr < (tformat + sizeof(tformat) - 1))
        *tptr++ = *format;

      type  = *format++;
      *tptr = '\0';

      switch (type)
      {
	case 'E' : // Floating point formats
	case 'G' :
	case 'e' :
	case 'f' :
	case 'g' :
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    snprintf(temp, sizeof(temp), tformat, va_arg(ap, double));

	    if ((decptr = strstr(temp, dec)) != NULL)
	    {
	      // Convert locale decimal point to "."
	      PDFIO_DEBUG("_pdfio_vsnprintf: Before \"%s\"\n", temp);
	      tempptr = decptr + strlen(dec);
	      if (tempptr > (decptr + 1))
	        memmove(decptr + 1, tempptr, strlen(tempptr) + 1);
	      *decptr = '.';

	      // Strip trailing 0's...
	      for (tempptr = temp + strlen(temp) - 1; tempptr > temp && *tempptr == '0'; tempptr --)
		*tempptr = '\0';

	      if (*tempptr == '.')
		*tempptr = '\0';	// Strip trailing decimal point

	      PDFIO_DEBUG("_pdfio_vsnprintf: After \"%s\"\n", temp);
	    }

            // Copy to the output buffer
            bytes += (int)strlen(temp);

            if (bufptr < bufend)
	    {
	      _pdfio_strlcpy(bufptr, temp, (size_t)(bufend - bufptr + 1));
	      bufptr += strlen(bufptr);
	    }
	    break;

        case 'B' : // Integer formats
	case 'X' :
	case 'b' :
        case 'd' :
	case 'i' :
	case 'o' :
	case 'u' :
	case 'x' :
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

#  ifdef HAVE_LONG_LONG
            if (size == 'L')
	      snprintf(temp, sizeof(temp), tformat, va_arg(ap, long long));
	    else
#  endif // HAVE_LONG_LONG
            if (size == 'l')
	      snprintf(temp, sizeof(temp), tformat, va_arg(ap, long));
	    else
	      snprintf(temp, sizeof(temp), tformat, va_arg(ap, int));

            bytes += (int)strlen(temp);

	    if (bufptr < bufend)
	    {
	      _pdfio_strlcpy(bufptr, temp, (size_t)(bufend - bufptr + 1));
	      bufptr += strlen(bufptr);
	    }
	    break;

	case 'p' : // Pointer value
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    snprintf(temp, sizeof(temp), tformat, va_arg(ap, void *));

            bytes += (int)strlen(temp);

	    if (bufptr < bufend)
	    {
	      _pdfio_strlcpy(bufptr, temp, (size_t)(bufend - bufptr + 1));
	      bufptr += strlen(bufptr);
	    }
	    break;

        case 'c' : // Character or character array
	    bytes += width;

	    if (bufptr < bufend)
	    {
	      if (width <= 1)
	      {
	        *bufptr++ = (char)va_arg(ap, int);
	      }
	      else
	      {
		if ((bufptr + width) > bufend)
		  width = (int)(bufend - bufptr);

		memcpy(bufptr, va_arg(ap, char *), (size_t)width);
		bufptr += width;
	      }
	    }
	    break;

	case 'S' : // PDF string
	    if ((s = va_arg(ap, char *)) == NULL)
	      s = "(null)";

            // PDF strings start with "("...
            if (bufptr < bufend)
              *bufptr++ = '(';

            bytes ++;

            // Loop through the literal string...
            while (*s)
            {
              // Escape special characters
              if (*s == '\\' || *s == '(' || *s == ')')
              {
                // Simple escape...
                if (bufptr < bufend)
                  *bufptr++ = '\\';

                if (bufptr < bufend)
                  *bufptr++ = *s;

                bytes += 2;
              }
              else if (*s < ' ')
              {
                // Octal escape...
                snprintf(bufptr, (size_t)(bufend - bufptr + 1), "\\%03o", *s & 255);
                bufptr += strlen(bufptr);
                bytes += 4;
              }
              else
              {
                // Literal character...
                if (bufptr < bufend)
                  *bufptr++ = *s;
                bytes ++;
              }

              s ++;
            }

            // PDF strings end with ")"...
            if (bufptr < bufend)
              *bufptr++ = ')';

            bytes ++;
	    break;

	case 's' : // Literal string
	    if ((s = va_arg(ap, char *)) == NULL)
	      s = "(null)";

            if (width != 0)
            {
              // Format string to fit inside the specified width...
	      if ((size_t)(width + 1) > sizeof(temp))
	        break;

              snprintf(temp, sizeof(temp), tformat, s);
              s = temp;
            }

            bytes += strlen(s);

	    if (bufptr < bufend)
	    {
	      _pdfio_strlcpy(bufptr, s, (size_t)(bufend - bufptr + 1));
	      bufptr += strlen(bufptr);
	    }
	    break;

        case 'N' : // Output name string with proper escaping
	    if ((s = va_arg(ap, char *)) == NULL)
	      s = "(null)";

	    // PDF names start with "/"...
            if (bufptr < bufend)
              *bufptr++ = '/';

            bytes ++;

            // Loop through the name string...
            while (*s)
            {
              if (*s < 0x21 || *s > 0x7e || *s == '#')
              {
                // Output #XX for character...
                snprintf(bufptr, (size_t)(bufend - bufptr + 1), "#%02X", *s & 255);
                bufptr += strlen(bufptr);
                bytes += 3;
              }
              else
              {
                // Output literal character...
                if (bufptr < bufend)
                  *bufptr++ = *s;
                bytes ++;
              }

              s ++;
            }
	    break;

	case 'n' : // Output number of chars so far
	    *(va_arg(ap, int *)) = (int)bytes;
	    break;
      }
    }
    else
    {
      // Literal character...
      bytes ++;

      if (bufptr < bufend)
        *bufptr++ = *format++;
    }
  }

  // Nul-terminate the string and return the number of characters needed.
  *bufptr = '\0';

  PDFIO_DEBUG("_pdfio_vsnprintf: Returning %ld \"%s\"\n", (long)bytes, buffer);

  return (bytes);
}


//
// '_pdfioStringAllocBuffer()' - Allocate a string buffer.
//

char *					// O - Buffer or `NULL` on error
_pdfioStringAllocBuffer(
    pdfio_file_t *pdf)			// I - PDF file
{
  _pdfio_strbuf_t	*current;	// Current string buffer


  // See if we have an available string buffer...
  for (current = pdf->strbuffers; current; current = current->next)
  {
    if (!current->bufused)
    {
      current->bufused = true;
      return (current->buffer);
    }
  }

  // Didn't find one, allocate a new one...
  if ((current = calloc(1, sizeof(_pdfio_strbuf_t))) == NULL)
    return (NULL);

  // Add to the linked list of string buffers...
  current->next    = pdf->strbuffers;
  current->bufused = true;

  pdf->strbuffers = current;

  return (current->buffer);
}


//
// 'pdfioStringCreate()' - Create a durable literal string.
//
// This function creates a literal string associated with the PDF file
// "pdf".  The "s" string points to a nul-terminated C string.
//
// `NULL` is returned on error, otherwise a `char *` that is valid until
// `pdfioFileClose` is called.
//

char *					// O - Durable string pointer or `NULL` on error
pdfioStringCreate(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *s)			// I - Nul-terminated string
{
  char		*news;			// New string
  size_t	idx;			// Index into strings
  int		diff;			// Different


  PDFIO_DEBUG("pdfioStringCreate(pdf=%p, s=\"%s\")\n", pdf, s);

  // Range check input...
  if (!pdf || !s)
    return (NULL);

  // See if the string has already been added...
  if (pdf->num_strings > 0)
  {
    idx = find_string(pdf, s, &diff);
    if (diff == 0)
      return (pdf->strings[idx]);
  }
  else
  {
    idx  = 0;
    diff = -1;
  }

  // Not already added, so add it...
  if ((news = strdup(s)) == NULL)
    return (NULL);

  if (pdf->num_strings >= pdf->alloc_strings)
  {
    // Expand the string array...
    char **temp = (char **)realloc(pdf->strings, (pdf->alloc_strings + 128) * sizeof(char *));

    if (!temp)
    {
      free(news);
      return (NULL);
    }

    pdf->strings       = temp;
    pdf->alloc_strings += 128;
  }

  // Insert the string...
  if (diff > 0)
    idx ++;

  PDFIO_DEBUG("pdfioStringCreate: Inserting \"%s\" at %u\n", news, (unsigned)idx);

  if (idx < pdf->num_strings)
    memmove(pdf->strings + idx + 1, pdf->strings + idx, (pdf->num_strings - idx) * sizeof(char *));

  pdf->strings[idx] = news;
  pdf->num_strings ++;

  PDFIO_DEBUG("pdfioStringCreate: %lu strings\n", (unsigned long)pdf->num_strings);

  return (news);
}


//
// 'pdfioStringCreatef()' - Create a durable formatted string.
//
// This function creates a formatted string associated with the PDF file
// "pdf".  The "format" string contains `printf`-style format characters.
//
// `NULL` is returned on error, otherwise a `char *` that is valid until
// `pdfioFileClose` is called.
//

char *					// O - Durable string pointer or `NULL` on error
pdfioStringCreatef(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *format,		// I - `printf`-style format string
    ...)				// I - Additional args as needed
{
  char		buffer[8192];		// String buffer
  va_list	ap;			// Argument list


  // Range check input...
  if (!pdf || !format)
    return (NULL);

  // Format the string...
  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  // Create the string from the buffer...
  return (pdfioStringCreate(pdf, buffer));
}


//
// '_pdfioStringFreeBuffer()' - Free a string buffer.
//

void
_pdfioStringFreeBuffer(
    pdfio_file_t *pdf,			// I - PDF file
    char         *buffer)		// I - String buffer
{
  _pdfio_strbuf_t	*current;	// Current string buffer


  for (current = pdf->strbuffers; current; current = current->next)
  {
    if (current->buffer == buffer)
    {
      current->bufused = false;
      break;
    }
  }
}


//
// '_pdfioStringIsAllocated()' - Check whether a string has been allocated.
//

bool					// O - `true` if allocated, `false` otherwise
_pdfioStringIsAllocated(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *s)			// I - String
{
  int	diff;				// Difference


  if (pdf->num_strings == 0)
    return (false);

  find_string(pdf, s, &diff);

  return (diff == 0);
}


//
// 'find_string()' - Find an element in the array.
//

static size_t				// O - Index of match
find_string(pdfio_file_t *pdf,		// I - PDF file
	    const char   *s,		// I - String to find
	    int          *rdiff)	// O - Difference of match
{
  size_t	left,			// Left side of search
		right,			// Right side of search
		current;		// Current element
  int		diff;			// Comparison with current element


  // Do a binary search for the string...
  left  = 0;
  right = pdf->num_strings - 1;

  do
  {
    current = (left + right) / 2;
    diff    = strcmp(s, pdf->strings[current]);

    if (diff == 0)
      break;
    else if (diff < 0)
      right = current;
    else
      left = current;
  }
  while ((right - left) > 1);

  if (diff != 0)
  {
    // Check the last 1 or 2 elements...
    if ((diff = strcmp(s, pdf->strings[left])) <= 0)
    {
      current = left;
    }
    else
    {
      diff    = strcmp(s, pdf->strings[right]);
      current = right;
    }
  }

  // Return the closest string and the difference...
  *rdiff = diff;

  return (current);
}
