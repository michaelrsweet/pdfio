//
// PDF metadata example for PDFio.
//
// Copyright © 2023-2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Usage:
//
//   ./pdfioinfo FILENAME.pdf
//

#include <pdfio.h>
#include <time.h>


//
// 'main()' - Open a PDF file and show its metadata.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// Command-line arguments
{
  const char	*filename;		// PDF filename
  pdfio_file_t	*pdf;			// PDF file
  const char	*author;		// Author name
  time_t	creation_date;		// Creation date
  struct tm	*creation_tm;		// Creation date/time information
  char		creation_text[256];	// Creation date/time as a string
  const char	*title;			// Title


  // Get the filename from the command-line...
  if (argc != 2)
  {
    fputs("Usage: ./pdfioinfo FILENAME.pdf\n", stderr);
    return (1);
  }

  filename = argv[1];

  // Open the PDF file with the default callbacks...
  pdf = pdfioFileOpen(filename, /*password_cb*/NULL,
                      /*password_cbdata*/NULL, /*error_cb*/NULL,
                      /*error_cbdata*/NULL);
  if (pdf == NULL)
    return (1);

  // Get the title and author...
  author = pdfioFileGetAuthor(pdf);
  title  = pdfioFileGetTitle(pdf);

  // Get the creation date and convert to a string...
  if ((creation_date = pdfioFileGetCreationDate(pdf)) > 0)
  {
    creation_tm   = localtime(&creation_date);
    strftime(creation_text, sizeof(creation_text), "%c", creation_tm);
  }
  else
  {
    snprintf(creation_text, sizeof(creation_text), "-- not set --");
  }

  // Print file information to stdout...
  printf("%s:\n", filename);
  printf("         Title: %s\n", title ? title : "-- not set --");
  printf("        Author: %s\n", author ? author : "-- not set --");
  printf("    Created On: %s\n", creation_text);
  printf("  Number Pages: %u\n", (unsigned)pdfioFileGetNumPages(pdf));

  // Close the PDF file...
  pdfioFileClose(pdf);

  return (0);
}
