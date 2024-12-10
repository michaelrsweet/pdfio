//
// Code 128 barcode example for PDFio.
//
// Copyright © 2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Usage:
//
//   ./code128 "BARCODE" ["TEXT"] >FILENAME.pdf
//

#include <pdfio.h>
#include <pdfio-content.h>


//
// 'make_code128()' - Make a Code 128 barcode string.
//
// This function produces a Code B (printable ASCII) representation of the
// source string and doesn't try to optimize using Code C.  Non-printable and
// extended characters are ignored in the source string.
//


static char *				// O - Output string
make_code128(char       *dst,		// I - Destination buffer
             const char *src,		// I - Source string
             size_t     dstsize)	// I - Size of destination buffer
{
  char		*dstptr,		// Pointer into destination buffer
		*dstend;		// End of destination buffer
  int		sum = 0;		// Weighted sum
  static const char *code128_chars =	// Code 128 characters
		" !\"#$%&'()*+,-./0123456789:;<=>?"
		"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
		"`abcdefghijklmnopqrstuvwxyz{|}~\303"
		"\304\305\306\307\310\311\312";
  static const char code128_fnc_3 = '\304';
					// FNC 3
  static const char code128_fnc_2 = '\305';
					// FNC 2
  static const char code128_shift_b = '\306';
					// Shift B (for lowercase)
  static const char code128_code_c = '\307';
					// Code C
  static const char code128_code_b = '\310';
					// Code B
  static const char code128_fnc_4 = '\311';
					// FNC 4
  static const char code128_fnc_1 = '\312';
					// FNC 1
  static const char code128_start_code_a = '\313';
					// Start code A
  static const char code128_start_code_b = '\314';
					// Start code A
  static const char code128_start_code_c = '\315';
					// Start code A
  static const char code128_stop = '\316';
					// Stop pattern


  // Start a Code B barcode...
  dstptr = dst;
  dstend = dst + dstsize - 3;

  *dstptr++ = code128_start_code_b;
  sum       = code128_start_code_b - 100;

  while (*src && dstptr < dstend)
  {
    if (*src >= ' ' && *src < 0x7f)
    {
      sum       += (dstptr - dst) * (*src - ' ');
      *dstptr++ = *src;
    }

    src ++;
  }

  // Add the weighted sum modulo 103
  *dstptr++ = code128_chars[sum % 103];

  // Add the stop pattern and return...
  *dstptr++ = code128_stop;
  *dstptr   = '\0';

  return (dst);
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
// 'main()' - Produce a single-page barcode file.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  const char	*barcode,		// Barcode to show
		*text;			// Text to display under barcode
  pdfio_file_t	*pdf;			// Output PDF file
  pdfio_obj_t	*barcode_font;		// Barcode font object
  pdfio_obj_t	*text_font = NULL;	// Text font object
  pdfio_dict_t	*page_dict;		// Page dictionary
  pdfio_rect_t	media_box;		// Media/CropBox for page
  pdfio_stream_t *page_st;		// Page stream
  char		barcode_temp[256];	// Barcode buffer
  double	barcode_height = 36.0,	// Height of barcode
		barcode_width,		// Width of barcode
		text_height = 0.0,	// Height of text
		text_width = 0.0;	// Width of text


  // Get the barcode and optional text from the command-line...
  if (argc < 2 || argc > 3)
  {
    fputs("Usage: code128 \"BARCODE\" [\"TEXT\"] >FILENAME.pdf\n", stderr);
    return (1);
  }

  barcode = argv[1];
  text    = argv[2];

  // Output a PDF file to the standard output...
#ifdef _WIN32
  setmode(1, O_BINARY);			// Force binary output on Windows
#endif // _WIN32

  if ((pdf = pdfioFileCreateOutput(output_cb, /*output_cbdata*/NULL, /*version*/NULL, /*media_box*/NULL, /*crop_box*/NULL, /*error_cb*/NULL, /*error_data*/NULL)) == NULL)
    return (1);

  // Load fonts...
  barcode_font = pdfioFileCreateFontObjFromFile(pdf, "code128.ttf", /*unicode*/false);
  if (text)
    text_font = pdfioFileCreateFontObjFromFile(pdf, "../testfiles/OpenSans-Regular.ttf", /*unicode*/true);

  // Generate Code128 characters for the desired barcode...
  if (!(barcode[0] & 0x80))
    barcode = make_code128(barcode_temp, barcode, sizeof(barcode_temp));

  // Compute sizes of the text...
  barcode_width = pdfioContentTextMeasure(barcode_font, barcode, barcode_height);
  if (text && text_font)
  {
    text_height = 9.0;
    text_width  = pdfioContentTextMeasure(text_font, text, text_height);
  }

  // Compute the size of the PDF page...
  media_box.x1 = 0.0;
  media_box.y1 = 0.0;
  media_box.x2 = (barcode_width > text_width ? barcode_width : text_width) + 18.0;
  media_box.y2 = barcode_height + text_height + 18.0;

  // Start a page for the barcode...
  page_dict = pdfioDictCreate(pdf);

  pdfioDictSetRect(page_dict, "MediaBox", &media_box);
  pdfioDictSetRect(page_dict, "CropBox", &media_box);

  pdfioPageDictAddFont(page_dict, "B128", barcode_font);
  if (text_font)
    pdfioPageDictAddFont(page_dict, "TEXT", text_font);

  page_st = pdfioFileCreatePage(pdf, page_dict);

  // Draw the page...
  pdfioContentSetStrokeColorGray(page_st, 0.0);

  pdfioContentSetTextFont(page_st, "B128", barcode_height);
  pdfioContentTextBegin(page_st);
  pdfioContentTextMoveTo(page_st, 0.5 * (media_box.x2 - barcode_width), 9.0 + text_height);
  pdfioContentTextShow(page_st, /*unicode*/false, barcode);
  pdfioContentTextEnd(page_st);

  if (text && text_font)
  {
    pdfioContentSetTextFont(page_st, "TEXT", text_height);
    pdfioContentTextBegin(page_st);
    pdfioContentTextMoveTo(page_st, 0.5 * (media_box.x2 - text_width), 9.0);
    pdfioContentTextShow(page_st, /*unicode*/true, text);
    pdfioContentTextEnd(page_st);
  }

  pdfioStreamClose(page_st);

  // Close and return...
  pdfioFileClose(pdf);

  return (0);
}

