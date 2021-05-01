//
// Test program for pdfio.
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pdfio.h"


//
// 'main()' - Main entry for test program.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  if (argc > 1)
  {
    int			i;		// Looping var
    pdfio_file_t	*pdf;		// PDF file

    for (i = 1; i < argc; i ++)
    {
      if ((pdf = pdfioFileOpen(argv[i], NULL, NULL)) != NULL)
      {
	printf("%s: PDF %s, %d objects.\n", argv[i], pdfioFileGetVersion(pdf), (int)pdfioFileGetNumObjects(pdf));
	pdfioFileClose(pdf);
      }
    }
  }

  return (0);
}
