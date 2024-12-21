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
//   ./md2pdf FILENAME.md FILENAME.pdf
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
#include <ctype.h>
#include <math.h>
#ifdef _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif // _WIN32
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
  DOCCOLOR_ORANGE,			// #CC0
  DOCCOLOR_BLUE,			// #00C
  DOCCOLOR_LTGRAY,			// #EEE
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

typedef struct doclink_s		// Document link info
{
  const char	*url;			// Target URL
  pdfio_rect_t	box;			// Link box
} doclink_t;

#define DOCLINK_MAX	1000		// Maximum number of links per page

typedef struct docaction_s		// Document action info
{
  const char	*target;		// Target name
  pdfio_obj_t	*obj;			// Link object
} docaction_t;

#define DOCACTION_MAX	10000		// Maximum number of actions per document

typedef struct doctarget_s		// Document target info
{
  char		name[128];		// Target name
  size_t	page;			// Target page
  double	y;			// Target page position
} doctarget_t;

#define DOCTARGET_MAX	1000		// Maximum number of targets per document

typedef struct doctoc_s			// Document table-of-contents entry
{
  int		level;			// Level
  int		count;			// Total number of child entries
  pdfio_obj_t	*obj;			// Dictionary object
  pdfio_dict_t	*dict;			// Dictionary value
} doctoc_t;

#define DOCTOC_MAX	1000		// Maximum number of TOC entries

typedef struct docdata_s		// Document formatting data
{
  // State for the whole document
  pdfio_file_t	*pdf;			// PDF file
  pdfio_rect_t	media_box;		// Media (page) box
  pdfio_rect_t	crop_box;		// Crop box (for margins)
  pdfio_rect_t	art_box;		// Art box (for markdown content)
  pdfio_obj_t	*fonts[DOCFONT_MAX];	// Embedded fonts
  double	font_space;		// Unit width of a space
  size_t	num_images;		// Number of embedded images
  docimage_t	images[DOCIMAGE_MAX];	// Embedded images
  const char	*title;			// Document title
  char		*heading;		// Current document heading
  size_t	num_actions;		// Number of actions for this document
  docaction_t	actions[DOCACTION_MAX];	// Actions for this document
  size_t	num_targets;		// Number of targets for this document
  doctarget_t	targets[DOCTARGET_MAX];	// Targets for this document
  size_t	num_toc;		// Number of table-of-contents entries
  doctoc_t	toc[DOCTOC_MAX];	// Table-of-contents entries

  // State for the current page
  pdfio_stream_t *st;			// Current page stream
  double	y;			// Current position on page
  docfont_t	font;			// Current font
  double	fsize;			// Current font size
  doccolor_t	color;			// Current color
  pdfio_array_t	*annots_array;		// Annotations array (for links)
  pdfio_obj_t	*annots_obj;		// Annotations object (for links)
  size_t	num_links;		// Number of links for this page
  doclink_t	links[DOCLINK_MAX];	// Links for this page
} docdata_t;

typedef struct linefrag_s		// Line fragment
{
  mmd_type_t	type;			// Type of fragment
  double	x,			// X position of item
		width,			// Width of item
		height;			// Height of item
  size_t	imagenum;		// Image number
  const char	*text;			// Text string
  const char	*url;			// Link URL string
  bool		ws;			// Whitespace before text?
  docfont_t	font;			// Text font
  doccolor_t	color;			// Text color
} linefrag_t;

#define LINEFRAG_MAX	200		// Maximum number of fragments on a line

typedef struct tablecol_s		// Table column data
{
  double	min_width,		// Minimum required width of column
		max_width,		// Maximum required width of column
		width,			// Width of column
		left,			// Left edge
		right;			// Right edge
} tablecol_t;

#define TABLECOL_MAX	20		// Maximum number of table columns

typedef struct tablerow_s		// Table row
{
  mmd_t		*cells[TABLECOL_MAX];	// Cells in row
  double	height;			// Row height
} tablerow_t;

#define TABLEROW_MAX	1000		// Maximum number of table rows


//
// Macros...
//

#define in2pt(in) (in * 72.0)
#define mm2pt(mm) (mm * 72.0 / 25.4)


//
// Constants...
//

#define USE_TRUETYPE	0		// Set to 1 to use Roboto TrueType fonts

#if USE_TRUETYPE
#  define UNICODE_VALUE	true		// `true` for Unicode text, `false` for ISO-8859-1
static const char * const docfont_filenames[] =
{
  "Roboto-Regular.ttf",
  "Roboto-Bold.ttf",
  "Roboto-Italic.ttf",
  "RobotoMono-Regular.ttf"
};
#else
#  define UNICODE_VALUE	false		// `true` for Unicode text, `false` for ISO-8859-1
static const char * const docfont_filenames[] =
{
  "Helvetica",
  "Helvetica-Bold",
  "Helvetica-Oblique",
  "Courier"
};
#endif // USE_TRUETYPE

static const char * const docfont_names[] =
{
  "FR",
  "FB",
  "FI",
  "FM"
};

#define CODE_PADDING	4.5		// Padding for code blocks

#define IMAGE_PPI	100.0		// Pixels per inch for images

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

#define TABLE_PADDING	4.5		// Table padding value


//
// Functions...
//

