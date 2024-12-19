//
// Image example for PDFio.
//
// Copyright © 2023-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Usage:
//
//   ./image2pdf FILENAME.{jpg,png} FILENAME.pdf ["TEXT"]
//

#include <pdfio.h>
#include <pdfio-content.h>
#include <string.h>


//
// 'create_pdf_image_file()' - Create a PDF file of an image with optional caption.
//

bool					// O - True on success, false on failure
create_pdf_image_file(
    const char *pdfname,		// I - PDF filename
    const char *imagename,		// I - Image filename
    const char *caption)		// I - Caption filename
{
  pdfio_file_t   *pdf;			// PDF file
  pdfio_obj_t    *font;			// Caption font
  pdfio_obj_t    *image;		// Image
  pdfio_dict_t   *dict;			// Page dictionary
  pdfio_stream_t *page;			// Page stream
  double         width, height;		// Width and height of image
  double         swidth, sheight;	// Scaled width and height on page
  double         tx, ty;		// Position on page


  // Create the PDF file...
  pdf = pdfioFileCreate(pdfname, /*version*/NULL, /*media_box*/NULL,
                        /*crop_box*/NULL, /*error_cb*/NULL,
                        /*error_cbdata*/NULL);
  if (!pdf)
    return (false);

  // Create a Courier base font for the caption
  font = pdfioFileCreateFontObjFromBase(pdf, "Courier");

  if (!font)
  {
    pdfioFileClose(pdf);
    return (false);
  }

  // Create an image object from the JPEG/PNG image file...
  image = pdfioFileCreateImageObjFromFile(pdf, imagename, true);

  if (!image)
  {
    pdfioFileClose(pdf);
    return (false);
  }

  // Create a page dictionary with the font and image...
  dict = pdfioDictCreate(pdf);
  pdfioPageDictAddFont(dict, "F1", font);
  pdfioPageDictAddImage(dict, "IM1", image);

  // Create the page and its content stream...
  page = pdfioFileCreatePage(pdf, dict);

  // Position and scale the image on the page...
  width  = pdfioImageGetWidth(image);
  height = pdfioImageGetHeight(image);

  // Default media_box is "universal" 595.28x792 points (8.27x11in or
  // 210x279mm).  Use margins of 36 points (0.5in or 12.7mm) with another
  // 36 points for the caption underneath...
  swidth  = 595.28 - 72.0;
  sheight = swidth * height / width;
  if (sheight > (792.0 - 36.0 - 72.0))
  {
    sheight = 792.0 - 36.0 - 72.0;
    swidth  = sheight * width / height;
  }

  tx = 0.5 * (595.28 - swidth);
  ty = 0.5 * (792 - 36 - sheight);

  pdfioContentDrawImage(page, "IM1", tx, ty + 36.0, swidth, sheight);

  // Draw the caption in black...
  pdfioContentSetFillColorDeviceGray(page, 0.0);

  // Compute the starting point for the text - Courier is monospaced
  // with a nominal width of 0.6 times the text height...
  tx = 0.5 * (595.28 - 18.0 * 0.6 * strlen(caption));

  // Position and draw the caption underneath...
  pdfioContentTextBegin(page);
  pdfioContentSetTextFont(page, "F1", 18.0);
  pdfioContentTextMoveTo(page, tx, ty);
  pdfioContentTextShow(page, /*unicode*/false, caption);
  pdfioContentTextEnd(page);

  // Close the page stream and the PDF file...
  pdfioStreamClose(page);
  pdfioFileClose(pdf);

  return (true);
}


//
// 'main()' - Produce a single-page file from an image.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  const char	*imagefile,		// Image filename
		*pdffile,		// PDF filename
		*caption;		// Caption text


  // Get the image file, PDF file, and optional caption text from the command-line...
  if (argc < 3 || argc > 4)
  {
    fputs("Usage: image2pdf FILENAME.{jpg,png} FILENAME.pdf [\"TEXT\"]\n", stderr);
    return (1);
  }

  imagefile = argv[1];
  pdffile   = argv[2];
  caption   = argv[3];

  return (create_pdf_image_file(imagefile, pdffile, caption) ? 0 : 1);
}
