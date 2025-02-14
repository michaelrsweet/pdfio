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
#include <math.h>


//
// 'main()' - Open a PDF file and show its metadata.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// Command-line arguments
{
  const char	*filename;		// PDF filename
  pdfio_file_t	*pdf;			// PDF file
  pdfio_dict_t	*catalog;		// Catalog dictionary
  const char	*author,		// Author name
  		*creator,		// Creator name
  		*producer,		// Producer name
		*title;			// Title
  time_t	creation_date,		// Creation date
		modification_date;	// Modification date
  struct tm	*creation_tm,		// Creation date/time information
		*modification_tm;	// Modification date/time information
  char		creation_text[256],	// Creation date/time as a string
		modification_text[256],	// Modification date/time human fmt string
		range_text[255];	// Page range text
  size_t	num_pages;		// PDF number of pages
  bool		has_acroform;		// Does the file have an AcroForm?
  pdfio_obj_t	*page;			// Object
  pdfio_dict_t	*page_dict;		// Object dictionary
  size_t	cur,			// Current page index
		prev;			// Previous page index
  pdfio_rect_t	cur_box,		// Current MediaBox
		prev_box;		// Previous MediaBox


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

  // Get the title, author, etc...
  catalog      = pdfioFileGetCatalog(pdf);
  author       = pdfioFileGetAuthor(pdf);
  creator      = pdfioFileGetCreator(pdf);
  has_acroform = pdfioDictGetObj(catalog, "AcroForm") != NULL ? true : false;
  num_pages    = pdfioFileGetNumPages(pdf);
  producer     = pdfioFileGetProducer(pdf);
  title        = pdfioFileGetTitle(pdf);

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
  if ((modification_date = pdfioFileGetModificationDate(pdf)) > 0)
  {
    modification_tm = localtime(&modification_date);
    strftime(modification_text, sizeof(modification_text), "%c", modification_tm);
  }
  else
  {
    snprintf(modification_text, sizeof(modification_text), "-- not set --");
  }

  // Print file information to stdout...
  printf("%s:\n", filename);
  printf("           Title: %s\n", title ? title : "-- not set --");
  printf("          Author: %s\n", author ? author : "-- not set --");
  printf("         Creator: %s\n", creator ? creator : "-- not set --");
  printf("        Producer: %s\n", producer ? producer : "-- not set --");
  printf("      Created On: %s\n", creation_text);
  printf("     Modified On: %s\n", modification_text);
  printf("         Version: %s\n", pdfioFileGetVersion(pdf));
  printf("        AcroForm: %s\n", has_acroform ? "Yes" : "No");
  printf(" Number of Pages: %u\n", (unsigned)num_pages);

  // Report the MediaBox for all of the pages
  prev_box.x1 = prev_box.x2 = prev_box.y1 = prev_box.y2 = 0.0;

  for (cur = 0, prev = 0; cur < num_pages; cur ++)
  {
    // Find the MediaBox for this page in the page tree...
    for (page = pdfioFileGetPage(pdf, cur);
         page != NULL;
         page = pdfioDictGetObj(page_dict, "Parent"))
    {
      cur_box.x1 = cur_box.x2 = cur_box.y1 = cur_box.y2 = 0.0;
      page_dict  = pdfioObjGetDict(page);

      if (pdfioDictGetRect(page_dict, "MediaBox", &cur_box))
        break;
    }

    // If this MediaBox is different from the previous one, show the range of
    // pages that have that size...
    if (cur == 0 ||
        fabs(cur_box.x1 - prev_box.x1) > 0.01 ||
        fabs(cur_box.y1 - prev_box.y1) > 0.01 ||
        fabs(cur_box.x2 - prev_box.x2) > 0.01 ||
        fabs(cur_box.y2 - prev_box.y2) > 0.01)
    {
      if (cur > prev)
      {
        snprintf(range_text, sizeof(range_text), "Pages %u-%u",
                 (unsigned)(prev + 1), (unsigned)cur);
        printf("%16s: [%g %g %g %g]\n", range_text,
               prev_box.x1, prev_box.y1, prev_box.x2, prev_box.y2);
      }

      // Start a new series of pages with the new size...
      prev     = cur;
      prev_box = cur_box;
    }
  }

  // Show the last range as needed...
  if (cur > prev)
  {
    snprintf(range_text, sizeof(range_text), "Pages %u-%u",
	     (unsigned)(prev + 1), (unsigned)cur);
    printf("%16s: [%g %g %g %g]\n", range_text,
	   prev_box.x1, prev_box.y1, prev_box.x2, prev_box.y2);
  }

  // Close the PDF file...
  pdfioFileClose(pdf);

  return (0);
}