static void	add_images(docdata_t *dd, mmd_t *doc);
static void	add_links(docdata_t *dd);
static pdfio_obj_t *find_image(docdata_t  *dd, const char *url, size_t *imagenum);
static void	format_block(docdata_t *dd, mmd_t *block, docfont_t deffont, double fsize, double left, double right, const char *leader);
static void	format_code(docdata_t *dd, mmd_t *block, double left, double right);
static void	format_doc(docdata_t *dd, mmd_t *doc, docfont_t deffont, double left, double right);
static void	format_table(docdata_t *dd, mmd_t *table, double left, double right);
static void	make_target_name(char *dst, const char *src, size_t dstsize);
static double	measure_cell(docdata_t *dd, mmd_t *cell, tablecol_t *col);
static mmd_t	*mmd_walk_next(mmd_t *top, mmd_t *node);
static void	new_page(docdata_t *dd);
static ssize_t	output_cb(void *output_cbdata, const void *buffer, size_t bytes);
static void	render_line(docdata_t *dd, double margin_left, double margin_top, double need_bottom, double lineheight, size_t num_frags, linefrag_t *frags);
static void	render_row(docdata_t *dd, size_t num_cols, tablecol_t *cols, tablerow_t *row);
static void	set_color(docdata_t *dd, doccolor_t color);
static void	set_font(docdata_t *dd, docfont_t font, double fsize);
static void	write_actions(docdata_t *dd);
static void	write_toc(docdata_t *dd);


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


  setbuf(stderr, NULL);

  // Get the markdown file from the command-line...
  if (argc < 2 || argc > 3)
  {
    fputs("Usage: md2pdf FILENANE.md [FILENAME.pdf]\n", stderr);
    fputs("       md2pdf FILENANE.md >FILENAME.pdf\n", stderr);
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

  if ((dd.title = mmdGetMetadata(doc, "title")) == NULL)
    dd.art_box.y2 = PAGE_HEADER;	// No header if there is no title

  if (argc == 2)
  {
    // Output a PDF file to the standard output...
#ifdef _WIN32
    setmode(1, O_BINARY);		// Force binary output on Windows
#endif // _WIN32

    if ((dd.pdf = pdfioFileCreateOutput(output_cb, /*output_cbdata*/NULL, /*version*/NULL, /*media_box*/NULL, /*crop_box*/NULL, /*error_cb*/NULL, /*error_data*/NULL)) == NULL)
      return (1);
  }
  else if ((dd.pdf = pdfioFileCreate(argv[2], /*version*/NULL, /*media_box*/NULL, /*crop_box*/NULL, /*error_cb*/NULL, /*error_data*/NULL)) == NULL)
  {
    return (1);
  }

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
#if USE_TRUETYPE
    if ((dd.fonts[fontface] = pdfioFileCreateFontObjFromFile(dd.pdf, docfont_filenames[fontface], UNICODE_VALUE)) == NULL)
      return (1);
#else
    if ((dd.fonts[fontface] = pdfioFileCreateFontObjFromBase(dd.pdf, docfont_filenames[fontface])) == NULL)
      return (1);
#endif // USE_TRUETYPE
  }

  dd.font_space = pdfioContentTextMeasure(dd.fonts[DOCFONT_REGULAR], " ", 1.0);

  // Add images...
  add_images(&dd, doc);

  // Parse the markdown document...
  format_doc(&dd, doc, DOCFONT_REGULAR, dd.art_box.x1, dd.art_box.x2);

  // Close the PDF and return...
  if (dd.st)
  {
    pdfioStreamClose(dd.st);
    add_links(&dd);
  }

  write_actions(&dd);

  if (dd.num_toc > 0)
    write_toc(&dd);

  pdfioFileClose(dd.pdf);

  mmdFree(doc);

  return (0);
}


//
// 'add_images()' - Scan the markdown document for images.
//

