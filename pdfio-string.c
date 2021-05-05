//
// PDF dictionary functions for pdfio.
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

static int	compare_strings(char **a, char **b);


//
// 'pdfioStringCreate()' - Create a literal string.
//
// This function creates a literal string associated with the PDF file
// "pwg".  The "s" string points to a nul-terminated C string.
//
// `NULL` is returned on error, otherwise a `char *` that is valid until
// `pdfioFileClose` is called.
//

char *					// O - New string or `NULL` on error
pdfioStringCreate(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *s)			// I - Nul-terminated string
{
  char	*news;				// New string


  PDFIO_DEBUG("pdfioStringCreate(pdf=%p, s=\"%s\")\n", pdf, s);

  // Range check input...
  if (!pdf || !s)
    return (NULL);

  // See if the string has already been added...
  if (pdf->num_strings > 0 && (news = (char *)bsearch(&s, pdf->strings, pdf->num_strings, sizeof(char *), (int (*)(const void *, const void *))compare_strings)) != NULL)
    return (news);

  // Not already added, so add it...
  if ((news = strdup(s)) == NULL)
    return (NULL);

  if (pdf->num_strings >= pdf->alloc_strings)
  {
    // Expand the string array...
    char **temp = (char **)realloc(pdf->strings, (pdf->alloc_strings + 32) * sizeof(char *));

    if (!temp)
    {
      free(news);
      return (NULL);
    }

    pdf->strings       = temp;
    pdf->alloc_strings += 32;
  }

  // TODO: Change to insertion sort as needed...
  pdf->strings[pdf->num_strings ++] = news;

  if (pdf->num_strings > 1)
    qsort(pdf->strings, pdf->num_strings, sizeof(char *), (int (*)(const void *, const void *))compare_strings);

#ifdef DEBUG
  {
    size_t	i;			// Looping var

    PDFIO_DEBUG("pdfioStringCreate: %lu strings\n", (unsigned long)pdf->num_strings);
    for (i = 0; i < pdf->num_strings; i ++)
      PDFIO_DEBUG("pdfioStringCreate: strings[%lu]=%p(\"%s\")\n", (unsigned long)i, pdf->strings[i], pdf->strings[i]);
  }
#endif // DEBUG

  return (news);
}


//
// 'pdfioStringCreatef()' - Create a formatted string.
//
// This function creates a formatted string associated with the PDF file
// "pwg".  The "format" string contains `printf`-style format characters.
//
// `NULL` is returned on error, otherwise a `char *` that is valid until
// `pdfioFileClose` is called.
//

char *					// O - New string or `NULL` on error
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
