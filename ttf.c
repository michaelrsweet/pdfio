//
// Implementation file for TTF library
//
//     https://github.com/michaelrsweet/ttf
//
// Copyright © 2018-2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#ifdef _WIN32
#  define _CRT_SECURE_NO_WARNINGS
#endif // _WIN32

#include "ttf.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>

#ifdef _WIN32
#  include <io.h>
#  include <direct.h>

//
// Microsoft renames the POSIX functions to _name, and introduces
// a broken compatibility layer using the original names.  As a result,
// random crashes can occur when, for example, strdup() allocates memory
// from a different heap than used by malloc() and free().
//
// To avoid moronic problems like this, we #define the POSIX function
// names to the corresponding non-standard Microsoft names.
//

#  define access	_access
#  define close		_close
#  define fileno	_fileno
#  define lseek		_lseek
#  define mkdir(d,p)	_mkdir(d)
#  define open		_open
#  define read		_read
#  define rmdir		_rmdir
#  define snprintf	_snprintf
#  define strdup	_strdup
#  define unlink	_unlink
#  define vsnprintf	_vsnprintf
#  define write		_write

//
// Map various parameters for POSIX...
//

#  define F_OK		00
#  define W_OK		02
#  define R_OK		04
#  define O_BINARY	_O_BINARY
#  define O_RDONLY	_O_RDONLY
#  define O_WRONLY	_O_WRONLY
#  define O_CREAT	_O_CREAT
#  define O_TRUNC	_O_TRUNC

typedef __int64 ssize_t;		// POSIX type not present on Windows...

#else
#  include <unistd.h>
#  define O_BINARY	0
#endif // _WIN32


//
// DEBUG is typically defined for debug builds.  TTF_DEBUG maps to printf when
// DEBUG is defined and is a no-op otherwise...
//

#ifdef DEBUG
#  define TTF_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#  define TTF_DEBUG(...)
#endif // DEBUG


//
// TTF_FORMAT_ARGS tells the compiler to validate printf-style format
// arguments, producing warnings when there are issues...
//

#if defined(__has_extension) || defined(__GNUC__)
#  define TTF_FORMAT_ARGS(a,b) __attribute__ ((__format__(__printf__, a, b)))
#else
#  define TTF_FORMAT_ARGS(a,b)
#endif // __has_extension || __GNUC__


//
// Constants...
//

#define TTF_FONT_MAX_CHAR	262144	// Maximum number of character values


//
// TTF/OFF tag constants...
//

#define TTF_OFF_cmap	0x636d6170	// Character to glyph mapping
#define TTF_OFF_head	0x68656164	// Font header
#define TTF_OFF_hhea	0x68686561	// Horizontal header
#define TTF_OFF_hmtx	0x686d7478	// Horizontal metrics
#define TTF_OFF_maxp	0x6d617870	// Maximum profile
#define TTF_OFF_name	0x6e616d65	// Naming table
#define TTF_OFF_OS_2	0x4f532f32	// OS/2 and Windows specific metrics
#define TTF_OFF_post	0x706f7374	// PostScript information

#define TTF_OFF_Unicode	0	// Unicode platform ID

#define TTF_OFF_Mac		1	// Macintosh platform ID
#define TTF_OFF_Mac_Roman	0	// Macintosh Roman encoding ID
#define TTF_OFF_Mac_USEnglish	0	// Macintosh US English language ID

#define TTF_OFF_Windows	3	// Windows platform ID
#define TTF_OFF_Windows_English 9	// Windows English language ID base
#define TTF_OFF_Windows_UCS2	1	// Windows UCS-2 encoding
#define TTF_OFF_Windows_UCS4	10	// Windows UCS-4 encoding

#define TTF_OFF_Copyright	0	// Copyright naming string
#define TTF_OFF_FontFamily	1	// Font family naming string ("Arial")
#define TTF_OFF_FontSubfamily	2	// Font sub-family naming string ("Bold")
#define TTF_OFF_FontFullName	4	// Font full name ("Arial Bold")
#define TTF_OFF_FontVersion	5	// Font version number
#define TTF_OFF_PostScriptName	6	// Font PostScript name


//
// Local types...
//

typedef struct _ttf_metric_s		//*** Font metric information ****/
{
  short		width,			// Advance width
		left_bearing;		// Left side bearing
} _ttf_metric_t;

typedef struct _ttf_off_dir_s		// OFF/TTF directory entry
{
  unsigned	tag;			// Table identifier
  unsigned	checksum;		// Checksum of table
  unsigned	offset;			// Offset from the beginning of the file
  unsigned	length;			// Length
} _ttf_off_dir_t;

typedef struct _ttf_off_table_s		// OFF/TTF offset table
{
  int		num_entries;		// Number of table entries
  _ttf_off_dir_t *entries;		// Table entries
} _ttf_off_table_t;

typedef struct _ttf_off_name_s		// OFF/TTF name string
{
  unsigned short platform_id,		// Platform identifier
		encoding_id,		// Encoding identifier
		language_id,		// Language identifier
		name_id,		// Name identifier
		length,			// Length of string
		offset;			// Offset from start of storage area
} _ttf_off_name_t;

typedef struct _ttf_off_names_s		// OFF/TTF naming table
{
  int		num_names;		// Number of names
  _ttf_off_name_t *names;		// Names
  unsigned char	*storage;		// Storage area
  unsigned	storage_size;		// Size of storage area
} _ttf_off_names_t;

struct _ttf_s
{
  int		fd;			// File descriptor
  size_t	idx;			// Font number in file
  ttf_err_cb_t	err_cb;			// Error callback, if any
  void		*err_data;		// Error callback data
  _ttf_off_table_t table;		// Offset table
  _ttf_off_names_t names;		// Names
  size_t	num_fonts;		// Number of fonts in this file
  char		*copyright;		// Copyright string
  char		*family;		// Font family string
  char		*postscript_name;	// PostScript name string
  char		*version;		// Font version string
  bool		is_fixed;		// Is this a fixed-width font?
  int		max_char,		// Last character in font
		min_char;		// First character in font
  size_t	num_cmap;		// Number of entries in glyph map
  int		*cmap;			// Unicode character to glyph map
  _ttf_metric_t	*widths[TTF_FONT_MAX_CHAR / 256];
					// Character metrics (sparse array)
  float		units;			// Width units
  short		ascent,			// Maximum ascent above baseline
		descent,		// Maximum descent below baseline
		cap_height,		// "A" height
		x_height,		// "x" height
		x_max,			// Bounding box
		x_min,
		y_max,
		y_min,
		weight;			// Font weight
  float		italic_angle;		// Angle of italic text
  ttf_stretch_t	stretch;		// Font stretch value
  ttf_style_t	style;			// Font style
};

typedef struct _ttf_off_cmap4_s		// Format 4 cmap table
{
  unsigned short startCode,		// First character
		endCode,		// Last character
		idRangeOffset;		// Offset for range (modulo 65536)
  short		idDelta;		// Delta for range (modulo 65536)
} _ttf_off_cmap4_t;

typedef struct _ttf_off_cmap12_s	// Format 12 cmap table
{
  unsigned	startCharCode,		// First character
		endCharCode,		// Last character
		startGlyphID;		// Glyph index for the first character
} _ttf_off_cmap12_t;

typedef struct _ttf_off_cmap13_s	// Format 13 cmap table
{
  unsigned	startCharCode,		// First character
		endCharCode,		// Last character
		glyphID;		// Glyph index for all characters
} _ttf_off_cmap13_t;

typedef struct _ttf_off_head_s		// Font header
{
  unsigned short unitsPerEm;		// Units for widths/coordinates
  short		xMin,			// Bounding box of all glyphs
		yMin,
		xMax,
		yMax;
  unsigned short macStyle;		// Mac style bits
} _ttf_off_head_t;