static void
add_images(docdata_t *dd,		// I - Document data
           mmd_t     *doc)		// I - Markdown document
{
  mmd_t	*current,			// Current node
	*next;				// Next node


  // Scan the entire document for images...
  for (current = mmdGetFirstChild(doc); current; current = next)
  {
    // Get next node
    next = mmd_walk_next(doc, current);

    // Look for image nodes...
    if (mmdGetType(current) == MMD_TYPE_IMAGE)
    {
      const char	*url,		// URL for image
			*ext;		// Extension

      url = mmdGetURL(current);
      ext = strrchr(url, '.');

      if (!access(url, 0) && ext && (!strcmp(ext, ".png") || !strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg")))
      {
        // Local JPEG or PNG file, so add it if we haven't already...
        size_t		i;		// Looping var

        for (i = 0; i < dd->num_images; i ++)
        {
          if (!strcmp(dd->images[i].url, url))
            break;
        }

        if (i >= dd->num_images && dd->num_images < DOCIMAGE_MAX)
        {
          dd->images[i].url = url;
          if ((dd->images[i].obj = pdfioFileCreateImageObjFromFile(dd->pdf, url, false)) != NULL)
	    dd->num_images ++;
        }
      }
    }
  }
}


//
// 'add_links()' - Add the page links, if any.
//

static void
add_links(docdata_t *dd)		// I - Document data
{
  size_t	i;			// Looping var
  doclink_t	*l;			// Current link
  pdfio_dict_t	*dict;			// Object dictionary
  pdfio_obj_t	*aobj,			// Annotation object
		*lobj;			// Link object
  pdfio_array_t	*border;		// Border values


  // Add any links we have...
  for (i = 0, l = dd->links; i < dd->num_links; i ++, l ++)
  {
    if (l->url[0] == '#')
    {
      // No remote action...
      aobj = NULL;
    }
    else
    {
      // Create the link action (remote URL)
      dict = pdfioDictCreate(dd->pdf);
      pdfioDictSetName(dict, "S", "URI");
      pdfioDictSetString(dict, "URI", pdfioStringCreate(dd->pdf, l->url));

      aobj = pdfioFileCreateObj(dd->pdf, dict);
      pdfioObjClose(aobj);
    }

    // Create the annotation object pointing to the action...
    dict = pdfioDictCreate(dd->pdf);
    pdfioDictSetName(dict, "Subtype", "Link");
    pdfioDictSetRect(dict, "Rect", &l->box);
    border = pdfioArrayCreate(dd->pdf);
    pdfioArrayAppendNumber(border, 0.0);
    pdfioArrayAppendNumber(border, 0.0);
    pdfioArrayAppendNumber(border, 0.0);
    pdfioDictSetArray(dict, "Border", border);

    lobj = pdfioFileCreateObj(dd->pdf, dict);

    if (l->url[0] == '#' && dd->num_actions < DOCACTION_MAX)
    {
      // Save this link action for later...
      docaction_t *a = dd->actions + dd->num_actions;
					// New action

      a->target = l->url + 1;
      a->obj    = lobj;

      dd->num_actions ++;
    }
    else if (aobj)
    {
      // Close out this link since we have a remote URL...
      pdfioDictSetObj(dict, "A", aobj);
      pdfioObjClose(lobj);
    }
    else
    {
      // Nothing that can be done for this one...
      pdfioObjClose(lobj);
    }

    pdfioArrayAppendObj(dd->annots_array, lobj);
  }

  // Close the Annots array object...
  pdfioObjClose(dd->annots_obj);

  // Reset links...
  dd->annots_array = NULL;
  dd->annots_obj   = NULL;
  dd->num_links    = 0;
}


//
// 'find_image()' - Find an image in the document.
//

static pdfio_obj_t *			// O - Image object or `NULL` if none
find_image(docdata_t  *dd,		// I - Document data
           const char *url,		// I - Image URL
           size_t     *imagenum)	// O - Image number
{
  size_t	i;			// Looping var


  // Look for a matching URL...
  for (i = 0; i < dd->num_images; i ++)
  {
    if (!strcmp(dd->images[i].url, url))
    {
      if (imagenum)
        *imagenum = i;

      return (dd->images[i].obj);
    }
  }

  // Not found, return NULL...
  if (imagenum)
    *imagenum = 0;

  return (NULL);
}


//
// 'format_block()' - Format a block of text
//

static void
format_block(docdata_t  *dd,		// I - Document data
             mmd_t      *block,		// I - Block to format
             docfont_t  deffont,	// I - Default font
             double     fsize,		// I - Size of font
             double     left,		// I - Left margin
             double     right,		// I - Right margin
             const char *leader)	// I - Leader text on the first line
{
  mmd_type_t	blocktype;		// Block type
  mmd_t		*current,		// Current node
		*next;			// Next node
  size_t	num_frags;		// Number of line fragments
  linefrag_t	frags[LINEFRAG_MAX],	// Line fragments
		*frag;			// Current fragment
  mmd_type_t	type;			// Current node type
  const char	*text,			// Current text
		*url;			// Current URL, if any
  bool		ws;			// Current whitespace
  pdfio_obj_t	*image;			// Current image, if any
  size_t	imagenum;		// Current image number
  doccolor_t	color = DOCCOLOR_BLACK;	// Current text color
  docfont_t	font = deffont;		// Current text font
  double	x,			// Current position
		width,			// Width of current fragment
		wswidth,		// Width of whitespace
		margin_left,		// Left margin
		margin_top,		// Top margin
		need_bottom,		// Space needed after this block
		height,			// Height of current fragment
		lineheight;		// Height of current line


  blocktype  = mmdGetType(block);

  if ((blocktype >= MMD_TYPE_TABLE_HEADER_CELL && blocktype <= MMD_TYPE_TABLE_BODY_CELL_RIGHT) || blocktype == MMD_TYPE_LIST_ITEM)
    margin_top = 0.0;
  else
    margin_top = fsize * LINE_HEIGHT;

  if (mmdGetNextSibling(block))
    need_bottom = 3.0 * SIZE_BODY * LINE_HEIGHT;
  else
    need_bottom = 0.0;

  if (leader)
  {
    // Add leader text on first line...
    frags[0].type     = MMD_TYPE_NORMAL_TEXT;
    frags[0].width    = pdfioContentTextMeasure(dd->fonts[deffont], leader, fsize);
    frags[0].height   = fsize;
    frags[0].x        = left - frags[0].width;
    frags[0].imagenum = 0;
    frags[0].text     = leader;
    frags[0].url      = NULL;
    frags[0].ws       = false;
    frags[0].font     = deffont;
    frags[0].color    = DOCCOLOR_BLACK;

    num_frags  = 1;
    lineheight = fsize * LINE_HEIGHT;
  }
  else
  {
    // No leader text...
    num_frags  = 0;
    lineheight = 0.0;
  }

  frag = frags + num_frags;

  // Loop through the block and render lines...
  for (current = mmdGetFirstChild(block), x = left; current; current = next)
  {
    // Get information about the current node...
    type     = mmdGetType(current);
    text     = mmdGetText(current);
    image    = NULL;
    imagenum = 0;
    url      = mmdGetURL(current);
    ws       = mmdGetWhitespace(current);
    wswidth  = ws ? dd->font_space * fsize : 0.0;
    next     = mmd_walk_next(block, current);

    // Process the node...
    if (type == MMD_TYPE_IMAGE && url)
    {
      // Embed an image
      if ((image = find_image(dd, url, &imagenum)) == NULL)
        continue;

      // Image - treat as 100dpi
      width  = 72.0 * pdfioImageGetWidth(image) / IMAGE_PPI;
      height = 72.0 * pdfioImageGetHeight(image) / IMAGE_PPI;
      text   = NULL;

      if (width > (right - left))
      {
        // Too wide, scale to width...
        width  = right - left;
        height = width * pdfioImageGetHeight(image) / pdfioImageGetWidth(image);
      }
      else if (height > (dd->art_box.y2 - dd->art_box.y1))
      {
        // Too tall, scale to height...
        height = dd->art_box.y2 - dd->art_box.y1;
        width  = height * pdfioImageGetWidth(image) / pdfioImageGetHeight(image);
      }
    }
    else if (type == MMD_TYPE_HARD_BREAK && num_frags > 0)
    {
      if (blocktype == MMD_TYPE_TABLE_HEADER_CELL || blocktype == MMD_TYPE_TABLE_BODY_CELL_CENTER)
        margin_left = 0.5 * (right - x);
      else if (blocktype == MMD_TYPE_TABLE_BODY_CELL_RIGHT)
        margin_left = right - x;
      else
        margin_left = 0.0;

      render_line(dd, margin_left, margin_top, need_bottom, lineheight, num_frags, frags);

      if (deffont == DOCFONT_ITALIC)
      {
        // Add an orange bar to the left of block quotes...
        set_color(dd, DOCCOLOR_ORANGE);
        pdfioContentSave(dd->st);
        pdfioContentSetLineWidth(dd->st, 3.0);
        pdfioContentPathMoveTo(dd->st, left - 6.0, dd->y - (LINE_HEIGHT - 1.0) * fsize);
        pdfioContentPathLineTo(dd->st, left - 6.0, dd->y + fsize);
        pdfioContentStroke(dd->st);
        pdfioContentRestore(dd->st);
      }

      num_frags   = 0;
      frag        = frags;
      x           = left;
      lineheight  = 0.0;
      margin_top  = 0.0;
      need_bottom = 0.0;

      continue;
    }
    else if (type == MMD_TYPE_CHECKBOX)
    {
      // Checkbox
      width = height = fsize;
    }
    else if (!text)
    {
      continue;
    }
    else
    {
      // Text fragment...
      if (type == MMD_TYPE_EMPHASIZED_TEXT)
	font = DOCFONT_ITALIC;
      else if (type == MMD_TYPE_STRONG_TEXT)
	font = DOCFONT_BOLD;
      else  if (type == MMD_TYPE_CODE_TEXT)
	font = DOCFONT_MONOSPACE;
      else
	font = deffont;

      if (type == MMD_TYPE_CODE_TEXT)
	color = DOCCOLOR_RED;
      else if (type == MMD_TYPE_LINKED_TEXT)
	color = DOCCOLOR_BLUE;
      else
	color = DOCCOLOR_BLACK;

      width  = pdfioContentTextMeasure(dd->fonts[font], text, fsize);
      height = fsize * LINE_HEIGHT;
    }

    // See if this node will fit on the current line...
    if ((num_frags > 0 && (x + width + wswidth) >= right) || num_frags == LINEFRAG_MAX)
    {
      // No, render this line and start over...
      if (blocktype == MMD_TYPE_TABLE_HEADER_CELL || blocktype == MMD_TYPE_TABLE_BODY_CELL_CENTER)
        margin_left = 0.5 * (right - x);
      else if (blocktype == MMD_TYPE_TABLE_BODY_CELL_RIGHT)
        margin_left = right - x;
      else
        margin_left = 0.0;

      render_line(dd, margin_left, margin_top, need_bottom, lineheight, num_frags, frags);

      if (deffont == DOCFONT_ITALIC)
      {
        // Add an orange bar to the left of block quotes...
        set_color(dd, DOCCOLOR_ORANGE);
        pdfioContentSave(dd->st);
        pdfioContentSetLineWidth(dd->st, 3.0);
        pdfioContentPathMoveTo(dd->st, left - 6.0, dd->y - (LINE_HEIGHT - 1.0) * fsize);
        pdfioContentPathLineTo(dd->st, left - 6.0, dd->y + fsize);
        pdfioContentStroke(dd->st);
        pdfioContentRestore(dd->st);
      }

      num_frags   = 0;
      frag        = frags;
      x           = left;
      lineheight  = 0.0;
      margin_top  = 0.0;
      need_bottom = 0.0;
    }

    // Add the current node to the fragment list
    if (num_frags == 0)
    {
      ws      = false;
      wswidth = 0.0;
    }

    frag->type       = type;
    frag->x          = x;
    frag->width      = width + wswidth;
    frag->height     = text ? fsize : height;
    frag->imagenum   = imagenum;
    frag->text       = text;
    frag->url        = url;
    frag->ws         = ws;
    frag->font       = font;
    frag->color      = color;

    num_frags ++;
    frag ++;
    x += width + wswidth;
    if (height > lineheight)
      lineheight = height;
  }

  if (num_frags > 0)
  {
    if (blocktype == MMD_TYPE_TABLE_HEADER_CELL || blocktype == MMD_TYPE_TABLE_BODY_CELL_CENTER)
      margin_left = 0.5 * (right - x);
    else if (blocktype == MMD_TYPE_TABLE_BODY_CELL_RIGHT)
      margin_left = right - x;
    else
      margin_left = 0.0;

    render_line(dd, margin_left, margin_top, need_bottom, lineheight, num_frags, frags);

    if (deffont == DOCFONT_ITALIC)
    {
      // Add an orange bar to the left of block quotes...
      set_color(dd, DOCCOLOR_ORANGE);
      pdfioContentSave(dd->st);
      pdfioContentSetLineWidth(dd->st, 3.0);
      pdfioContentPathMoveTo(dd->st, left - 6.0, dd->y - (LINE_HEIGHT - 1.0) * fsize);
      pdfioContentPathLineTo(dd->st, left - 6.0, dd->y + fsize);
      pdfioContentStroke(dd->st);
      pdfioContentRestore(dd->st);
    }
  }
}


//
// 'format_code()' - Format a code block...
//

static void
format_code(docdata_t *dd,		// I - Document data
            mmd_t     *block,		// I - Code block
            double    left,		// I - Left margin
            double    right)		// I - Right margin
{
  mmd_t		*code;			// Current code block
  double	lineheight,		// Line height
		margin_top;		// Top margin


  // Compute line height and initial top margin...
  lineheight = SIZE_CODEBLOCK * LINE_HEIGHT;
  margin_top = lineheight;

  // Start a new page as needed...
  if (!dd->st)
  {
    new_page(dd);

    margin_top = 0.0;
  }

  dd->y -= lineheight + margin_top + CODE_PADDING;

  if ((dd->y - lineheight) < dd->art_box.y1)
  {
    new_page(dd);

    dd->y -= lineheight + CODE_PADDING;
  }

  // Draw the top padding...
  set_color(dd, DOCCOLOR_LTGRAY);
  pdfioContentPathRect(dd->st, left - CODE_PADDING, dd->y + SIZE_CODEBLOCK, right - left + 2.0 * CODE_PADDING, CODE_PADDING);
  pdfioContentFillAndStroke(dd->st, false);

  // Start a code text block...
  set_font(dd, DOCFONT_MONOSPACE, SIZE_CODEBLOCK);
  pdfioContentTextBegin(dd->st);
  pdfioContentTextMoveTo(dd->st, left, dd->y);

  for (code = mmdGetFirstChild(block); code; code = mmdGetNextSibling(code))
  {
    set_color(dd, DOCCOLOR_LTGRAY);
    pdfioContentPathRect(dd->st, left - CODE_PADDING, dd->y - (LINE_HEIGHT - 1.0) * SIZE_CODEBLOCK, right - left + 2.0 * CODE_PADDING, lineheight);
    pdfioContentFillAndStroke(dd->st, false);

    set_color(dd, DOCCOLOR_RED);
    pdfioContentTextShow(dd->st, UNICODE_VALUE, mmdGetText(code));
    dd->y -= lineheight;

    if (dd->y < dd->art_box.y1)
    {
      // End the current text block...
      pdfioContentTextEnd(dd->st);

      // Start a new page...
      new_page(dd);
      set_font(dd, DOCFONT_MONOSPACE, SIZE_CODEBLOCK);

      dd->y -= lineheight;

      pdfioContentTextBegin(dd->st);
      pdfioContentTextMoveTo(dd->st, left, dd->y);
    }
  }

  // End the current text block...
  pdfioContentTextEnd(dd->st);
  dd->y += lineheight;

  // Draw the bottom padding...
  set_color(dd, DOCCOLOR_LTGRAY);
  pdfioContentPathRect(dd->st, left - CODE_PADDING, dd->y - CODE_PADDING - (LINE_HEIGHT - 1.0) * SIZE_CODEBLOCK, right - left + 2.0 * CODE_PADDING, CODE_PADDING);
  pdfioContentFillAndStroke(dd->st, false);
}


//
// 'format_doc()' - Format a document.
//

static void
format_doc(docdata_t *dd,		// I - Document data
           mmd_t     *doc,		// I - Document node to format
           docfont_t deffont,		// I - Default font
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

      case MMD_TYPE_THEMATIC_BREAK :
          // Force a page break
          dd->y = dd->art_box.y1;
          break;

      case MMD_TYPE_BLOCK_QUOTE :
          format_doc(dd, current, DOCFONT_ITALIC, left + 36.0, right - 36.0);
          break;

      case MMD_TYPE_ORDERED_LIST :
      case MMD_TYPE_UNORDERED_LIST :
          if (dd->st)
            dd->y -= SIZE_BODY * LINE_HEIGHT;

          format_doc(dd, current, deffont, left + 36.0, right);
          break;

      case MMD_TYPE_LIST_ITEM :
          if (doctype == MMD_TYPE_ORDERED_LIST)
          {
            snprintf(leader, sizeof(leader), "%d. ", i);
            format_block(dd, current, deffont, SIZE_BODY, left, right, leader);
          }
          else
          {
            format_block(dd, current, deffont, SIZE_BODY, left, right, /*leader*/"• ");
          }
          break;

      case MMD_TYPE_HEADING_1 :
      case MMD_TYPE_HEADING_2 :
      case MMD_TYPE_HEADING_3 :
      case MMD_TYPE_HEADING_4 :
      case MMD_TYPE_HEADING_5 :
      case MMD_TYPE_HEADING_6 :
          free(dd->heading);

          dd->heading = mmdCopyAllText(current);

          format_block(dd, current, DOCFONT_BOLD, heading_sizes[curtype - MMD_TYPE_HEADING_1], left, right, /*leader*/NULL);

          if (dd->num_toc < DOCTOC_MAX)
          {
            doctoc_t *t = dd->toc + dd->num_toc;
					// New TOC
	    pdfio_array_t *dest;	// Destination array

            t->level = curtype - MMD_TYPE_HEADING_1;
            t->dict  = pdfioDictCreate(dd->pdf);
            t->obj   = pdfioFileCreateObj(dd->pdf, t->dict);
            dest     = pdfioArrayCreate(dd->pdf);

	    pdfioArrayAppendObj(dest, pdfioFileGetPage(dd->pdf, pdfioFileGetNumPages(dd->pdf) - 1));
	    pdfioArrayAppendName(dest, "XYZ");
	    pdfioArrayAppendNumber(dest, PAGE_LEFT);
	    pdfioArrayAppendNumber(dest, dd->y + heading_sizes[curtype - MMD_TYPE_HEADING_1] * LINE_HEIGHT);
	    pdfioArrayAppendNumber(dest, 0.0);

	    pdfioDictSetArray(t->dict, "Dest", dest);
	    pdfioDictSetString(t->dict, "Title", pdfioStringCreate(dd->pdf, dd->heading));

            dd->num_toc ++;
          }

          if (dd->num_targets < DOCTARGET_MAX)
          {
            doctarget_t *t = dd->targets + dd->num_targets;
					// New target

            make_target_name(t->name, dd->heading, sizeof(t->name));
            t->page = pdfioFileGetNumPages(dd->pdf) - 1;
            t->y    = dd->y + heading_sizes[curtype - MMD_TYPE_HEADING_1] * LINE_HEIGHT;

            dd->num_targets ++;
          }
          break;

      case MMD_TYPE_PARAGRAPH :
          format_block(dd, current, deffont, SIZE_BODY, left, right, /*leader*/NULL);
          break;

      case MMD_TYPE_TABLE :
          format_table(dd, current, left, right);
          break;

      case MMD_TYPE_CODE_BLOCK :
          format_code(dd, current, left + CODE_PADDING, right - CODE_PADDING);
          break;
    }
  }
}


