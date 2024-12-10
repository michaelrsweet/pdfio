//
// Simple markdown to PDF converter example for PDFio.
//
// Copyright © 2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Usage:
//
//   ./md2pdf FILENAME.md >FILENAME.pdf
//
// The generated PDF file is formatted for a "universal" paper size (8.27x11",
// the intersection of US Letter and ISO A4) with 1" top and bottom margins and
// 0.5" side margins.  The document title (if present) is centered at the top
// of the second and subsequent pages while the current heading and page number
// are provided at the bottom of each page.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mmd.h"
#include <pdfio.h>
#include <pdfio-content.h>


//
// Types...
//

typedef enum doccolor_e			// Document color enumeration
{
  DOCCOLOR_BLACK,			// #000
  DOCCOLOR_RED,				// #900
  DOCCOLOR_GREEN,			// #090
  DOCCOLOR_BLUE,			// #00C
  DOCCOLOR_GRAY				// #555
} doccolor_t;

typedef enum docfont_e			// Document font enumeration
{
  DOCFONT_REGULAR,			// Roboto-Regular
  DOCFONT_BOLD,				// Roboto-Bold
  DOCFONT_ITALIC,			// Roboto-Italic
  DOCFONT_MONOSPACE,			// RobotoMono-Regular
  DOCFONT_MAX				// Number of fonts
} docfont_t;

typedef struct docimage_s		// Document image info
{
  const char	*url;			// Reference URL
  pdfio_obj_t	*obj;			// Image object
} docimage_t;

#define DOCIMAGE_MAX	1000		// Maximum number of images

typedef struct docdata_s		// Document formatting data
{
  pdfio_file_t	*pdf;			// PDF file
  pdfio_rect_t	media_box;		// Media (page) box
  pdfio_rect_t	crop_box;		// Crop box (for margins)
  pdfio_rect_t	art_box;		// Art box (for markdown content)
  pdfio_obj_t	*fonts[DOCFONT_MAX];	// Embedded fonts
  size_t	num_images;		// Number of embedded images
  docimage_t	images[DOCIMAGE_MAX];	// Embedded images
  const char	*title;			// Document title
  char		*heading;		// Current document heading
  pdfio_stream_t *st;			// Current page stream
  double	y;			// Current position on page
  doccolor_t	color;			// Current color
} docdata_t;


//
// Macros...
//

#define in2pt(in) (in * 72.0)
#define mm2pt(mm) (mm * 72.0 / 25.4)


//
// Constants...
//

static const char * const docfont_filenames[] =
{
  "Roboto-Regular.ttf",
  "Roboto-Bold.ttf",
  "Roboto-Italic.ttf",
  "RobotoMono-Regular.ttf"
};

static const char * const docfont_names[] =
{
  "FR",
  "FB",
  "FI",
  "FM"
};

#define LINE_HEIGHT	1.4		// Multiplier for line height

#define SIZE_BODY	11.0		// Size of body text (points)
#define SIZE_CODEBLOCK	10.0		// Size of code block text (points)
#define SIZE_HEADFOOT	9.0		// Size of header/footer text (points)
#define SIZE_HEADING_1	18.0		// Size of first level heading (points)
#define SIZE_HEADING_2	16.0		// Size of top level heading (points)
#define SIZE_HEADING_3	15.0		// Size of top level heading (points)
#define SIZE_HEADING_4	14.0		// Size of top level heading (points)
#define SIZE_HEADING_5	13.0		// Size of top level heading (points)
#define SIZE_HEADING_6	12.0		// Size of top level heading (points)
#define SIZE_TABLE	10.0		// Size of table text (points)

#define PAGE_WIDTH	mm2pt(210)	// Page width in points
#define PAGE_LENGTH	in2pt(11)	// Page length in points
#define PAGE_LEFT	in2pt(0.5)	// Left margin in points
#define PAGE_RIGHT	(PAGE_WIDTH - in2pt(0.5))
					// Right margin in points
#define PAGE_BOTTOM	in2pt(1.0)	// Bottom margin in points
#define PAGE_TOP	(PAGE_LENGTH - in2pt(1.0))
					// Top margin in points
#define PAGE_HEADER	(PAGE_LENGTH - in2pt(0.5))
					// Vertical position of header
#define PAGE_FOOTER	in2pt(0.5)	// Vertical position of footer

#define UNICODE_VALUE	true		// `true` for Unicode text, `false` for ISO-8859-1


