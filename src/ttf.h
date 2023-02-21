//
// Header file for TTF library
//
//     https://github.com/michaelrsweet/ttf
//
// Copyright © 2018-2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef TTF_H
#  define TTF_H

//
// Include necessary headers...
//

#  include <stdbool.h>
#  include <sys/types.h>

#  ifdef __cplusplus
extern "C" {
#  endif //


//
// Types...
//

typedef struct _ttf_s ttf_t;	//// Font object

typedef void (*ttf_err_cb_t)(void *data, const char *message);
				//// Font error callback

typedef enum ttf_stretch_e	//// Font stretch
{
  TTF_STRETCH_NORMAL,		// normal
  TTF_STRETCH_ULTRA_CONDENSED,	// ultra-condensed
  TTF_STRETCH_EXTRA_CONDENSED,	// extra-condensed
  TTF_STRETCH_CONDENSED,	// condensed
  TTF_STRETCH_SEMI_CONDENSED,	// semi-condensed
  TTF_STRETCH_SEMI_EXPANDED,	// semi-expanded
  TTF_STRETCH_EXPANDED,		// expanded
  TTF_STRETCH_EXTRA_EXPANDED,	// extra-expanded
  TTF_STRETCH_ULTRA_EXPANDED	// ultra-expanded
} ttf_stretch_t;

typedef enum ttf_style_e	//// Font style
{
  TTF_STYLE_NORMAL,		// Normal font
  TTF_STYLE_ITALIC,		// Italic font
  TTF_STYLE_OBLIQUE		// Oblique (angled) font
} ttf_style_t;

typedef enum ttf_variant_e	//// Font variant
{
  TTF_VARIANT_NORMAL,		// Normal font
  TTF_VARIANT_SMALL_CAPS	// Font whose lowercase letters are small capitals
} ttf_variant_t;

typedef enum ttf_weight_e	//// Font weight
{
  TTF_WEIGHT_100 = 100,		// Weight 100 (Thin)
  TTF_WEIGHT_200 = 200,		// Weight 200 (Extra/Ultra-Light)
  TTF_WEIGHT_300 = 300,		// Weight 300 (Light)
  TTF_WEIGHT_400 = 400,		// Weight 400 (Normal/Regular)
  TTF_WEIGHT_500 = 500,		// Weight 500 (Medium)
  TTF_WEIGHT_600 = 600,		// Weight 600 (Semi/Demi-Bold)
  TTF_WEIGHT_700 = 700,		// Weight 700 (Bold)
  TTF_WEIGHT_800 = 800,		// Weight 800 (Extra/Ultra-Bold)
  TTF_WEIGHT_900 = 900		// Weight 900 (Black/Heavy)
} ttf_weight_t;

typedef struct ttf_rect_s	//// Bounding rectangle
{
  float	left;			// Left offset
  float	top;			// Top offset
  float	right;			// Right offset
  float	bottom;			// Bottom offset
} ttf_rect_t;


//
// Functions...
//

extern ttf_t		*ttfCreate(const char *filename, size_t idx, ttf_err_cb_t err_cb, void *err_data);
extern void		ttfDelete(ttf_t *font);
extern int		ttfGetAscent(ttf_t *font);
extern ttf_rect_t	*ttfGetBounds(ttf_t *font, ttf_rect_t *bounds);
extern int		ttfGetCapHeight(ttf_t *font);
extern const int	*ttfGetCMap(ttf_t *font, size_t *num_cmap);
extern const char	*ttfGetCopyright(ttf_t *font);
extern int		ttfGetDescent(ttf_t *font);
extern ttf_rect_t	*ttfGetExtents(ttf_t *font, float size, const char *s, ttf_rect_t *extents);
extern const char	*ttfGetFamily(ttf_t *font);
extern float		ttfGetItalicAngle(ttf_t *font);
extern int		ttfGetMaxChar(ttf_t *font);
extern int		ttfGetMinChar(ttf_t *font);
extern size_t		ttfGetNumFonts(ttf_t *font);
extern const char	*ttfGetPostScriptName(ttf_t *font);
extern ttf_stretch_t	ttfGetStretch(ttf_t *font);
extern ttf_style_t	ttfGetStyle(ttf_t *font);
extern const char	*ttfGetVersion(ttf_t *font);
extern int		ttfGetWidth(ttf_t *font, int ch);
extern ttf_weight_t	ttfGetWeight(ttf_t *font);
extern int		ttfGetXHeight(ttf_t *font);
extern bool		ttfIsFixedPitch(ttf_t *font);


#  ifdef __cplusplus
}
#  endif // __cplusplus

#endif // !TTF_H