//
// 'format_table()' - Format a table...
//

static void
format_table(docdata_t *dd,		// I - Document data
             mmd_t     *table,		// I - Table node
             double    left,		// I - Left margin
             double    right)		// I - Right margin
{
  mmd_t		*current,		// Current node
		*next;			// Next node
  mmd_type_t	type;			// Node type
  size_t	col,			// Current column
		num_cols;		// Number of columns
  tablecol_t	cols[TABLECOL_MAX];	// Columns
  size_t	row,			// Current row
		num_rows;		// Number of rows
  tablerow_t	rows[TABLEROW_MAX],	// Rows
		*rowptr;		// Pointer to current row
  double	x,			// Current X position
		height,			// Height of cell
		format_width,		// Maximum format width of table
		table_width;		// Total width of table


  // Find all of the rows and columns in the table...
  num_cols = num_rows = 0;

  memset(cols, 0, sizeof(cols));
  memset(rows, 0, sizeof(rows));

  rowptr = rows;

  for (current = mmdGetFirstChild(table); current && num_rows < TABLEROW_MAX; current = next)
  {
    next = mmd_walk_next(table, current);
    type = mmdGetType(current);

    if (type == MMD_TYPE_TABLE_ROW)
    {
      // Parse row...
      for (col = 0, current = mmdGetFirstChild(current); current && num_cols < TABLECOL_MAX; current = mmdGetNextSibling(current), col ++)
      {
        rowptr->cells[col] = current;

        measure_cell(dd, current, cols + col);

        if (col >= num_cols)
          num_cols = col + 1;
      }

      rowptr ++;
      num_rows ++;
    }
  }

  // Figure out the width of each column...
  for (col = 0, table_width = 0.0; col < num_cols; col ++)
  {
    cols[col].max_width += 2.0 * TABLE_PADDING;

    table_width += cols[col].max_width;
    cols[col].width = cols[col].max_width;
  }

  format_width = right - left - 2.0 * TABLE_PADDING * num_cols;

  if (table_width > format_width)
  {
    // Content too wide, try scaling the widths...
    double	avg_width,		// Average column width
		base_width,		// Base width
		remaining_width,	// Remaining width
		scale_width;		// Width for scaling
    size_t	num_remaining_cols = 0;	// Number of remaining columns

    // First mark any columns that are narrower than the average width...
    avg_width = format_width / num_cols;

    for (col = 0, base_width = 0.0, remaining_width = 0.0; col < num_cols; col ++)
    {
      if (cols[col].width > avg_width)
      {
        remaining_width += cols[col].width;
        num_remaining_cols ++;
      }
      else
      {
        base_width += cols[col].width;
      }
    }

    // Then proportionately distribute the remaining width to the other columns...
    format_width -= base_width;

    for (col = 0, table_width = 0.0; col < num_cols; col ++)
    {
      if (cols[col].width > avg_width)
        cols[col].width = cols[col].width * format_width / remaining_width;

      table_width += cols[col].width;
    }
  }

  // Calculate the margins of each column in preparation for formatting
  for (col = 0, x = left + TABLE_PADDING; col < num_cols; col ++)
  {
    cols[col].left  = x;
    cols[col].right = x + cols[col].width;

    x += cols[col].width + 2.0 * TABLE_PADDING;
  }

  // Calculate the height of each row and cell in preparation for formatting
  for (row = 0, rowptr = rows; row < num_rows; row ++, rowptr ++)
  {
    for (col = 0; col < num_cols; col ++)
    {
      height = measure_cell(dd, rowptr->cells[col], cols + col) + 2.0 * TABLE_PADDING;
      if (height > rowptr->height)
        rowptr->height = height;
    }
  }

  // Render each table row...
  if (dd->st)
    dd->y -= SIZE_TABLE * LINE_HEIGHT;

  for (row = 0, rowptr = rows; row < num_rows; row ++, rowptr ++)
    render_row(dd, num_cols, cols, rowptr);
}