//
// 'set_color()' - Set the stroke and fill color.
//

static void
set_color(docdata_t  *dd,		// I - Document data
          doccolor_t color)		// I - Document color
{
  switch (color)
  {
    case DOCCOLOR_BLACK :
        pdfioContentSetFillColorDeviceGray(dd->st, 0.0);
        pdfioContentSetStrokeColorDeviceGray(dd->st, 0.0);
        break;
    case DOCCOLOR_RED :
        pdfioContentSetFillColorDeviceRGB(dd->st, 0.6, 0.0, 0.0);
        pdfioContentSetStrokeColorDeviceRGB(dd->st, 0.6, 0.0, 0.0);
        break;
    case DOCCOLOR_GREEN :
        pdfioContentSetFillColorDeviceRGB(dd->st, 0.0, 0.6, 0.0);
        pdfioContentSetStrokeColorDeviceRGB(dd->st, 0.0, 0.6, 0.0);
        break;
    case DOCCOLOR_BLUE :
        pdfioContentSetFillColorDeviceRGB(dd->st, 0.0, 0.0, 0.8);
        pdfioContentSetStrokeColorDeviceRGB(dd->st, 0.0, 0.0, 0.8);
        break;
    case DOCCOLOR_GRAY :
        pdfioContentSetFillColorDeviceGray(dd->st, 0.333);
        pdfioContentSetStrokeColorDeviceGray(dd->st, 0.333);
        break;
  }

  dd->color = color;
}


//
// 'new_page()' - Start a new page.
//

static void
new_page(docdata_t *dd)			// I - Document data
{
  pdfio_dict_t	*page_dict;		// Page dictionary
  docfont_t	fontface;		// Current font face
  size_t	i;			// Looping var
  char		temp[32];		// Temporary string
  double	width;			// Width of fragment


  // Close the current page...
  pdfioStreamClose(dd->st);

  // Prep the new page...
  page_dict = pdfioDictCreate(dd->pdf);

  pdfioDictSetRect(page_dict, "MediaBox", &dd->media_box);
//  pdfioDictSetRect(page_dict, "CropBox", &dd->crop_box);
  pdfioDictSetRect(page_dict, "ArtBox", &dd->art_box);

  for (fontface = DOCFONT_REGULAR; fontface < DOCFONT_MAX; fontface ++)
    pdfioPageDictAddFont(page_dict, docfont_names[fontface], dd->fonts[fontface]);

  for (i = 0; i < dd->num_images; i ++)
  {
    snprintf(temp, sizeof(temp), "I%u", (unsigned)i);
    pdfioPageDictAddImage(page_dict, temp, dd->images[i].obj);
  }

  dd->st = pdfioFileCreatePage(dd->pdf, page_dict);

  // Add header/footer text
  set_color(dd, DOCCOLOR_GRAY);
  pdfioContentSetTextFont(dd->st, docfont_names[DOCFONT_REGULAR], SIZE_HEADFOOT);

  if (pdfioFileGetNumPages(dd->pdf) > 1 && dd->title)
  {
    // Show title in header...
    width = pdfioContentTextMeasure(dd->fonts[DOCFONT_REGULAR], dd->title, SIZE_HEADFOOT);

    pdfioContentTextBegin(dd->st);
    pdfioContentTextMoveTo(dd->st, dd->crop_box.x1 + 0.5 * (dd->crop_box.x2 - dd->crop_box.x1 - width), dd->crop_box.y2 - SIZE_HEADFOOT);
    pdfioContentTextShow(dd->st, UNICODE_VALUE, dd->title);
    pdfioContentTextEnd(dd->st);

    pdfioContentPathMoveTo(dd->st, dd->crop_box.x1, dd->crop_box.y2 - 2 * SIZE_HEADFOOT * LINE_HEIGHT + SIZE_HEADFOOT);
    pdfioContentPathLineTo(dd->st, dd->crop_box.x2, dd->crop_box.y2 - 2 * SIZE_HEADFOOT * LINE_HEIGHT + SIZE_HEADFOOT);
    pdfioContentStroke(dd->st);
  }

  // Show page number and current heading...
  pdfioContentPathMoveTo(dd->st, dd->crop_box.x1, dd->crop_box.y1 + SIZE_HEADFOOT * LINE_HEIGHT);
  pdfioContentPathLineTo(dd->st, dd->crop_box.x2, dd->crop_box.y1 + SIZE_HEADFOOT * LINE_HEIGHT);
  pdfioContentStroke(dd->st);

  pdfioContentTextBegin(dd->st);
  snprintf(temp, sizeof(temp), "%u", (unsigned)pdfioFileGetNumPages(dd->pdf));
  if (pdfioFileGetNumPages(dd->pdf) & 1)
  {
    // Page number on right...
    width = pdfioContentTextMeasure(dd->fonts[DOCFONT_REGULAR], temp, SIZE_HEADFOOT);
    pdfioContentTextMoveTo(dd->st, dd->crop_box.x2 - width, dd->crop_box.y1);
  }
  else
  {
    // Page number on left...
    pdfioContentTextMoveTo(dd->st, dd->crop_box.x1, dd->crop_box.y1);
  }

  pdfioContentTextShow(dd->st, UNICODE_VALUE, temp);
  pdfioContentTextEnd(dd->st);

  if (dd->heading)
  {
    pdfioContentTextBegin(dd->st);

    if (pdfioFileGetNumPages(dd->pdf) & 1)
    {
      // Current heading on left...
      pdfioContentTextMoveTo(dd->st, dd->crop_box.x1, dd->crop_box.y1);
    }
    else
    {
      width = pdfioContentTextMeasure(dd->fonts[DOCFONT_REGULAR], dd->heading, SIZE_HEADFOOT);
      pdfioContentTextMoveTo(dd->st, dd->crop_box.x2 - width, dd->crop_box.y1);
    }

    pdfioContentTextShow(dd->st, UNICODE_VALUE, dd->heading);
    pdfioContentTextEnd(dd->st);
  }

  // The rest of the text will be full black...
  set_color(dd, DOCCOLOR_BLACK);

  dd->y = dd->art_box.y2;
}


