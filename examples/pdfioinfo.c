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
  const char	*author,		// Author name
  		*creator,		// Creator name
  		*producer;		// Producer name
  time_t	creation_date,		// Creation date
		mod_date;		// Modification date
  struct tm	*creation_tm,		// Creation date/time information
		*mod_tm;		// Mod. date/time information
  char		creation_text[256],	// Creation date/time as a string
		mod_text[256];		// Mod. date/time human fmt string
  const char	*title;			// Title
  size_t	num_pages;		// PDF number of pages
  bool		has_acroform;		// AcroForm or not


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

  // Get the title, author...
  author = pdfioFileGetAuthor(pdf);
  title  = pdfioFileGetTitle(pdf);
  creator = pdfioFileGetCreator(pdf);
  producer = pdfioFileGetProducer(pdf);
  num_pages = pdfioFileGetNumPages(pdf);

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

  // Get the modification date and convert to a string...
  if ((mod_date = pdfioFileGetModDate(pdf)) > 0)
  {
    mod_tm   = localtime(&mod_date);
    strftime(mod_text, sizeof(mod_text), "%c", mod_tm);
  }
  else
  {
    snprintf(mod_text, sizeof(mod_text), "-- not set --");
  }

  // Detect simply if AcroFrom is a dict in catalog
  {
    pdfio_dict_t	*dict;			// some Object dictionary
   
    dict = pdfioFileGetCatalog(pdf);
    has_acroform = (dict != NULL && pdfioDictGetObj(dict, "AcroForm") != NULL)?
                   true : false;
  }

  // Print file information to stdout...
  printf("%s:\n", filename);
  printf("         Title: %s\n", title ? title : "-- not set --");
  printf("        Author: %s\n", author ? author : "-- not set --");
  printf("       Creator: %s\n", creator ? creator : "-- not set --");
  printf("      Producer: %s\n", producer ? producer : "-- not set --");
  printf("    Created On: %s\n", creation_text);
  printf("   Modified On: %s\n", mod_text);
  printf("       Version: %s\n", pdfioFileGetVersion(pdf));
  printf("      AcroForm: %s\n", has_acroform ? "Yes" : "No");
  printf("  Number Pages: %u\n", (unsigned)num_pages);
  printf("    MediaBoxes:");
  // There can be a different MediaBox per page
  // Loop and report MediaBox and number of consecutive pages of this size
  {
    pdfio_obj_t		*obj;			// Object
    pdfio_dict_t	*dict;			// Object dictionary
    pdfio_rect_t 	prev,			// MediaBox previous
     			now;			// MediaBox now
    size_t 		n,			// Page index
          		nprev;			// Number previous prev size

    // MediaBox should be set at least on the root
    for (n = nprev = 0; n < num_pages; n++)
    {
      obj = pdfioFileGetPage(pdf, n);
      while (obj != NULL)
      {
        dict = pdfioObjGetDict(obj);
        if (pdfioDictGetRect(dict, "MediaBox", &now))
        {
          if ( 
                nprev == 0
                || (
                     now.x1 != prev.x1 || now.y1 != prev.y1
                     || now.x2 != prev.x2 || now.y2 != prev.y2
                   )
             )
          {
            if (nprev) printf("(%zd) ", nprev);
            prev = now;
            printf("[%.7g %.7g %.7g %.7g]", now.x1, now.y1, now.x2, now.y2);
            nprev = 1;
          }
          else
            ++nprev;
	  obj = NULL;
        }
        else
	  obj = pdfioDictGetObj(dict, "Parent");
      }
    }
    printf("(%zd)", nprev);
  }
  printf("\n");


  // Close the PDF file...
  pdfioFileClose(pdf);

  return (0);
}
