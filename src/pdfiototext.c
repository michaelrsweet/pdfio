//
// PDF to text program for PDFio.
//
// Copyright © 2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Usage:
//
//   ./pdfiototext FILENAME.pdf > FILENAME.txt
//

#include "pdfio.h"
#include <string.h>


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  pdfio_file_t	*file;			// PDF file
  size_t	i, j,			// Looping vars
		num_pages,		// Number of pages
		num_streams;		// Number of streams for page
  pdfio_obj_t	*obj;			// Current page object
  pdfio_stream_t *st;			// Current page content stream
  char		buffer[1024];		// String buffer
  bool		first;			// First string token?


  // Verify command-line arguments...
  if (argc != 2)
  {
    puts("Usage: pdfiototext FILENAME.pdf > FILENAME.txt");
    return (1);
  }

  // Open the PDF file...
  if ((file = pdfioFileOpen(argv[1], NULL, NULL, NULL, NULL)) == NULL)
    return (1);

//  printf("%s: %u pages\n", argv[1], (unsigned)pdfioFileGetNumPages(file));

  // Try grabbing content from all of the pages...
  for (i = 0, num_pages = pdfioFileGetNumPages(file); i < num_pages; i ++)
  {
    if ((obj = pdfioFileGetPage(file, i)) == NULL)
      continue;

    num_streams = pdfioPageGetNumStreams(obj);

//    printf("%s: page%u=%p, num_streams=%u\n", argv[1], (unsigned)i, obj, (unsigned)num_streams);

    for (j = 0; j < num_streams; j ++)
    {
      if ((st = pdfioPageOpenStream(obj, j, true)) == NULL)
	continue;

//      printf("%s: page%u st%u=%p\n", argv[1], (unsigned)i, (unsigned)j, st);

      first = true;
      while (pdfioStreamGetToken(st, buffer, sizeof(buffer)))
      {
	if (buffer[0] == '(')
	{
	  if (first)
	    first = false;
	  else
	    putchar(' ');

	  fputs(buffer + 1, stdout);
	}
	else if (!strcmp(buffer, "Td") || !strcmp(buffer, "TD") || !strcmp(buffer, "T*") || !strcmp(buffer, "\'") || !strcmp(buffer, "\""))
	{
	  putchar('\n');
	  first = true;
	}
      }

      if (!first)
	putchar('\n');

      pdfioStreamClose(st);
    }
  }

  pdfioFileClose(file);

  return (0);
}