#define TTF_OFF_macStyle_Bold		0x01
#define TTF_OFF_macStyle_Italic	0x02
#define TTF_OFF_macStyle_Underline	0x04
#define TTF_OFF_macStyle_Outline	0x08
#define TTF_OFF_macStyle_Shadow	0x10
#define TTF_OFF_macStyle_Condensed	0x20
#define TTF_OFF_macStyle_Extended	0x40

typedef struct _ttf_off_hhea_s		// Horizontal header
{
  short		ascender,		// Ascender
		descender;		// Descender
  int		numberOfHMetrics;	// Number of horizontal metrics
} _ttf_off_hhea_t;

typedef struct _ttf_off_os_2_s		// OS/2 information
{
  unsigned short usWeightClass,		// Font weight
		usWidthClass,		// Font weight
		fsType;			// Type bits
  short		sTypoAscender,		// Ascender
		sTypoDescender,		// Descender
		sxHeight,		// xHeight
		sCapHeight;		// CapHeight
} _ttf_off_os_2_t;

typedef struct _ttf_off_post_s		// PostScript information
{
  float		italicAngle;		// Italic angle
  unsigned	isFixedPitch;		// Fixed-width font?
} _ttf_off_post_t;


//
// Local functions...
//

static char	*copy_name(ttf_t *font, unsigned name_id);
static void	errorf(ttf_t *font, const char *message, ...) TTF_FORMAT_ARGS(2,3);
static bool	read_cmap(ttf_t *font);
static bool	read_head(ttf_t *font, _ttf_off_head_t *head);
static bool	read_hhea(ttf_t *font, _ttf_off_hhea_t *hhea);
static _ttf_metric_t *read_hmtx(ttf_t *font, _ttf_off_hhea_t *hhea);
static int	read_maxp(ttf_t *font);
static bool	read_names(ttf_t *font);
static bool	read_os_2(ttf_t *font, _ttf_off_os_2_t *os_2);
static bool	read_post(ttf_t *font, _ttf_off_post_t *post);
static int	read_short(ttf_t *font);
static bool	read_table(ttf_t *font);
static unsigned	read_ulong(ttf_t *font);
static int	read_ushort(ttf_t *font);
static unsigned	seek_table(ttf_t *font, unsigned tag, unsigned offset, bool required);


//
// 'ttfCreate()' - Create a new font object for the named font family.
//

ttf_t *					// O - New font object
ttfCreate(const char   *filename,	// I - Filename
          size_t       idx,		// I - Font number to create in collection (0-based)
          ttf_err_cb_t err_cb,		// I - Error callback or `NULL` to log to stderr
          void         *err_data)	// I - Error callback data
{
  ttf_t			*font = NULL;	// New font object
  size_t		i;		// Looping var
  _ttf_metric_t		*widths = NULL;	// Glyph metrics
  _ttf_off_head_t	head;		// head table
  _ttf_off_hhea_t	hhea;		// hhea table
  _ttf_off_os_2_t	os_2;		// OS/2 table
  _ttf_off_post_t	post;		// PostScript table


  TTF_DEBUG("ttfCreate(filename=\"%s\", idx=%u, err_cb=%p, err_data=%p)\n", filename, (unsigned)idx, err_cb, err_data);

  // Range check input..
  if (!filename)
  {
    errno = EINVAL;
    return (NULL);
  }

  // Allocate memory...
  if ((font = (ttf_t *)calloc(1, sizeof(ttf_t))) == NULL)
    return (NULL);

  font->idx      = idx;
  font->err_cb   = err_cb;
  font->err_data = err_data;

  // Open the font file...
  if ((font->fd = open(filename, O_RDONLY | O_BINARY)) < 0)
  {
    errorf(font, "Unable to open '%s': %s", filename, strerror(errno));
    goto error;
  }

  TTF_DEBUG("ttfCreate: fd=%d\n", font->fd);

  // Read the table of contents and the identifying names...
  if (!read_table(font))
    goto error;

  TTF_DEBUG("ttfCreate: num_entries=%d\n", font->table.num_entries);

  if (!read_names(font))
    goto error;

  TTF_DEBUG("ttfCreate: num_names=%d\n", font->names.num_names);

  // Copy key font meta data strings...
  font->copyright       = copy_name(font, TTF_OFF_Copyright);
  font->family          = copy_name(font, TTF_OFF_FontFamily);
  font->postscript_name = copy_name(font, TTF_OFF_PostScriptName);
  font->version         = copy_name(font, TTF_OFF_FontVersion);

  if (read_post(font, &post))
  {
    font->italic_angle = post.italicAngle;
    font->is_fixed     = post.isFixedPitch != 0;
  }

  TTF_DEBUG("ttfCreate: copyright=\"%s\"\n", font->copyright);
  TTF_DEBUG("ttfCreate: family=\"%s\"\n", font->family);
  TTF_DEBUG("ttfCreate: postscript_name=\"%s\"\n", font->postscript_name);
  TTF_DEBUG("ttfCreate: version=\"%s\"\n", font->version);
  TTF_DEBUG("ttfCreate: italic_angle=%g\n", font->italic_angle);
  TTF_DEBUG("ttfCreate: is_fixed=%s\n", font->is_fixed ? "true" : "false");

  if (!read_cmap(font))
    goto error;

  if (!read_head(font, &head))
    goto error;

  font->units = (float)head.unitsPerEm;
  font->x_max = head.xMax;
  font->x_min = head.xMin;
  font->y_max = head.yMax;
  font->y_min = head.yMin;

  if (head.macStyle & TTF_OFF_macStyle_Italic)
  {
    if (font->postscript_name && strstr(font->postscript_name, "Oblique"))
      font->style = TTF_STYLE_OBLIQUE;
    else
      font->style = TTF_STYLE_ITALIC;
  }
  else
    font->style = TTF_STYLE_NORMAL;

  if (!read_hhea(font, &hhea))
    goto error;

  font->ascent  = hhea.ascender;
  font->descent = hhea.descender;

  if (read_maxp(font) < 0)
    goto error;

  if (hhea.numberOfHMetrics > 0)
  {
    if ((widths = read_hmtx(font, &hhea)) == NULL)
      goto error;
  }
  else
  {
    errorf(font, "Number of horizontal metrics is 0.");
    goto error;
  }

  if (read_os_2(font, &os_2))
  {
    // Copy key values from OS/2 table...
    static const ttf_stretch_t stretches[] =
    {
      TTF_STRETCH_ULTRA_CONDENSED,	// ultra-condensed
      TTF_STRETCH_EXTRA_CONDENSED,	// extra-condensed
      TTF_STRETCH_CONDENSED,		// condensed
      TTF_STRETCH_SEMI_CONDENSED,	// semi-condensed
      TTF_STRETCH_NORMAL,		// normal
      TTF_STRETCH_SEMI_EXPANDED,	// semi-expanded
      TTF_STRETCH_EXPANDED,		// expanded
      TTF_STRETCH_EXTRA_EXPANDED,	// extra-expanded
      TTF_STRETCH_ULTRA_EXPANDED	// ultra-expanded
    };

    if (os_2.usWidthClass >= 1 && os_2.usWidthClass <= (int)(sizeof(stretches) / sizeof(stretches[0])))
      font->stretch = stretches[os_2.usWidthClass - 1];

    font->weight     = (short)os_2.usWeightClass;
    font->cap_height = os_2.sCapHeight;
    font->x_height   = os_2.sxHeight;
  }
  else
  {
    // Default key values since there isn't an OS/2 table...
    TTF_DEBUG("ttfCreate: Unable to read OS/2 table.\n");

    font->weight = 400;
  }

  if (font->cap_height == 0)
    font->cap_height = font->ascent;

  if (font->x_height == 0)
    font->x_height   = 3 * font->ascent / 5;

  // Build a sparse glyph widths table...
  font->min_char = -1;

  for (i = 0; i < font->num_cmap; i ++)
  {
    if (font->cmap[i] >= 0)
    {
      int	bin = (int)i / 256,	// Sub-array bin
		glyph = font->cmap[i];	// Glyph index

      // Update min/max...
      if (font->min_char < 0)
        font->min_char = (int)i;

      font->max_char = (int)i;

      // Allocate a sub-array as needed...
      if (!font->widths[bin])
        font->widths[bin] = (_ttf_metric_t *)calloc(256, sizeof(_ttf_metric_t));

      // Copy the width of the specified glyph or the last one if we are past
      // the end of the table...
      if (glyph >= hhea.numberOfHMetrics)
	font->widths[bin][i & 255] = widths[hhea.numberOfHMetrics - 1];
      else
	font->widths[bin][i & 255] = widths[glyph];
    }
  }

  // Cleanup and return the font...
  free(widths);

  return (font);

  // If we get here something bad happened...
  error:

  free(widths);
  ttfDelete(font);

  return (NULL);
}