//
// 'make_target_name()' - Convert text to a target name.
//

static void
make_target_name(char       *dst,	// I - Destination buffer
                 const char *src,	// I - Source text
                 size_t     dstsize)	// I - Size of destination buffer
{
  char	*dstptr = dst,			// Pointer into destination
	*dstend = dst + dstsize - 1;	// End of destination


  while (*src && dstptr < dstend)
  {
    if (isalnum(*src) || *src == '.' || *src == '-')
      *dstptr++ = tolower(*src);
    else if (*src == ' ')
      *dstptr++ = '-';

    src ++;
  }

  *dstptr = '\0';
}


//
// 'measure_cell()' - Measure the dimensions of a table cell.
//

static double				// O - Formatted height
measure_cell(docdata_t  *dd,		// I - Document data
             mmd_t      *cell,		// I - Cell node
             tablecol_t *col)		// O - Column data
{
  mmd_t		*current,		// Current node
		*next;			// Next node
  mmd_type_t	type;			// Node type
  const char	*text,			// Current text
		*url;			// Current URL, if any
  bool		ws;			// Current whitespace
  docfont_t	font;			// Current font
  double	x = 0.0,		// Current X position
		width,			// Width of node
		wswidth,		// Width of whitespace
		height,			// Height of node
		lineheight = 0.0,	// Height of line
		cellheight = 0.0;	// Height of cell


  for (current = mmdGetFirstChild(cell); current; current = next)
  {
    next    = mmd_walk_next(cell, current);
    type    = mmdGetType(current);
    text    = mmdGetText(current);
    url     = mmdGetURL(current);
    ws      = mmdGetWhitespace(current);
    wswidth = 0.0;

    if (type == MMD_TYPE_IMAGE && url)
    {
      // Embed an image
      pdfio_obj_t *image = find_image(dd, url, /*imagenum*/NULL);
					// Image object

      if (!image)
        continue;

      // Image - treat as 100dpi
      width  = 72.0 * pdfioImageGetWidth(image) / IMAGE_PPI;
      height = 72.0 * pdfioImageGetHeight(image) / IMAGE_PPI;

      if (col->width > 0.0 && width > col->width)
      {
	// Too wide, scale to width...
	width  = col->width;
	height = width * pdfioImageGetHeight(image) / pdfioImageGetWidth(image);
      }
      else if (height > (dd->art_box.y2 - dd->art_box.y1))
      {
	// Too tall, scale to height...
	height = dd->art_box.y2 - dd->art_box.y1;
	width  = height * pdfioImageGetWidth(image) / pdfioImageGetHeight(image);
      }
    }
    else if (type == MMD_TYPE_HARD_BREAK && x > 0.0)
    {
      // Hard break...
      if (x > col->max_width)
        col->max_width = x;

      cellheight += lineheight;
      x          = 0.0;
      lineheight = 0.0;
      continue;
    }
    else if (type == MMD_TYPE_CHECKBOX)
    {
      // Checkbox...
      width = height = SIZE_TABLE;
    }
    else if (!text)
    {
      continue;
    }
    else
    {
      // Text fragment...
      if (type == MMD_TYPE_EMPHASIZED_TEXT)
	font = DOCFONT_ITALIC;
      else if (type == MMD_TYPE_STRONG_TEXT)
	font = DOCFONT_BOLD;
      else  if (type == MMD_TYPE_CODE_TEXT)
	font = DOCFONT_MONOSPACE;
      else if (mmdGetType(cell) == MMD_TYPE_TABLE_HEADER_CELL)
        font = DOCFONT_BOLD;
      else
	font = DOCFONT_REGULAR;

      width  = pdfioContentTextMeasure(dd->fonts[font], text, SIZE_TABLE);
      height = SIZE_TABLE * LINE_HEIGHT;

      if (ws && x > 0.0)
	wswidth = pdfioContentTextMeasure(dd->fonts[font], " ", SIZE_TABLE);
    }

    if (width > col->min_width)
      col->min_width = width;

    // See if this node will fit on the current line
    if (col->width > 0.0 && (x + width + wswidth) >= col->width)
    {
      // No, record the new line...
      if (x > col->max_width)
        col->max_width = x;

      cellheight += lineheight;
      x          = 0.0;
      lineheight = 0.0;
      wswidth    = 0.0;
    }

    x += width + wswidth;

    if (height > lineheight)
      lineheight = height;
  }

  // Capture the last line's measurements...
  if (x > col->max_width)
    col->max_width = x;

  if (x > 0.0)
    cellheight += lineheight;

  // Return the total height...
  return (cellheight);
}


