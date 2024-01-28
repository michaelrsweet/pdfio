//
// PDF string functions for PDFio.
//
// Copyright © 2021-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pdfio-private.h"


//
// Local functions...
//

static int	compare_strings(char **a, char **b);


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
  bufptr  = buffer;
  bufend  = buffer + bufsize - 1;
  *bufend = '\0';
  bytes   = 0;

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
	  prec = 0;

	  while (isdigit(*format & 255))
	  {
	    if (tptr < (tformat + sizeof(tformat) - 1))
	      *tptr++ = *format;

	    prec = prec * 10 + *format++ - '0';
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
	      strncpy(bufptr, temp, (size_t)(bufend - bufptr - 1));
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
	      strncpy(bufptr, temp, (size_t)(bufend - bufptr - 1));
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
	      strncpy(bufptr, temp, (size_t)(bufend - bufptr - 1));
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

	case 's' : // String
	    if ((s = va_arg(ap, char *)) == NULL)
	      s = "(null)";

            bytes += strlen(s);

	    if (bufptr < bufend)
	    {
	      strncpy(bufptr, s, (size_t)(bufend - bufptr - 1));
	      bufptr += strlen(bufptr);
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
  if (bufptr < bufend)
  {
    // Everything fit in the buffer...
    *bufptr = '\0';
  }

  PDFIO_DEBUG("_pdfio_vsnprintf: Returning %ld \"%s\"\n", (long)bytes, buffer);

  return (bytes);
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
  char	*news;				// New string
  char	**match;			// Matching string


  PDFIO_DEBUG("pdfioStringCreate(pdf=%p, s=\"%s\")\n", pdf, s);

  // Range check input...
  if (!pdf || !s)
    return (NULL);

  // See if the string has already been added...
  if (pdf->num_strings > 0 && (match = (char **)bsearch(&s, pdf->strings, pdf->num_strings, sizeof(char *), (int (*)(const void *, const void *))compare_strings)) != NULL)
    return (*match);

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

  // TODO: Change to insertion sort as needed...
  pdf->strings[pdf->num_strings ++] = news;

  if (pdf->num_strings > 1)
    qsort(pdf->strings, pdf->num_strings, sizeof(char *), (int (*)(const void *, const void *))compare_strings);

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
// '_pdfioStringIsAllocated()' - Check whether a string has been allocated.
//

bool					// O - `true` if allocated, `false` otherwise
_pdfioStringIsAllocated(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *s)			// I - String
{
  return (pdf->num_strings > 0 && bsearch(&s, pdf->strings, pdf->num_strings, sizeof(char *), (int (*)(const void *, const void *))compare_strings) != NULL);
}


//
// 'compare_strings()' - Compare two strings.
//

static int				// O - Result of comparison
compare_strings(char **a,		// I - First string
                char **b)		// I - Second string
{
  return (strcmp(*a, *b));
}