//
// 'ttfDelete()' - Free all memory used for a font family object.
//

void
ttfDelete(ttf_t *font)			// I - Font
{
  int	i;				// Looping var


  // Range check input...
  if (!font)
    return;

  // Close the font file...
  if (font->fd >= 0)
    close(font->fd);

  // Free all memory used...
  free(font->copyright);
  free(font->family);
  free(font->postscript_name);
  free(font->version);

  free(font->table.entries);
  free(font->names.names);
  free(font->names.storage);

  free(font->cmap);

  for (i = 0; i < 256; i ++)
    free(font->widths[i]);

  free(font);
}


//
// 'ttfGetAscent()' - Get the maximum height of non-accented characters.
//

int					// O - Ascent in 1000ths
ttfGetAscent(ttf_t *font)		// I - Font
{
  return (font ? (int)(1000 * font->ascent / font->units) : 0);
}


//
// 'ttfGetBounds()' - Get the bounds of all characters in a font.
//

ttf_rect_t *				// O - Bounds or `NULL` on error
ttfGetBounds(ttf_t      *font,		// I - Font
             ttf_rect_t *bounds)	// I - Bounds buffer
{
  // Range check input...
  if (!font || !bounds)
  {
    if (bounds)
      memset(bounds, 0, sizeof(ttf_rect_t));

    return (NULL);
  }

  bounds->left   = 1000.0f * font->x_min / font->units;
  bounds->right  = 1000.0f * font->x_max / font->units;
  bounds->bottom = 1000.0f * font->y_min / font->units;
  bounds->top    = 1000.0f * font->y_max / font->units;

  return (bounds);
}


//
// 'ttfGetCapHeight()' - Get the height of capital letters.
//

int					// O - Capital letter height in 1000ths
ttfGetCapHeight(ttf_t *font)		// I - Font
{
  return (font ? (int)(1000 * font->cap_height / font->units) : 0);
}


//
// 'ttfGetCMap()' - Get the Unicode to glyph mapping table.
//

const int *				// O - CMap table
ttfGetCMap(ttf_t  *font,		// I - Font
           size_t *num_cmap)		// O - Number of entries in table
{
  // Range check input...
  if (!font || !num_cmap)
  {
    if (num_cmap)
      *num_cmap = 0;

    return (NULL);
  }

  *num_cmap = font->num_cmap;
  return (font->cmap);
}


//
// 'ttfGetCopyright()' - Get the copyright text for a font.
//

const char *				// O - Copyright text
ttfGetCopyright(ttf_t *font)		// I - Font
{
  return (font ? font->copyright : NULL);
}


//
// 'ttfGetDescent()' - Get the maximum depth of non-accented characters.
//

int					// O - Descent in 1000ths
ttfGetDescent(ttf_t *font)		// I - Font
{
  return (font ? (int)(1000 * font->descent / font->units) : 0);
}


//
// 'ttfGetExtents()' - Get the extents of a UTF-8 string.
//
// This function computes the extents of a UTF-8 string when rendered using the
// specified font and size.
//