//
// 'mmd_walk_next()' - Find the next markdown node.
//

static mmd_t *				// O - Next node or `NULL` at end
mmd_walk_next(mmd_t *top,		// I - Top node
              mmd_t *node)		// I - Current node
{
  mmd_t	*next,				// Next node
	*parent;			// Parent node


  // Figure out the next node under "top"...
  if ((next = mmdGetFirstChild(node)) == NULL)
  {
    if ((next = mmdGetNextSibling(node)) == NULL)
    {
      if ((parent = mmdGetParent(node)) != top)
      {
	while ((next = mmdGetNextSibling(parent)) == NULL)
	{
	  if ((parent = mmdGetParent(parent)) == top)
	    break;
	}
      }
    }
  }

  return (next);
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
  if (dd->st)
  {
    pdfioStreamClose(dd->st);
    add_links(dd);
  }

  // Prep the new page...
  page_dict = pdfioDictCreate(dd->pdf);

  dd->annots_array = pdfioArrayCreate(dd->pdf);
  dd->annots_obj   = pdfioFileCreateArrayObj(dd->pdf, dd->annots_array);
  pdfioDictSetObj(page_dict, "Annots", dd->annots_obj);

  pdfioDictSetRect(page_dict, "MediaBox", &dd->media_box);
//  pdfioDictSetRect(page_dict, "CropBox", &dd->crop_box);
  pdfioDictSetRect(page_dict, "ArtBox", &dd->art_box);

  for (fontface = DOCFONT_REGULAR; fontface < DOCFONT_MAX; fontface ++)
    pdfioPageDictAddFont(page_dict, docfont_names[fontface], dd->fonts[fontface]);

  for (i = 0; i < dd->num_images; i ++)
    pdfioPageDictAddImage(page_dict, pdfioStringCreatef(dd->pdf, "I%u", (unsigned)i), dd->images[i].obj);

  dd->st    = pdfioFileCreatePage(dd->pdf, page_dict);
  dd->color = DOCCOLOR_BLACK;
  dd->font  = DOCFONT_MAX;
  dd->fsize = 0.0;
  dd->y     = dd->art_box.y2;

  // Add header/footer text
  set_color(dd, DOCCOLOR_GRAY);
  set_font(dd, DOCFONT_REGULAR, SIZE_HEADFOOT);

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
// 'render_line()' - Render a line of text/graphics.
//

static void
render_line(docdata_t  *dd,		// I - Document data
            double     margin_left,	// I - Left margin
            double     margin_top,	// I - Top margin
            double     need_bottom,	// I - How much space is needed after
            double     lineheight,	// I - Height of line
            size_t     num_frags,	// I - Number of line fragments
            linefrag_t *frags)		// I - Line fragments
{
  size_t	i;			// Looping var
  linefrag_t	*frag;			// Current line fragment
  bool		in_text = false;	// Are we in a text block?


  if (!dd->st)
  {
    new_page(dd);
    margin_top = 0.0;
  }

  dd->y -= margin_top + lineheight;
  if ((dd->y - need_bottom) < dd->art_box.y1)
  {
    new_page(dd);

    dd->y -= lineheight;
  }

  for (i = 0, frag = frags; i < num_frags; i ++, frag ++)
  {
    if (frag->type == MMD_TYPE_CHECKBOX)
    {
      // Draw checkbox...
      set_color(dd, frag->color);

      if (in_text)
      {
	pdfioContentTextEnd(dd->st);
	in_text = false;
      }

      // Add box
      pdfioContentPathRect(dd->st, frag->x + 1.0 + margin_left, dd->y, frag->width - 3.0, frag->height - 3.0);

      if (frag->text)
      {
        // Add check
        pdfioContentPathMoveTo(dd->st, frag->x + 3.0 + margin_left, dd->y + 2.0);
        pdfioContentPathLineTo(dd->st, frag->x + frag->width - 4.0 + margin_left, dd->y + frag->height - 5.0);

        pdfioContentPathMoveTo(dd->st, frag->x + 3.0 + margin_left, dd->y + frag->height - 5.0);
        pdfioContentPathLineTo(dd->st, frag->x + frag->width - 4.0 + margin_left, dd->y + 2.0);
      }

      pdfioContentStroke(dd->st);
    }
    else if (frag->text)
    {
      // Draw text
      if (!in_text)
      {
	pdfioContentTextBegin(dd->st);
	pdfioContentTextMoveTo(dd->st, frag->x + margin_left, dd->y);

	in_text = true;
      }

      if (frag->ws && frag->font == DOCFONT_MONOSPACE)
      {
	set_font(dd, DOCFONT_REGULAR, frag->height);
	pdfioContentTextShow(dd->st, UNICODE_VALUE, " ");
      }

      set_color(dd, frag->color);
      set_font(dd, frag->font, frag->height);

      if (frag->font == DOCFONT_MONOSPACE)
	pdfioContentTextShow(dd->st, UNICODE_VALUE, frag->text);
      else
	pdfioContentTextShowf(dd->st, UNICODE_VALUE, "%s%s", frag->ws ? " " : "", frag->text);

      if (frag->url && dd->num_links < DOCLINK_MAX)
      {
        doclink_t *l = dd->links + dd->num_links;
					// Pointer to this link record

        if (!strcmp(frag->url, "@"))
        {
          // Use mapped text as link target...
          char	targetlink[129];	// Targeted link

          targetlink[0] = '#';
          make_target_name(targetlink + 1, frag->text, sizeof(targetlink) - 1);

          l->url = pdfioStringCreate(dd->pdf, targetlink);
        }
        else if (!strcmp(frag->url, "@@"))
        {
          // Use literal text as anchor...
          l->url = pdfioStringCreatef(dd->pdf, "#%s", frag->text);
        }
        else
        {
          // Use URL as-is...
          l->url = frag->url;
        }

        l->box.x1 = frag->x;
        l->box.y1 = dd->y;
        l->box.x2 = frag->x + frag->width;
        l->box.y2 = dd->y + frag->height;

        dd->num_links ++;
      }
    }
    else
    {
      // Draw image
      char	imagename[32];		// Current image name

      if (in_text)
      {
	pdfioContentTextEnd(dd->st);
	in_text = false;
      }

      snprintf(imagename, sizeof(imagename), "I%u", (unsigned)frag->imagenum);
      pdfioContentDrawImage(dd->st, imagename, frag->x + margin_left, dd->y, frag->width, frag->height);
    }
  }

  if (in_text)
    pdfioContentTextEnd(dd->st);
}


//
// 'render_row()' - Render a table row...
//

static void
render_row(docdata_t  *dd,		// I - Document data
           size_t     num_cols,		// I - Number of columns
           tablecol_t *cols,		// I - Column information
           tablerow_t *row)		// I - Row
{
  size_t	col;			// Current column
  double	row_y;			// Start of row
  docfont_t	deffont;		// Default font


  // Start a new page as needed...
  if (!dd->st)
    new_page(dd);

  if ((dd->y - row->height) < dd->art_box.y1)
    new_page(dd);

  if (mmdGetType(row->cells[0]) == MMD_TYPE_TABLE_HEADER_CELL)
  {
    // Header row, no border...
    deffont = DOCFONT_BOLD;
  }
  else
  {
    // Regular body row, add borders...
    deffont = DOCFONT_REGULAR;

    set_color(dd, DOCCOLOR_GRAY);
    pdfioContentPathRect(dd->st, cols[0].left - TABLE_PADDING, dd->y - row->height, cols[num_cols - 1].right - cols[0].left + 2.0 * TABLE_PADDING, row->height);
    for (col = 1; col < num_cols; col ++)
    {
      pdfioContentPathMoveTo(dd->st, cols[col].left - TABLE_PADDING, dd->y);
      pdfioContentPathLineTo(dd->st, cols[col].left - TABLE_PADDING, dd->y - row->height);
    }
    pdfioContentStroke(dd->st);
  }

  row_y = dd->y;

  for (col = 0; col < num_cols; col ++)
  {
    dd->y = row_y;

    format_block(dd, row->cells[col], deffont, SIZE_TABLE, cols[col].left, cols[col].right, /*leader*/NULL);
  }

  dd->y = row_y - row->height;
}


//
// 'set_color()' - Set the stroke and fill color as needed.
//

static void
set_color(docdata_t  *dd,		// I - Document data
          doccolor_t color)		// I - Document color
{
  if (color == dd->color)
    return;

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
    case DOCCOLOR_ORANGE :
        pdfioContentSetFillColorDeviceRGB(dd->st, 1.0, 0.5, 0.0);
        pdfioContentSetStrokeColorDeviceRGB(dd->st, 1.0, 0.5, 0.0);
        break;
    case DOCCOLOR_BLUE :
        pdfioContentSetFillColorDeviceRGB(dd->st, 0.0, 0.0, 0.8);
        pdfioContentSetStrokeColorDeviceRGB(dd->st, 0.0, 0.0, 0.8);
        break;
    case DOCCOLOR_LTGRAY :
        pdfioContentSetFillColorDeviceGray(dd->st, 0.933);
        pdfioContentSetStrokeColorDeviceGray(dd->st, 0.933);
        break;
    case DOCCOLOR_GRAY :
        pdfioContentSetFillColorDeviceGray(dd->st, 0.333);
        pdfioContentSetStrokeColorDeviceGray(dd->st, 0.333);
        break;
  }

  dd->color = color;
}