//
// 'format_block()' - Format a block of text
//

static void
format_block(docdata_t  *dd,		// I - Document data
             mmd_t      *block,		// I - Block to format
             docfont_t  fontface,	// I - Default font
             double     fontsize,	// I - Size of font
             double     left,		// I - Left margin
             double     right,		// I - Right margin
             const char *leader)	// I - Leader text on the first line
{
  mmd_type_t	blocktype;		// Block type
  mmd_t		*current,		// Current node
		*next;			// Next node
  mmd_type_t	curtype;		// Current node type
  const char	*curtext,		// Current text
		*cururl;		// Current URL, if any
  bool		curws;			// Current whitespace
  docfont_t	curface,		// Current font face
		prevface;		// Previous font face
  double	x, y;			// Current position
  double	width,			// Width of current fragment
		lwidth,			// Leader width
		wswidth;		// Width of whitespace
  doccolor_t	color;			// Color of text


  blocktype = mmdGetType(block);

  if (!dd->st)
    new_page(dd);

  if ((y = dd->y - 2.0 * fontsize * LINE_HEIGHT) < dd->art_box.y1)
  {
    new_page(dd);
    y = dd->y - fontsize;
  }

  if (leader)
  {
    // Add leader text on first line...
    pdfioContentSetTextFont(dd->st, docfont_names[prevface = fontface], fontsize);

    lwidth = pdfioContentTextMeasure(dd->fonts[fontface], leader, fontsize);

    pdfioContentTextBegin(dd->st);
    pdfioContentTextMoveTo(dd->st, left - lwidth, y);
    pdfioContentTextShow(dd->st, UNICODE_VALUE, leader);
  }
  else
  {
    // No leader text...
    prevface = DOCFONT_MAX;
    lwidth   = 0.0;
  }

  for (current = mmdGetFirstChild(block), x = left; current; current = next)
  {
    // Get information about the current node...
    curtype = mmdGetType(current);
    curtext = mmdGetText(current);
    cururl  = mmdGetURL(current);
    curws   = mmdGetWhitespace(current);

//    fprintf(stderr, "current=%p, curtype=%d, curtext=\"%s\", cururl=\"%s\", curws=%s\n", (void *)current, curtype, curtext, cururl, curws ? "true" : "false");

    // Figure out the next node under this block...
    if ((next = mmdGetFirstChild(current)) == NULL)
    {
      if ((next = mmdGetNextSibling(current)) == NULL)
      {
        mmd_t *parent;			// Parent node

        if ((parent = mmdGetParent(current)) != block)
        {
          while ((next = mmdGetNextSibling(parent)) == NULL)
          {
            if ((parent = mmdGetParent(parent)) == block)
              break;
          }
        }
      }
    }

    // Process the node...
    if (!curtext)
      continue;

    if (curtype == MMD_TYPE_EMPHASIZED_TEXT)
      curface = DOCFONT_ITALIC;
    else if (curtype == MMD_TYPE_STRONG_TEXT)
      curface = DOCFONT_BOLD;
    else  if (curtype == MMD_TYPE_CODE_TEXT)
      curface = DOCFONT_MONOSPACE;
    else
      curface = fontface;

    if (curtype == MMD_TYPE_CODE_TEXT)
      color = DOCCOLOR_RED;
    else if (curtype == MMD_TYPE_LINKED_TEXT)
      color = DOCCOLOR_BLUE;
    else
      color = DOCCOLOR_BLACK;

    width = pdfioContentTextMeasure(dd->fonts[curface], curtext, fontsize);
    if (curws)
      wswidth = pdfioContentTextMeasure(dd->fonts[curface], " ", fontsize);
    else
      wswidth = 0.0;

    if (x > left && (x + width + wswidth) >= right)
    {
      // New line...
      x = left;
      y -= fontsize * LINE_HEIGHT;

      if (y < dd->art_box.y1)
      {
        // New page...
	if (prevface != DOCFONT_MAX)
	{
	  pdfioContentTextEnd(dd->st);
	  prevface = DOCFONT_MAX;
	}

        new_page(dd);

        y = dd->y - fontsize * LINE_HEIGHT;
      }
      else
      {
	pdfioContentTextMoveTo(dd->st, lwidth, -fontsize * LINE_HEIGHT);
	lwidth = 0.0;
      }
    }

    if (curface != prevface)
    {
      if (prevface == DOCFONT_MAX)
      {
	pdfioContentTextBegin(dd->st);
	pdfioContentTextMoveTo(dd->st, x, y);
      }

      pdfioContentSetTextFont(dd->st, docfont_names[prevface = curface], fontsize);
    }

    if (color != dd->color)
      set_color(dd, color);

    if (x > left && curws)
    {
      pdfioContentTextShowf(dd->st, UNICODE_VALUE, " %s", curtext);
      x += width + wswidth;
    }
    else
    {
      pdfioContentTextShow(dd->st, UNICODE_VALUE, curtext);
      x += width;
    }

    if (blocktype == MMD_TYPE_CODE_BLOCK)
    {
      // Force a new line...
      x = left;
      y -= fontsize * LINE_HEIGHT;

      if (y < dd->art_box.y1)
      {
        // New page...
	if (prevface != DOCFONT_MAX)
	{
	  pdfioContentTextEnd(dd->st);
	  prevface = DOCFONT_MAX;
	}

        new_page(dd);

        y = dd->y - fontsize * LINE_HEIGHT;
      }
      else
      {
	pdfioContentTextMoveTo(dd->st, lwidth, -fontsize * LINE_HEIGHT);
	lwidth = 0.0;
      }
    }
  }

  // End the current text block and save out position on the page...
  if (prevface != DOCFONT_MAX)
    pdfioContentTextEnd(dd->st);

  dd->y = y;
}