ttf_rect_t *				// O - Pointer to extents or `NULL` on error
ttfGetExtents(
    ttf_t      *font,			// I - Font
    float      size,			// I - Font size
    const char *s,			// I - String
    ttf_rect_t *extents)		// O - Extents of the string
{
  bool		first = true;		// First character?
  int		ch,			// Current character
		width = 0;		// Width
  _ttf_metric_t	*widths;		// Widths


  TTF_DEBUG("ttfGetExtents(font=%p, size=%.2f, s=\"%s\", extents=%p)\n", (void *)font, size, s, (void *)extents);

  // Make sure extents is zeroed out...
  if (extents)
    memset(extents, 0, sizeof(ttf_rect_t));

  // Range check input...
  if (!font || size <= 0.0f || !s || !extents)
    return (NULL);

  // Loop through the string...
  while (*s)
  {
    // Get the next Unicode character...
    if ((*s & 0xe0) == 0xc0 && (s[1] & 0xc0) == 0x80)
    {
      // Two byte UTF-8
      ch = ((*s & 0x1f) << 6) | (s[1] & 0x3f);
      s += 2;
    }
    else if ((*s & 0xf0) == 0xe0 && (s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80)
    {
      // Three byte UTF-8
      ch = ((*s & 0x0f) << 12) | ((s[1] & 0x3f) << 6) | (s[2] & 0x3f);
      s += 3;
    }
    else if ((*s & 0xf8) == 0xf0 && (s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80 && (s[3] & 0xc0) == 0x80)
    {
      // Four byte UTF-8
      ch = ((*s & 0x07) << 18) | ((s[1] & 0x3f) << 12) | ((s[2] & 0x3f) << 6) | (s[3] & 0x3f);
      s += 4;
    }
    else if (*s & 0x80)
    {
      // Invalid UTF-8
      errorf(font, "Invalid UTF-8 sequence starting with 0x%02X.", *s & 255);
      return (NULL);
    }
    else
    {
      // ASCII...
      ch = *s++;
    }

    // Find its width...
    if ((widths = font->widths[ch / 256]) != NULL)
    {
      if (first)
      {
        extents->left = -widths[ch & 255].left_bearing / font->units;
        first         = false;
      }

      width += widths[ch & 255].width;
    }
    else if ((widths = font->widths[0]) != NULL)
    {
      // Use the ".notdef" (0) glyph width...
      if (first)
      {
        extents->left = -widths[0].left_bearing / font->units;
        first         = false;
      }

      width += widths[0].width;
    }
  }

  // Calculate the bounding box for the text and return...
  TTF_DEBUG("ttfGetExtents: width=%d\n", width);

  extents->bottom = size * font->y_min / font->units;
  extents->right  = size * width / font->units + extents->left;
  extents->top    = size * font->y_max / font->units;

  return (extents);
}


//
// 'ttfGetFamily()' - Get the family name of a font.
//

const char *				// O - Family name
ttfGetFamily(ttf_t *font)		// I - Font
{
  return (font ? font->family : NULL);
}


//
// 'ttfIsFixedPitch()' - Determine whether a font is fixedpitch.
//

bool					// O - `true` if fixed pitch, `false` otherwise
ttfIsFixedPitch(ttf_t *font)		// I - Font
{
  return (font ? font->is_fixed : false);
}


//
// 'ttfGetItalicAngle()' - Get the italic angle.
//

float					// O - Angle in degrees
ttfGetItalicAngle(ttf_t *font)		// I - Font
{
  return (font ? font->italic_angle : 0.0f);
}


//
// 'ttfGetMaxChar()' - Get the last character in the font.
//

int					// O - Last character in font
ttfGetMaxChar(ttf_t *font)		// I - Font
{
  return (font ? font->max_char : 0);
}


//
// 'ttfGetMinChar()' - Get the first character in the font.
//

int					// O - First character in font
ttfGetMinChar(ttf_t *font)		// I - Font
{
  return (font ? font->min_char : 0);
}


//
// 'ttfGetNumFonts()' - Get the number of fonts in this collection.
//

size_t					// O - Number of fonts
ttfGetNumFonts(ttf_t *font)		// I - Font
{
  return (font ? font->num_fonts : 0);
}


//
// 'ttfGetPostScriptName()' - Get the PostScript name of a font.
//

const char *				// O - PostScript name
ttfGetPostScriptName(
    ttf_t *font)			// I - Font
{
  return (font ? font->postscript_name : NULL);
}


//
// 'ttfGetStretch()' - Get the font "stretch" value...
//

ttf_stretch_t				// O - Stretch value
ttfGetStretch(ttf_t *font)		// I - Font
{
  return (font ? font->stretch : TTF_STRETCH_NORMAL);
}


//
// 'ttfGetStyle()' - Get the font style.
//

ttf_style_t				// O - Style
ttfGetStyle(ttf_t *font)		// I - Font
{
  return (font ? font->style : TTF_STYLE_NORMAL);
}


//
// 'ttfGetVersion()' - Get the version number of a font.
//

const char *				// O - Version number
ttfGetVersion(ttf_t *font)		// I - Font
{
  return (font ? font->version : NULL);
}


//
// 'ttfGetWeight()' - Get the weight of a font.
//

ttf_weight_t				// O - Weight
ttfGetWeight(ttf_t *font)		// I - Font
{
  return (font ? (ttf_weight_t)font->weight : TTF_WEIGHT_400);
}


//
// 'ttfGetWidth()' - Get the width of a single character.
//

int					// O - Width in 1000ths
ttfGetWidth(ttf_t *font,		// I - Font
            int   ch)			// I - Unicode character
{
  int	bin = ch >> 8;			// Bin in widths array


  // Range check input...
  if (!font || ch < ' ' || ch == 0x7f)
    return (0);
  else if (font->widths[bin])
    return ((int)(1000.0f * font->widths[bin][ch & 255].width / font->units));
  else if (font->widths[0])		// .notdef
    return ((int)(1000.0f * font->widths[0][0].width / font->units));
  else
    return (0);
}


//
// 'ttfGetXHeight()' - Get the height of lowercase letters.
//

int					// O - Lowercase letter height in 1000ths
ttfGetXHeight(ttf_t *font)		// I - Font
{
  return (font ? (int)(1000 * font->x_height / font->units) : 0);
}


//
// 'copy_name()' - Copy a name string from a font.
//

static char *				// O - Name string or `NULL`
copy_name(ttf_t    *font,		// I - Font
          unsigned name_id)		// I - Name identifier
{
  int			i;		// Looping var
  _ttf_off_name_t	*name;		// Current name


  for (i = font->names.num_names, name = font->names.names; i > 0; i --, name ++)
  {
    if (name->name_id == name_id &&
        ((name->platform_id == TTF_OFF_Mac && name->language_id == TTF_OFF_Mac_USEnglish) ||
         (name->platform_id == TTF_OFF_Windows && (name->language_id & 0xff) == TTF_OFF_Windows_English)))
    {
      char	temp[1024],	// Temporary string buffer
		*tempptr,	// Pointer into temporary string
		*storptr;	// Pointer into storage
      int	chars,		// Length of string to copy in characters
		bpc;		// Bytes per character

      if ((unsigned)(name->offset + name->length) > font->names.storage_size)
      {
        TTF_DEBUG("copy_name: offset(%d)+length(%d) > storage_size(%d)\n", name->offset, name->length, font->names.storage_size);
        continue;
      }

      if (name->platform_id == TTF_OFF_Windows && name->encoding_id == TTF_OFF_Windows_UCS2)
      {
        storptr = (char *)font->names.storage + name->offset;
        chars   = name->length / 2;
        bpc     = 2;
      }
      else if (name->platform_id == TTF_OFF_Windows && name->encoding_id == TTF_OFF_Windows_UCS4)
      {
        storptr = (char *)font->names.storage + name->offset;
        chars   = name->length / 4;
        bpc     = 4;
      }
      else
      {
        storptr = (char *)font->names.storage + name->offset;
        chars   = name->length;
        bpc     = 1;
      }

      for (tempptr = temp; chars > 0; storptr += bpc, chars --)
      {
        int ch;				// Current character

        // Convert to Unicode...
        if (bpc == 1)
          ch = *storptr;
	else if (bpc == 2)
	  ch = ((storptr[0] & 255) << 8) | (storptr[1] & 255);
	else
	  ch = ((storptr[0] & 255) << 24) | ((storptr[1] & 255) << 16) | ((storptr[2] & 255) << 8) | (storptr[3] & 255);

        // Convert to UTF-8...
        if (ch < 0x80)
        {
          // ASCII...
	  if (tempptr < (temp + sizeof(temp) - 1))
	    *tempptr++ = (char)ch;
	  else
	    break;
	}
	else if (ch < 0x400)
	{
	  // Two byte UTF-8
	  if (tempptr < (temp + sizeof(temp) - 2))
	  {
	    *tempptr++ = (char)(0xc0 | (ch >> 6));
	    *tempptr++ = (char)(0x80 | (ch & 0x3f));
	  }
	  else
	    break;
	}
	else if (ch < 0x10000)
	{
	  // Three byte UTF-8
	  if (tempptr < (temp + sizeof(temp) - 3))
	  {
	    *tempptr++ = (char)(0xe0 | (ch >> 12));
	    *tempptr++ = (char)(0x80 | ((ch >> 6) & 0x3f));
	    *tempptr++ = (char)(0x80 | (ch & 0x3f));
	  }
	  else
	    break;
	}
	else
	{
	  // Four byte UTF-8
	  if (tempptr < (temp + sizeof(temp) - 4))
	  {
	    *tempptr++ = (char)(0xf0 | (ch >> 18));
	    *tempptr++ = (char)(0x80 | ((ch >> 12) & 0x3f));
	    *tempptr++ = (char)(0x80 | ((ch >> 6) & 0x3f));
	    *tempptr++ = (char)(0x80 | (ch & 0x3f));
	  }
	  else
	    break;
	}
      }

      *tempptr = '\0';

      TTF_DEBUG("copy_name: name_id(%d) = \"%s\"\n", name_id, temp);

      return (strdup(temp));
    }
  }

  TTF_DEBUG("copy_name: No English name string for %d.\n", name_id);
#ifdef DEBUG
  for (i = font->names.num_names, name = font->names.names; i > 0; i --, name ++)
  {
    if (name->name_id == name_id)
      TTF_DEBUG("copy_name: Found name_id=%d, platform_id=%d, language_id=%d(0x%04x)\n", name_id, name->platform_id, name->language_id, name->language_id);
  }
#endif // DEBUG

  return (NULL);
}


//
// 'errorf()' - Show an error message.
//

static void
errorf(ttf_t      *font,		// I - Font
       const char *message,		// I - `printf`-style message string
       ...)				// I - Addition arguments as needed
{
  va_list	ap;			// Argument pointer
  char		buffer[2048];		// Message buffer


  // Format the message...
  va_start(ap, message);
  vsnprintf(buffer, sizeof(buffer), message, ap);
  va_end(ap);

  // If an error callback is set, sent the message there.  Otherwise write it
  // to stderr...
  if (font && font->err_cb)
    (font->err_cb)(font->err_data, buffer);
  else
    fprintf(stderr, "%s\n", message);
}


/*
 * 'read_cmap()' - Read the cmap table, getting the Unicode mapping table.
 */

static bool				// O - `true` on success, `false` on error
read_cmap(ttf_t *font)			// I - Font
{
  unsigned	length;			// Length of cmap table
  int		i,			// Looping var
		temp,			// Temporary value
		num_tables,		// Number of cmap tables
		platform_id,		// Platform identifier (Windows or Mac)
		encoding_id,		// Encoding identifier (varies)
		cformat;		// Formap of cmap data
  unsigned	clength,		// Length of cmap data
		coffset = 0,		// Offset to cmap data
		roman_offset = 0;	// MacRoman offset
  int		*cmapptr;		// Pointer into cmap
#if 0
  const int	*unimap = NULL;		// Unicode character map, if any
  static const int romanmap[256] =	// MacRoman to Unicode map
  {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
        0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
        0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0xC4, 0xC5, 0xC7, 0xC9, 0xD1, 0xD6, 0xDC, 0xE1,
        0xE0, 0xE2, 0xE4, 0xE3, 0xE5, 0xE7, 0xE9, 0xE8,
    0xEA, 0xEB, 0xED, 0xEC, 0xEE, 0xEF, 0xF1, 0xF3,
        0xF2, 0xF4, 0xF6, 0xF5, 0xFA, 0xF9, 0xFB, 0xFC,
    0x2020, 0xB0, 0xA2, 0xA3, 0xA7, 0x2022, 0xB6, 0xDF,
        0xAE, 0xA9, 0x2122, 0xB4, 0xA8, 0x2260, 0xC6, 0xD8,
    0x221E, 0xB1, 0x2264, 0x2265, 0xA5, 0xB5, 0x2202, 0x2211,
        0x220F, 0x3C0, 0x222B, 0xAA, 0xBA, 0x3A9, 0xE6, 0xF8,
    0xBF, 0xA1, 0xAC, 0x221A, 0x192, 0x2248, 0x2206, 0xAB,
        0xBB, 0x2026, 0xA0, 0xC0, 0xC3, 0xD5, 0x152, 0x153,
    0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0xF7, 0x25CA,
        0xFF, 0x178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02,
    0x2021, 0xB7, 0x201A, 0x201E, 0x2030, 0xC2, 0xCA, 0xC1,
        0xCB, 0xC8, 0xCD, 0xCE, 0xCF, 0xCC, 0xD3, 0xD4,
    0xF8FF, 0xD2, 0xDA, 0xDB, 0xD9, 0x131, 0x2C6, 0x2DC,
        0xAF, 0x2D8, 0x2D9, 0x2DA, 0xB8, 0x2DD, 0x2DB, 0x2C7
  };
#endif /* 0 */


  // Find the cmap table...
  if (seek_table(font, TTF_OFF_cmap, 0, true) == 0)
    return (false);

  if ((temp = read_ushort(font)) != 0)
  {
    errorf(font, "Unknown cmap version %d.", temp);
    return (false);
  }

  if ((num_tables = read_ushort(font)) < 1)
  {
    errorf(font, "No cmap tables to read.");
    return (false);
  }

  TTF_DEBUG("read_cmap: num_tables=%d\n", num_tables);

  // Find a Unicode table we can use...
  for (i = 0; i < num_tables; i ++)
  {
    platform_id = read_ushort(font);
    encoding_id = read_ushort(font);
    coffset     = read_ulong(font);

    TTF_DEBUG("read_cmap: table[%d].platform_id=%d, encoding_id=%d, coffset=%u\n", i, platform_id, encoding_id, coffset);

    if (platform_id == TTF_OFF_Unicode || (platform_id == TTF_OFF_Windows && encoding_id == TTF_OFF_Windows_UCS2))
      break;

    if (platform_id == TTF_OFF_Mac && encoding_id == TTF_OFF_Mac_Roman)
      roman_offset = coffset;
  }

  if (i >= num_tables)
  {
    if (roman_offset)
    {
      TTF_DEBUG("read_cmap: Using MacRoman cmap table.\n");
      coffset = roman_offset;
    }
    else
    {
      errorf(font, "No usable cmap table.");
      return (false);
    }
  }

  if ((length = seek_table(font, TTF_OFF_cmap, coffset, true)) == 0)
    return (false);

  if ((cformat = read_ushort(font)) < 0)
  {
    errorf(font, "Unable to read cmap table format at offset %u.", coffset);
    return (false);
  }

  TTF_DEBUG("read_cmap: cformat=%d\n", cformat);

  switch (cformat)
  {
    case 0 :
        {
          // Format 0: Byte encoding table.
          //
          // This is a simple 8-bit mapping.
          size_t	j;		// Looping var
          unsigned char bmap[256];	// Byte map buffer

	  if ((unsigned)read_ushort(font) == (unsigned)-1)
	  {
	    errorf(font, "Unable to read cmap table length at offset %u.", coffset);
	    return (false);
	  }

          /* language = */ read_ushort(font);

          if (length > (256 + 6))
          {
	    errorf(font, "Bad cmap table length at offset %u.", coffset);
	    return (false);
          }

	  font->num_cmap = length - 6;

	  if ((font->cmap = (int *)malloc(font->num_cmap * sizeof(int))) == NULL)
	  {
	    errorf(font, "Unable to allocate cmap table.");
	    return (false);
	  }

          if (read(font->fd, bmap, font->num_cmap) != (ssize_t)font->num_cmap)
          {
	    errorf(font, "Unable to read cmap table length at offset %u.", coffset);
	    return (false);
          }

	  // Copy into the actual cmap table...
	  for (j = 0; j < font->num_cmap; j ++)
	    font->cmap[j] = bmap[j];
        }
        break;

    case 4 :
        {
          // Format 4: Segment mapping to delta values.
          //
          // This is an overly complicated linear way of encoding a sparse
          // mapping table.  And it uses 1-based indexing with modulo
          // arithmetic...
          int		ch,		// Current character
			seg,		// Current segment
			glyph,		// Current glyph
			segCount,	// Number of segments
			numGlyphIdArray,// Number of glyph IDs
			*glyphIdArray;	// Glyph IDs
          _ttf_off_cmap4_t *segments,	// Segment data
			*segment;	// This segment


          // Read the table...
	  if ((clength = (unsigned)read_ushort(font)) == (unsigned)-1)
	  {
	    errorf(font, "Unable to read cmap table length at offset %u.", coffset);
	    return (false);
	  }

	  TTF_DEBUG("read_cmap: clength=%u\n", clength);

          /* language = */       read_ushort(font);
          segCount             = read_ushort(font) / 2;
	  /* searchRange = */    read_ushort(font);
	  /* entrySelectoed = */ read_ushort(font);
	  /* rangeShift = */     read_ushort(font);

          TTF_DEBUG("read_cmap: segCount=%d\n", segCount);

          if (segCount < 2)
          {
	    errorf(font, "Bad cmap table.");
	    return (false);
          }

          numGlyphIdArray = ((int)clength - 8 * segCount - 16) / 2;
          segments        = (_ttf_off_cmap4_t *)calloc((size_t)segCount, sizeof(_ttf_off_cmap4_t));
          glyphIdArray    = (int *)calloc((size_t)numGlyphIdArray, sizeof(int));

          if (!segments || !glyphIdArray)
          {
            errorf(font, "Unable to allocate memory for cmap.");
            free(segments);
            free(glyphIdArray);
            return (false);
	  }

          TTF_DEBUG("read_cmap: numGlyphIdArray=%d\n", numGlyphIdArray);

          for (i = 0; i < segCount; i ++)
            segments[i].endCode = (unsigned short)read_ushort(font);

	  /* reservedPad = */ read_ushort(font);

          for (i = 0; i < segCount; i ++)
            segments[i].startCode = (unsigned short)read_ushort(font);

          for (i = 0; i < segCount; i ++)
            segments[i].idDelta = (short)read_short(font);

          for (i = 0; i < segCount; i ++)
            segments[i].idRangeOffset = (unsigned short)read_ushort(font);

          for (i = 0; i < numGlyphIdArray; i ++)
            glyphIdArray[i] = read_ushort(font);

#ifdef DEBUG
          for (i = 0; i < segCount; i ++)
            TTF_DEBUG("read_cmap: segment[%d].startCode=%d, endCode=%d, idDelta=%d, idRangeOffset=%d\n", i, segments[i].startCode, segments[i].endCode, segments[i].idDelta, segments[i].idRangeOffset);

          for (i = 0; i < numGlyphIdArray; i ++)
            TTF_DEBUG("read_cmap: glyphIdArray[%d]=%d\n", i, glyphIdArray[i]);
#endif /* DEBUG */

          // Based on the end code of the segent table, allocate space for the
          // uncompressed cmap table...
          segCount --;			// Last segment is not used (sigh)

	  font->num_cmap = segments[segCount - 1].endCode + 1;
	  font->cmap     = cmapptr = (int *)malloc(font->num_cmap * sizeof(int));

	  if (!font->cmap)
          {
            errorf(font, "Unable to allocate memory for cmap.");
            free(segments);
            free(glyphIdArray);
            return (false);
	  }

          memset(cmapptr, -1, font->num_cmap * sizeof(int));

          // Now loop through the segments and assign glyph indices from the
          // array...
          for (seg = segCount, segment = segments; seg > 0; seg --, segment ++)
          {
            for (ch = segment->startCode; ch <= segment->endCode; ch ++)
            {
              if (segment->idRangeOffset)
              {
                // Use an "obscure indexing trick" (words from the spec, not
                // mine) to look up the glyph index...
                temp = segment->idRangeOffset / 2 + ch - segment->startCode + seg - segCount - 1;

                if (temp < 0 || temp >= numGlyphIdArray || !glyphIdArray[temp])
                  glyph = -1;
		else
		  glyph = ((glyphIdArray[temp] + segment->idDelta) & 65535) - 1;
              }
              else
              {
                // Just use idDelta to compute a glyph index...
                glyph = ((ch + segment->idDelta) & 65535) - 1;
	      }

	      cmapptr[ch] = glyph;
            }
	  }

          // Free the segment data...
	  free(segments);
	  free(glyphIdArray);
        }
        break;

    case 12 :
	{
	  // Format 12: Segmented coverage
	  //
	  // A simple sparse linear segment mapping format.
	  unsigned	ch,		// Current character
			gidx,		// Current group
			nGroups;	// Number of groups
	  _ttf_off_cmap12_t *groups,	// Groups
			*group;		// This group

	  // Read the table...
          /* reserved */ read_ushort(font);

	  if (read_ulong(font) == 0)
	  {
	    errorf(font, "Unable to read cmap table length at offset %u.", coffset);
	    return (false);
	  }

	  /* language = */ read_ulong(font);
	  nGroups        = read_ulong(font);

	  TTF_DEBUG("read_cmap: nGroups=%u\n", nGroups);

	  if ((groups = (_ttf_off_cmap12_t *)calloc(nGroups, sizeof(_ttf_off_cmap12_t))) == NULL)
          {
            errorf(font, "Unable to allocate memory for cmap.");
            return (false);
	  }

	  for (gidx = 0, group = groups, font->num_cmap = 0; gidx < nGroups; gidx ++, group ++)
	  {
	    group->startCharCode = read_ulong(font);
	    group->endCharCode   = read_ulong(font);
	    group->startGlyphID  = read_ulong(font);
	    TTF_DEBUG("read_cmap: [%u] startCharCode=%u, endCharCode=%u, startGlyphID=%u\n", gidx, group->startCharCode, group->endCharCode, group->startGlyphID);

            if (group->endCharCode >= font->num_cmap)
              font->num_cmap = group->endCharCode + 1;
	  }

	  // Based on the end code of the segent table, allocate space for the
	  // uncompressed cmap table...
          TTF_DEBUG("read_cmap: num_cmap=%u\n", (unsigned)font->num_cmap);
	  font->cmap = cmapptr = (int *)malloc(font->num_cmap * sizeof(int));

	  if (!font->cmap)
          {
            errorf(font, "Unable to allocate memory for cmap.");
            free(groups);
            return (false);
	  }

	  memset(cmapptr, -1, font->num_cmap * sizeof(int));

	  // Now loop through the groups and assign glyph indices from the
	  // array...
	  for (gidx = 0, group = groups; gidx < nGroups; gidx ++, group ++)
	  {
            for (ch = group->startCharCode; ch <= group->endCharCode && ch < TTF_FONT_MAX_CHAR; ch ++)
              cmapptr[ch] = (int)(group->startGlyphID + ch - group->startCharCode);
          }

	  // Free the group data...
	  free(groups);
	}
        break;

    case 13 :
	{
	  // Format 13: Many-to-one range mappings
	  //
	  // Typically used for fonts of last resort where multiple characters
	  // map to the same glyph.
	  unsigned	ch,		// Current character
			gidx,		// Current group
			nGroups;	// Number of groups
	  _ttf_off_cmap13_t *groups,	// Groups
			*group;		// This group

	  // Read the table...
          /* reserved */ read_ushort(font);

	  if (read_ulong(font) == 0)
	  {
	    errorf(font, "Unable to read cmap table length at offset %u.", coffset);
	    return (false);
	  }

	  /* language = */ read_ulong(font);
	  nGroups        = read_ulong(font);

	  TTF_DEBUG("read_cmap: nGroups=%u\n", nGroups);

	  if ((groups = (_ttf_off_cmap13_t *)calloc(nGroups, sizeof(_ttf_off_cmap13_t))) == NULL)
	  {
	    errorf(font, "Unable to allocate memory for cmap.");
	    return (false);
	  }

	  for (gidx = 0, group = groups, font->num_cmap = 0; gidx < nGroups; gidx ++, group ++)
	  {
	    group->startCharCode = read_ulong(font);
	    group->endCharCode   = read_ulong(font);
	    group->glyphID       = read_ulong(font);
	    TTF_DEBUG("read_cmap: [%u] startCharCode=%u, endCharCode=%u, glyphID=%u\n", gidx, group->startCharCode, group->endCharCode, group->glyphID);

            if (group->endCharCode >= font->num_cmap)
              font->num_cmap = group->endCharCode + 1;
	  }

	  // Based on the end code of the segent table, allocate space for the
	  // uncompressed cmap table...
          TTF_DEBUG("read_cmap: num_cmap=%u\n", (unsigned)font->num_cmap);
	  font->cmap = cmapptr = (int *)malloc(font->num_cmap * sizeof(int));

	  if (!font->cmap)
	  {
	    errorf(font, "Unable to allocate cmap.");
	    free(groups);
	    return (false);
	  }

	  memset(cmapptr, -1, font->num_cmap * sizeof(int));

	  // Now loop through the groups and assign glyph indices from the
	  // array...
	  for (gidx = 0, group = groups; gidx < nGroups; gidx ++, group ++)
	  {
            for (ch = group->startCharCode; ch <= group->endCharCode && ch < TTF_FONT_MAX_CHAR; ch ++)
              cmapptr[ch] = (int)group->glyphID;
          }

	  // Free the group data...
	  free(groups);
	}
        break;

    default :
        errorf(font, "Format %d cmap tables are not yet supported.", cformat);
        return (false);
  }

#ifdef DEBUG
  cmapptr = font->cmap;
  for (i = 0; i < (int)font->num_cmap && i < 127; i ++)
  {
    if (cmapptr[i] >= 0)
      TTF_DEBUG("read_cmap; cmap[%d]=%d\n", i, cmapptr[i]);
  }
#endif // DEBUG

  return (true);
}


//
// 'read_head()' - Read the head table.
//

static bool				// O - `true` on success, `false` on error
read_head(ttf_t           *font,	// I - Font
	  _ttf_off_head_t *head)	// O - head table data
{
  memset(head, 0, sizeof(_ttf_off_head_t));

  if (seek_table(font, TTF_OFF_head, 0, true) == 0)
    return (false);

  /* majorVersion */       read_ushort(font);
  /* minorVersion */       read_ushort(font);
  /* fontRevision */       read_ulong(font);
  /* checkSumAdjustment */ read_ulong(font);
  /* magicNumber */        read_ulong(font);
  /* flags */              read_ushort(font);
  head->unitsPerEm       = (unsigned short)read_ushort(font);
  /* created */            read_ulong(font); read_ulong(font);
  /* modified */           read_ulong(font); read_ulong(font);
  head->xMin             = (short)read_short(font);
  head->yMin             = (short)read_short(font);
  head->xMax             = (short)read_short(font);
  head->yMax             = (short)read_short(font);
  head->macStyle         = (unsigned short)read_ushort(font);

  return (true);
}


//
// 'read_hhea()' - Read the hhea table.
//

static bool				// O - `true` on success, `false` on error
read_hhea(ttf_t           *font,	// I - Font
          _ttf_off_hhea_t *hhea)	// O - hhea table data
{
  memset(hhea, 0, sizeof(_ttf_off_hhea_t));

  if (seek_table(font, TTF_OFF_hhea, 0, true) == 0)
    return (false);

  /* majorVersion */        read_ushort(font);
  /* minorVersion */        read_ushort(font);
  hhea->ascender          = (short)read_short(font);
  hhea->descender         = (short)read_short(font);
  /* lineGap */             read_short(font);
  /* advanceWidthMax */     read_ushort(font);
  /* minLeftSideBearing */  read_short(font);
  /* minRightSideBearing */ read_short(font);
  /* mMaxExtent */          read_short(font);
  /* caretSlopeRise */      read_short(font);
  /* caretSlopeRun */       read_short(font);
  /* caretOffset */         read_short(font);
  /* (reserved) */          read_short(font);
  /* (reserved) */          read_short(font);
  /* (reserved) */          read_short(font);
  /* (reserved) */          read_short(font);
  /* metricDataFormat */    read_short(font);
  hhea->numberOfHMetrics  = (unsigned short)read_ushort(font);

  return (true);
}


/*
 * 'read_hmtx()' - Read the horizontal metrics from the font.
 */

static _ttf_metric_t *			// O - Array of glyph metrics
read_hmtx(ttf_t           *font,	// I - Font
          _ttf_off_hhea_t *hhea)	// O - hhea table data
{
  unsigned	length;			// Length of hmtx table
  int		i;			// Looping var
  _ttf_metric_t	*widths;		// Glyph metrics array


  if ((length = seek_table(font, TTF_OFF_hmtx, 0, true)) == 0)
    return (NULL);

  if (length < (unsigned)(4 * hhea->numberOfHMetrics))
  {
    errorf(font, "Length of hhea table is only %u, expected at least %d.", length, 4 * hhea->numberOfHMetrics);
    return (NULL);
  }

  if ((widths = (_ttf_metric_t *)calloc((size_t)hhea->numberOfHMetrics, sizeof(_ttf_metric_t))) == NULL)
    return (NULL);

  for (i = 0; i < hhea->numberOfHMetrics; i ++)
  {
    widths[i].width        = (short)read_ushort(font);
    widths[i].left_bearing = (short)read_short(font);
  }

  return (widths);
}


//
// 'read_maxp()' - Read the number of glyphs in the font.
//

static int				// O - Number of glyphs or -1 on error
read_maxp(ttf_t *font)			// I - Font
{
  // All we care about is the number of glyphs, so just grab that...
  if (seek_table(font, TTF_OFF_maxp, 4, true) == 0)
    return (-1);
  else
    return (read_ushort(font));
}


//
// 'read_names()' - Read the name strings from a font.
//

static bool				// O - `true` on success, `false` on error
read_names(ttf_t *font)			// I - Font
{
  unsigned	length;			// Length of names table
  int		i,			// Looping var
		format,			// Name table format
		offset;			// Offset to storage
  _ttf_off_name_t *name;		// Current name


  // Find the name table...
  if ((length = seek_table(font, TTF_OFF_name, 0, true)) == 0)
    return (false);

  if ((format = read_ushort(font)) < 0 || format > 1)
  {
    errorf(font, "Unsupported name table format %d.", format);
    return (false);
  }

  TTF_DEBUG("read_names: format=%d\n", format);

  if ((font->names.num_names = read_ushort(font)) < 1)
    return (false);

  if ((font->names.names = (_ttf_off_name_t *)calloc((size_t)font->names.num_names, sizeof(_ttf_off_name_t))) == NULL)
    return (false);

  if ((offset = read_ushort(font)) < 0 || (unsigned)offset >= length)
    return (false);

  font->names.storage_size = length - (unsigned)offset;
  if ((font->names.storage = malloc(font->names.storage_size)) == NULL)
    return (false);
  memset(font->names.storage, 'A', font->names.storage_size);

  for (i = font->names.num_names, name = font->names.names; i > 0; i --, name ++)
  {
    name->platform_id = (unsigned short)read_ushort(font);
    name->encoding_id = (unsigned short)read_ushort(font);
    name->language_id = (unsigned short)read_ushort(font);
    name->name_id     = (unsigned short)read_ushort(font);
    name->length      = (unsigned short)read_ushort(font);
    name->offset      = (unsigned short)read_ushort(font);

    TTF_DEBUG("name->platform_id=%d, encoding_id=%d, language_id=%d(0x%04x), name_id=%d, length=%d, offset=%d\n", name->platform_id, name->encoding_id, name->language_id, name->language_id, name->name_id, name->length, name->offset);
  }

  if (format == 1)
  {
    // Skip language_id table...
    int count = read_ushort(font);	// Number of language IDs

    TTF_DEBUG("read_names: Skipping language_id table...\n");

    while (count > 0)
    {
      /* length = */read_ushort(font);
      /* offset = */read_ushort(font);
      count --;
    }
  }

  length -= (unsigned)offset;

  if (read(font->fd, font->names.storage, length) < 0)
  {
    errorf(font, "Unable to read name table: %s", strerror(errno));
    return (false);
  }

  return (true);
}


//
// 'read_os_2()' - Read the OS/2 table.
//

static bool				// O - `true` on success, `false` on error
read_os_2(ttf_t           *font,	// I - Font
          _ttf_off_os_2_t *os_2)	// O - OS/2 table
{
  int		version;		// OS/2 table version
  unsigned char	panose[10];		// panose value


  memset(os_2, 0, sizeof(_ttf_off_os_2_t));

  // Find the OS/2 table...
  if (seek_table(font, TTF_OFF_OS_2, 0, false) == 0)
    return (false);

  if ((version = read_ushort(font)) < 0)
    return (false);

  TTF_DEBUG("read_names: version=%d\n", version);

  /* xAvgCharWidth */       read_short(font);
  os_2->usWeightClass     = (unsigned short)read_ushort(font);
  os_2->usWidthClass      = (unsigned short)read_ushort(font);
  os_2->fsType            = (unsigned short)read_ushort(font);
  /* ySubscriptXSize */     read_short(font);
  /* ySubscriptYSize */     read_short(font);
  /* ySubscriptXOffset */   read_short(font);
  /* ySubscriptYOffset */   read_short(font);
  /* ySuperscriptXSize */   read_short(font);
  /* ySuperscriptYSize */   read_short(font);
  /* ySuperscriptXOffset */ read_short(font);
  /* ySuperscriptYOffset */ read_short(font);
  /* yStrikeoutSize */      read_short(font);
  /* yStrikeoutOffset */    read_short(font);
  /* sFamilyClass */        read_short(font);
  /* panose[10] */
  if (read(font->fd, panose, sizeof(panose)) != (ssize_t)sizeof(panose))
    return (false);
  /* ulUnicodeRange1 */     read_ulong(font);
  /* ulUnicodeRange2 */     read_ulong(font);
  /* ulUnicodeRange3 */     read_ulong(font);
  /* ulUnicodeRange4 */     read_ulong(font);
  /* achVendID */           read_ulong(font); read_ulong(font);
                            read_ulong(font); read_ulong(font);
  /* fsSelection */         read_ushort(font);
  /* usFirstCharIndex */    read_ushort(font);
  /* usLastCharIndex */     read_ushort(font);
  os_2->sTypoAscender     = (short)read_short(font);
  os_2->sTypoDescender    = (short)read_short(font);
  /* sTypoLineGap */        read_short(font);
  /* usWinAscent */         read_ushort(font);
  /* usWinDescent */        read_ushort(font);

  if (version >= 4)
  {
    /* ulCodePageRange1 */  read_ulong(font);
    /* ulCodePageRange2 */  read_ulong(font);
    os_2->sxHeight        = (short)read_short(font);
    os_2->sCapHeight      = (short)read_short(font);
  }

  return (true);
}


//
// 'read_post()' - Read the PostScript table.
//

static bool				// O - `true` on success, `false` otherwise
read_post(ttf_t           *font,	// I - Font
          _ttf_off_post_t *post)	// I - PostScript table
{
  memset(post, 0, sizeof(_ttf_off_post_t));

  if (seek_table(font, TTF_OFF_post, 0, false) == 0)
    return (false);

  /* version            = */read_ulong(font);
  post->italicAngle     = (int)read_ulong(font) / 65536.0f;
  /* underlinePosition  = */read_ushort(font);
  /* underlineThickness = */read_ushort(font);
  post->isFixedPitch    = read_ulong(font);

  return (true);
}


//
// 'read_short()' - Read a 16-bit signed integer.
//

static int				// O - 16-bit signed integer value or EOF
read_short(ttf_t *font)			// I - Font
{
  unsigned char	buffer[2];		// Read buffer


  if (read(font->fd, buffer, sizeof(buffer)) != sizeof(buffer))
    return (EOF);
  else if (buffer[0] & 0x80)
    return (((buffer[0] << 8) | buffer[1]) - 65536);
  else
    return ((buffer[0] << 8) | buffer[1]);
}


//
// 'read_table()' - Read an OFF/TTF offset table.
//

static bool				// O - `true` on success, `false` on error
read_table(ttf_t  *font)		// I - Font
{
  int		i;			// Looping var
  unsigned	temp;			// Temporary value
  _ttf_off_dir_t *current;		// Current table entry


  // Read the table header:
  //
  //     Fixed  sfnt version (should be 0x10000 for version 1.0)
  //     USHORT numTables
  //     USHORT searchRange
  //     USHORT entrySelector
  //     USHORT rangeShift
  /* sfnt version */
  if ((temp = read_ulong(font)) != 0x10000 && temp != 0x4f54544f && temp != 0x74746366)
  {
    errorf(font, "Invalid font file.");
    return (false);
  }

  if (temp == 0x74746366)
  {
    // Font collection, get the number of fonts and then seek to the start of
    // the offset table for the desired font...
    size_t	idx;			// Current index

    TTF_DEBUG("read_table: Font collection\n");

    /* Version */
    if ((temp = read_ulong(font)) != 0x10000 && temp != 0x20000)
    {
      errorf(font, "Unsupported font collection version %f.", temp / 65536.0);
      return (false);
    }

    TTF_DEBUG("read_table: Collection version=%f\n", temp / 65536.0);

    /* numFonts */
    if ((temp = read_ulong(font)) == 0)
    {
      errorf(font, "No fonts in collection.");
      return (false);
    }

    font->num_fonts = (size_t)temp;

    TTF_DEBUG("read_table: numFonts=%u\n", temp);

    if (font->idx >= font->num_fonts)
      return (false);

    /* OffsetTable */
    temp = read_ulong(font);
    for (idx = font->idx; idx > 0; idx --)
      temp = read_ulong(font);

    TTF_DEBUG("read_table: Offset for font %u is %u.\n", (unsigned)font->idx, temp);

    if (lseek(font->fd, temp + 4, SEEK_SET) < 0)
    {
      errorf(font, "Unable to seek to font %u: %s", (unsigned)font->idx, strerror(errno));
      return (false);
    }
  }
  else
  {
    // Not a collection so just one font...
    font->num_fonts = 1;
  }

  // numTables
  if ((font->table.num_entries = read_ushort(font)) <= 0)
  {
    errorf(font, "Unable to read font tables.");
    return (false);
  }

  TTF_DEBUG("read_table: num_entries=%u\n", (unsigned)font->table.num_entries);

  // searchRange
  if (read_ushort(font) < 0)
  {
    errorf(font, "Unable to read font tables.");
    return (false);
  }

  // entrySelector
  if (read_ushort(font) < 0)
  {
    errorf(font, "Unable to read font tables.");
    return (false);
  }

  // rangeShift
  if (read_ushort(font) < 0)
  {
    errorf(font, "Unable to read font tables.");
    return (false);
  }

 /*
  * Read the table entries...
  */

  if ((font->table.entries = calloc((size_t)font->table.num_entries, sizeof(_ttf_off_dir_t))) == NULL)
  {
    errorf(font, "Unable to allocate memory for font tables.");
    return (false);
  }

  for (i = font->table.num_entries, current = font->table.entries; i > 0; i --, current ++)
  {
    current->tag      = read_ulong(font);
    current->checksum = read_ulong(font);
    current->offset   = read_ulong(font);
    current->length   = read_ulong(font);

    TTF_DEBUG("read_table: [%d] tag='%c%c%c%c' checksum=%u offset=%u length=%u\n", i, (current->tag >> 24) & 255, (current->tag >> 16) & 255, (current->tag >> 8) & 255, current->tag & 255, current->checksum, current->offset, current->length);
  }

  return (true);
}


//
// 'read_ulong()' - Read a 32-bit unsigned integer.
//

static unsigned				// O - 32-bit unsigned integer value or EOF
read_ulong(ttf_t *font)			// I - Font
{
  unsigned char	buffer[4];		// Read buffer


  if (read(font->fd, buffer, sizeof(buffer)) != sizeof(buffer))
    return ((unsigned)EOF);
  else
    return ((unsigned)((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]));
}


//
// 'read_ushort()' - Read a 16-bit unsigned integer.
//

static int				// O - 16-bit unsigned integer value or EOF
read_ushort(ttf_t *font)		// I - Font
{
  unsigned char	buffer[2];		// Read buffer


  if (read(font->fd, buffer, sizeof(buffer)) != sizeof(buffer))
    return (EOF);
  else
    return ((buffer[0] << 8) | buffer[1]);
}


//
// 'seek_table()' - Seek to a specific table in a font.
//

static unsigned				// O - Length of table or 0 if not found
seek_table(ttf_t    *font,		// I - Font
           unsigned tag,		// I - Tag to find
           unsigned offset,		// I - Additional offset
           bool     required)		// I - Required table?
{
  int		i;			// Looping var
  _ttf_off_dir_t *current;		// Current entry


  // Look up the tag in the table...
  for (i = font->table.num_entries, current = font->table.entries; i  > 0; i --, current ++)
  {
    if (current->tag == tag)
    {
      // Found it, seek and return...
      if (lseek(font->fd, current->offset + offset, SEEK_SET) == (current->offset + offset))
      {
        // Successful seek...
        return (current->length - offset);
      }
      else
      {
        // Seek failed...
        errorf(font, "Unable to seek to %c%c%c%c table: %s", (tag >> 24) & 255, (tag >> 16) & 255, (tag >> 8) & 255, tag & 255, strerror(errno));
        return (0);
      }
    }
  }

  // Not found, return 0...
  if (required)
    errorf(font, "%c%c%c%c table not found.", (tag >> 24) & 255, (tag >> 16) & 255, (tag >> 8) & 255, tag & 255);

  return (0);
}