//
// 'set_font()' - Set the font typeface and size as needed.
//

static void
set_font(docdata_t *dd,			// I - Document data
         docfont_t font,		// I - Font
         double    fsize)		// I - Font size
{
  if (font == dd->font && fabs(fsize - dd->fsize) < 0.1)
    return;

  if (font == DOCFONT_MAX)
    return;

  pdfioContentSetTextFont(dd->st, docfont_names[font], fsize);

  if (fabs(fsize - dd->fsize) >= 0.1)
    pdfioContentSetTextLeading(dd->st, fsize * LINE_HEIGHT);

  dd->font  = font;
  dd->fsize = fsize;
}


//
// 'write_actions()' - Write remaining actions to the PDF file.
//

static void
write_actions(docdata_t *dd)		// I - Document data
{
  size_t	i, j;			// Looping vars
  docaction_t	*a;			// Current action
  doctarget_t	*t;			// Current target


  for (i = 0, a = dd->actions; i < dd->num_actions; i ++, a ++)
  {
    for (j = 0, t = dd->targets; j < dd->num_targets; j ++, t ++)
    {
      if (!strcmp(a->target, t->name))
        break;
    }

    if (j < dd->num_targets)
    {
      pdfio_array_t *dest = pdfioArrayCreate(dd->pdf);
					// Destination array

      pdfioArrayAppendObj(dest, pdfioFileGetPage(dd->pdf, t->page));
      pdfioArrayAppendName(dest, "XYZ");
      pdfioArrayAppendNumber(dest, PAGE_LEFT);
      pdfioArrayAppendNumber(dest, t->y);
      pdfioArrayAppendNumber(dest, 0.0);

      pdfioDictSetArray(pdfioObjGetDict(a->obj), "Dest", dest);
    }

    pdfioObjClose(a->obj);
  }
}