//
// 'format_doc()' - Format a document.
//

static void
format_doc(docdata_t *dd,		// I - Document data
           mmd_t     *doc,		// I - Document node to format
           double    left,		// I - Left margin
           double    right)		// I - Right margin
{
  int		i;			// Child number
  mmd_type_t	doctype;		// Document node type
  mmd_t		*current;		// Current node
  mmd_type_t	curtype;		// Current node type
  char		leader[32];		// Leader
  static const double heading_sizes[] =	// Heading font sizes
  {
    SIZE_HEADING_1,
    SIZE_HEADING_2,
    SIZE_HEADING_3,
    SIZE_HEADING_4,
    SIZE_HEADING_5,
    SIZE_HEADING_6
  };


  doctype = mmdGetType(doc);

  for (i = 1, current = mmdGetFirstChild(doc); current; i ++, current = mmdGetNextSibling(current))
  {
    switch (curtype = mmdGetType(current))
    {
      default :
          break;

      case MMD_TYPE_BLOCK_QUOTE :
          format_doc(dd, current, left + 36.0, right - 36.0);
          break;

      case MMD_TYPE_ORDERED_LIST :
      case MMD_TYPE_UNORDERED_LIST :
          format_doc(dd, current, left + 36.0, right);
          break;

      case MMD_TYPE_LIST_ITEM :
          if (doctype == MMD_TYPE_ORDERED_LIST)
          {
            snprintf(leader, sizeof(leader), "%d. ", i);
            format_block(dd, current, DOCFONT_REGULAR, SIZE_BODY, left, right, leader);
          }
          else
          {
            format_block(dd, current, DOCFONT_REGULAR, SIZE_BODY, left, right, /*leader*/"• ");
          }
          break;

      case MMD_TYPE_HEADING_1 :
      case MMD_TYPE_HEADING_2 :
      case MMD_TYPE_HEADING_3 :
      case MMD_TYPE_HEADING_4 :
      case MMD_TYPE_HEADING_5 :
      case MMD_TYPE_HEADING_6 :
          if (dd->heading)
            free(dd->heading);

          dd->heading = mmdCopyAllText(current);

          format_block(dd, current, DOCFONT_BOLD, heading_sizes[curtype - MMD_TYPE_HEADING_1], left, right, /*leader*/NULL);
          break;

      case MMD_TYPE_PARAGRAPH :
          format_block(dd, current, DOCFONT_REGULAR, SIZE_BODY, left, right, /*leader*/NULL);
          break;

      case MMD_TYPE_CODE_BLOCK :
          format_block(dd, current, DOCFONT_MONOSPACE, SIZE_BODY, left + 36.0, right, /*leader*/NULL);
          break;
    }
  }
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
// 'main()' - Convert markdown to PDF.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  docdata_t	dd;			// Document data
  docfont_t	fontface;		// Current font
  mmd_t		*doc;			// Markdown document
  const char	*value;			// Metadata value


  // Get the markdown file from the command-line...
  if (argc != 2)
  {
    fputs("Usage: md2pdf FILENANE.md >FILENAME.pdf\n", stderr);
    return (1);
  }

  if ((doc = mmdLoad(/*root*/NULL, argv[1])) == NULL)
    return (1);

  // Initialize the document data
  memset(&dd, 0, sizeof(dd));

  dd.media_box.x2 = PAGE_WIDTH;
  dd.media_box.y2 = PAGE_LENGTH;

  dd.crop_box.x1 = PAGE_LEFT;
  dd.crop_box.y1 = PAGE_FOOTER;
  dd.crop_box.x2 = PAGE_RIGHT;
  dd.crop_box.y2 = PAGE_HEADER;

  dd.art_box.x1  = PAGE_LEFT;
  dd.art_box.y1  = PAGE_BOTTOM;
  dd.art_box.x2  = PAGE_RIGHT;
  dd.art_box.y2  = PAGE_TOP;

  dd.title       = mmdGetMetadata(doc, "title");

  // Output a PDF file to the standard output...
#ifdef _WIN32
  setmode(1, O_BINARY);			// Force binary output on Windows
#endif // _WIN32

  if ((dd.pdf = pdfioFileCreateOutput(output_cb, /*output_cbdata*/NULL, /*version*/NULL, /*media_box*/NULL, /*crop_box*/NULL, /*error_cb*/NULL, /*error_data*/NULL)) == NULL)
    return (1);

  if ((value = mmdGetMetadata(doc, "author")) != NULL)
    pdfioFileSetAuthor(dd.pdf, value);

  if ((value = mmdGetMetadata(doc, "keywords")) != NULL)
    pdfioFileSetKeywords(dd.pdf, value);

  if ((value = mmdGetMetadata(doc, "subject")) != NULL)
    pdfioFileSetSubject(dd.pdf, value);
  else if ((value = mmdGetMetadata(doc, "copyright")) != NULL)
    pdfioFileSetSubject(dd.pdf, value);

  if (dd.title)
    pdfioFileSetTitle(dd.pdf, dd.title);

  // Add fonts...
  for (fontface = DOCFONT_REGULAR; fontface < DOCFONT_MAX; fontface ++)
  {
    if ((dd.fonts[fontface] = pdfioFileCreateFontObjFromFile(dd.pdf, docfont_filenames[fontface], UNICODE_VALUE)) == NULL)
      return (1);
  }

  // Parse the markdown document...
  format_doc(&dd, doc, dd.art_box.x1, dd.art_box.x2);

  // Close the PDF and return...
  pdfioStreamClose(dd.st);
  pdfioFileClose(dd.pdf);

  mmdFree(doc);

  return (0);
}
