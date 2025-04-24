//
// PDF merge program for PDFio.
//
// Copyright © 2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Usage:
//
//   ./pdfiomerge [-o OUTPUT.pdf] INPUT.pdf [... INPUT.pdf]
//   ./pdfiomerge INPUT.pdf [... INPUT.pdf] >OUTPUT.pdf
//

#include <pdfio.h>
#include <string.h>


//
// Local functions...
//

static ssize_t	output_cb(void *output_cbdata, const void *buffer, size_t bytes);
static int	usage(FILE *out);


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  const char	*opt;			// Current option
  pdfio_file_t	*inpdf,			// Input PDF file
		*outpdf = NULL;		// Output PDF file


  // Parse command-line...
  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
    {
      return (usage(stdout));
    }
    else if (!strncmp(argv[i], "--", 2))
    {
      fprintf(stderr, "pdfiomerge: Unknown option '%s'.\n", argv[i]);
      return (usage(stderr));
    }
    else if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
        {
          case 'o' : // -o OUTPUT.pdf
              if (outpdf)
              {
                fputs("pdfiomerge: Only one output file can be specified.\n", stderr);
                return (usage(stderr));
              }

              i ++;
              if (i >= argc)
              {
                fputs("pdfiomerge: Missing output filename after '-o'.\n", stderr);
                return (usage(stderr));
              }

              if ((outpdf = pdfioFileCreate(argv[i], /*version*/NULL, /*media_box*/NULL, /*crop_box*/NULL, /*error_cb*/NULL, /*error_data*/NULL)) == NULL)
                return (1);
              break;

          default :
              fprintf(stderr, "pdfiomerge: Unknown option '-%c'.\n", *opt);
              return (usage(stderr));
        }
      }
    }
    else if ((inpdf = pdfioFileOpen(argv[i], /*password_cb*/NULL, /*password_data*/NULL, /*error_cb*/NULL, /*error_data*/NULL)) == NULL)
    {
      return (1);
    }
    else
    {
      // Copy PDF file...
      size_t	p,			// Current page
		nump;			// Number of pages

      if (!outpdf)
      {
	if ((outpdf = pdfioFileCreateOutput(output_cb, /*output_cbdata*/NULL, /*version*/NULL, /*media_box*/NULL, /*crop_box*/NULL, /*error_cb*/NULL, /*error_data*/NULL)) == NULL)
	  return (1);
      }

      for (p = 0, nump = pdfioFileGetNumPages(inpdf); p < nump; p ++)
      {
        if (!pdfioPageCopy(outpdf, pdfioFileGetPage(inpdf, p)))
          return (1);
      }

      pdfioFileClose(inpdf);
    }
  }

  if (!outpdf)
    return (usage(stderr));

  pdfioFileClose(outpdf);

  return (0);
}


//
// 'output_cb()' - Write PDF data to the standard output...
//

static ssize_t				// O - Number of bytes written
output_cb(void       *output_cbdata,	// I - Callback data (not used)
          const void *buffer,		// I - Buffer to write
          size_t     bytes)		// I - Number of bytes to write
{
  (void)output_cbdata;

  return ((ssize_t)fwrite(buffer, 1, bytes, stdout));
}


//
// 'usage()' - Show program usage.
//

static int				// O - Exit status
usage(FILE *out)			// I - stdout or stderr
{
  fputs("Usage: pdfmerge [OPTIONS] INPUT.pdf [... INPUT.pdf] >OUTPUT.pdf\n", out);
  fputs("Options:\n", out);
  fputs("  --help                   Show help.\n", out);
  fputs("  -o OUTPUT.pdf            Send output to filename instead of stdout.\n", out);

  return (out == stdout ? 0 : 1);
}