//
// 'write_toc()' - Write the table-of-contents outline.
//

static void
write_toc(docdata_t *dd)		// I - Document data
{
  size_t	i, j;			// Looping vars
  doctoc_t	*t,			// Table-of-contents entry
		*nt,			// Next entry
		*levels[6];		// Current entries for each level
  int		level;			// Current level
  pdfio_dict_t	*dict;			// Outline dictionary
  pdfio_obj_t	*obj;			// Outline object


  // Initialize the various TOC levels...
  levels[0] = levels[1] = levels[2] = levels[3] = levels[4] = levels[5] = NULL;

  // Scan the table of contents and finalize the dictionaries...
  for (i = 0, t = dd->toc; i < dd->num_toc; i ++, t ++)
  {
    // Set parent, previous, and next entries...
    if (t->level > 0 && levels[t->level - 1])
      pdfioDictSetObj(t->dict, "Parent", levels[t->level - 1]->obj);

    if (levels[t->level])
      pdfioDictSetObj(t->dict, "Prev", levels[t->level]->obj);

    for (j = i + 1, nt = t + 1; j < dd->num_toc; j ++, nt ++)
    {
      if (nt->level == t->level)
      {
	pdfioDictSetObj(t->dict, "Next", nt->obj);
        break;
      }
      else if (nt->level < t->level)
      {
        break;
      }
    }

    // First, last, and count...
    for (level = 0; level < t->level; level ++)
      levels[level]->count ++;

    levels[t->level] = t;

    if ((i + 1) < dd->num_toc && t[1].level > t->level)
      pdfioDictSetObj(t->dict, "First", t[1].obj);

    if ((i + 1) >= dd->num_toc)
    {
      // Close out all levels...
      for (level = t->level; level > 0; level --)
      {
        pdfioDictSetObj(levels[level - 1]->dict, "Last", levels[level]->obj);
	levels[level] = NULL;
      }
    }
    else if (t->level > t[1].level)
    {
      // Close out N levels...
      for (level = t->level; level > t[1].level; level --)
      {
        pdfioDictSetObj(levels[level - 1]->dict, "Last", levels[level]->obj);
	levels[level] = NULL;
      }
    }
  }

  // Create the top-level outline object...
  dict = pdfioDictCreate(dd->pdf);
  obj  = pdfioFileCreateObj(dd->pdf, dict);

  pdfioDictSetName(dict, "Type", "Outline");
  pdfioDictSetNumber(dict, "Count", dd->num_toc);
  pdfioDictSetObj(dict, "First", dd->toc[0].obj);

  // Close the objects for the entries...
  for (i = 0, t = dd->toc; i < dd->num_toc; i ++, t ++)
  {
    if (t->level == 0)
      pdfioDictSetObj(dict, "Last", t->obj);

    if (t->count)
    {
      // Set Count value...
      if (t->level == 0)
        pdfioDictSetNumber(t->dict, "Count", t->count);
      else
        pdfioDictSetNumber(t->dict, "Count", -t->count);
    }

    pdfioObjClose(t->obj);
  }

  // Close the outline object and add it to the document catalog...
  pdfioObjClose(obj);

  pdfioDictSetObj(pdfioFileGetCatalog(dd->pdf), "Outlines", obj);
}
