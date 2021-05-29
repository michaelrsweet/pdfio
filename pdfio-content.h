//
// Public content header file for pdfio.
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef PDFIO_CONTENT_H
#  define PDFIO_CONTENT_H

//
// Include necessary headers...
//

#  include "pdfio.h"


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Types and constants...
//

typedef enum pdfio_linecap_e		// Line capping modes
{
  PDFIO_LINECAP_BUTT,			// Butt ends
  PDFIO_LINECAP_ROUND,			// Round ends
  PDFIO_LINECAP_SQUARE			// Square ends
} pdfio_linecap_t;

typedef enum pdfio_linejoin_e		// Line joining modes
{
  PDFIO_LINEJOIN_MITER,			// Miter joint
  PDFIO_LINEJOIN_ROUND,			// Round joint
  PDFIO_LINEJOIN_BEVEL			// Bevel joint
} pdfio_linejoin_t;

typedef float pdfio_matrix_t[3][2];	// Transform matrix

typedef enum pdfio_textrendering_e	// Text rendering modes
{
  PDFIO_TEXTRENDERING_FILL,		// Fill text
  PDFIO_TEXTRENDERING_STROKE,		// Stroke text
  PDFIO_TEXTRENDERING_FILL_AND_STROKE,	// Fill then stroke text
  PDFIO_TEXTRENDERING_INVISIBLE,	// Don't fill or stroke (invisible)
  PDFIO_TEXTRENDERING_FILL_PATH,	// Fill text and add to path
  PDFIO_TEXTRENDERING_STROKE_PATH,	// Stroke text and add to path
  PDFIO_TEXTRENDERING_FILL_AND_STROKE_PATH,
					// Fill then stroke text and add to path
  PDFIO_TEXTRENDERING_TEXT_PATH		// Add text to path (invisible)
} pdfio_textrendering_t;

extern const float	pdfioAdobeRGBGamma;
					// AdobeRGB gamma
extern const float	pdfioAdobeRGBWhitePoint[];
					// AdobeRGB white point
extern const float	pdfioDisplayP3Gamma;
					// Display P3 gamma
extern const float	pdfioDisplayP3WhitePoint[];
					// Display P3 white point
extern const float	pdfioSRGBGamma;	// sRGB gamma
extern const float	pdfioSRGBWhitePoint[];
					// sRGB white point


//
// Functions...
//

// PDF content drawing functions...
extern bool		pdfioContentBeginText(pdfio_stream_t *st) PDFIO_PUBLIC;
extern bool		pdfioContentClip(pdfio_stream_t *st, bool even_odd) PDFIO_PUBLIC;
extern bool		pdfioContentDrawImage(pdfio_stream_t *st, const char *name, float x, float y, float w, float h) PDFIO_PUBLIC;
extern bool		pdfioContentEndText(pdfio_stream_t *st) PDFIO_PUBLIC;
extern bool		pdfioContentFill(pdfio_stream_t *st, bool even_odd) PDFIO_PUBLIC;
extern bool		pdfioContentFillAndStroke(pdfio_stream_t *st, bool even_odd) PDFIO_PUBLIC;
extern bool		pdfioContentMatrixConcat(pdfio_stream_t *st, pdfio_matrix_t m) PDFIO_PUBLIC;
extern bool		pdfioContentMatrixRotate(pdfio_stream_t *st, float degrees) PDFIO_PUBLIC;
extern bool		pdfioContentMatrixScale(pdfio_stream_t *st, float sx, float sy) PDFIO_PUBLIC;
extern bool		pdfioContentMatrixTranslate(pdfio_stream_t *st, float tx, float ty) PDFIO_PUBLIC;
extern bool		pdfioContentPathClose(pdfio_stream_t *st) PDFIO_PUBLIC;
extern bool		pdfioContentPathCurve(pdfio_stream_t *st, float x1, float y1, float x2, float y2, float x3, float y3) PDFIO_PUBLIC;
extern bool		pdfioContentPathCurve13(pdfio_stream_t *st, float x1, float y1, float x3, float y3) PDFIO_PUBLIC;
extern bool		pdfioContentPathCurve23(pdfio_stream_t *st, float x2, float y2, float x3, float y3) PDFIO_PUBLIC;
extern bool		pdfioContentPathLineTo(pdfio_stream_t *st, float x, float y) PDFIO_PUBLIC;
extern bool		pdfioContentPathMoveTo(pdfio_stream_t *st, float x, float y) PDFIO_PUBLIC;
extern bool		pdfioContentPathRect(pdfio_stream_t *st, pdfio_rect_t *rect) PDFIO_PUBLIC;
extern bool		pdfioContentRestore(pdfio_stream_t *st) PDFIO_PUBLIC;
extern bool		pdfioContentSave(pdfio_stream_t *st) PDFIO_PUBLIC;
extern bool		pdfioContentSetDashPattern(pdfio_stream_t *st, int phase, int on, int off) PDFIO_PUBLIC;
extern bool		pdfioContentSetFillColorDeviceCMYK(pdfio_stream_t *st, float c, float m, float y, float k) PDFIO_PUBLIC;
extern bool		pdfioContentSetFillColorDeviceGray(pdfio_stream_t *st, float g) PDFIO_PUBLIC;
extern bool		pdfioContentSetFillColorDeviceRGB(pdfio_stream_t *st, float r, float g, float b) PDFIO_PUBLIC;
extern bool		pdfioContentSetFillColorGray(pdfio_stream_t *st, float g) PDFIO_PUBLIC;
extern bool		pdfioContentSetFillColorRGB(pdfio_stream_t *st, float r, float g, float b) PDFIO_PUBLIC;
extern bool		pdfioContentSetFillColorSpace(pdfio_stream_t *st, const char *name) PDFIO_PUBLIC;
extern bool		pdfioContentSetFlatness(pdfio_stream_t *st, float f) PDFIO_PUBLIC;
extern bool		pdfioContentSetLineCap(pdfio_stream_t *st, pdfio_linecap_t lc) PDFIO_PUBLIC;
extern bool		pdfioContentSetLineJoin(pdfio_stream_t *st, pdfio_linejoin_t lj) PDFIO_PUBLIC;
extern bool		pdfioContentSetLineWidth(pdfio_stream_t *st, float width) PDFIO_PUBLIC;
extern bool		pdfioContentSetMiterLimit(pdfio_stream_t *st, float limit) PDFIO_PUBLIC;
extern bool		pdfioContentSetStrokeColorDeviceCMYK(pdfio_stream_t *st, float c, float m, float y, float k) PDFIO_PUBLIC;
extern bool		pdfioContentSetStrokeColorDeviceGray(pdfio_stream_t *st, float g) PDFIO_PUBLIC;
extern bool		pdfioContentSetStrokeColorDeviceRGB(pdfio_stream_t *st, float r, float g, float b) PDFIO_PUBLIC;
extern bool		pdfioContentSetStrokeColorGray(pdfio_stream_t *st, float g) PDFIO_PUBLIC;
extern bool		pdfioContentSetStrokeColorRGB(pdfio_stream_t *st, float r, float g, float b) PDFIO_PUBLIC;
extern bool		pdfioContentSetStrokeColorSpace(pdfio_stream_t *st, const char *name) PDFIO_PUBLIC;
extern bool		pdfioContentSetTextCharacterSpacing(pdfio_stream_t *st, float spacing) PDFIO_PUBLIC;
extern bool		pdfioContentSetTextFont(pdfio_stream_t *st, const char *name, float size) PDFIO_PUBLIC;
extern bool		pdfioContentSetTextLeading(pdfio_stream_t *st, float leading) PDFIO_PUBLIC;
extern bool		pdfioContentSetTextMatrix(pdfio_stream_t *st, pdfio_matrix_t m) PDFIO_PUBLIC;
extern bool		pdfioContentSetTextRenderingMode(pdfio_stream_t *st, pdfio_textrendering_t mode) PDFIO_PUBLIC;
extern bool		pdfioContentSetTextRise(pdfio_stream_t *st, float rise) PDFIO_PUBLIC;
extern bool		pdfioContentSetTextWordSpacing(pdfio_stream_t *st, float spacing) PDFIO_PUBLIC;
extern bool		pdfioContentSetTextXScaling(pdfio_stream_t *st, float percent) PDFIO_PUBLIC;
extern bool		pdfioContentStroke(pdfio_stream_t *st) PDFIO_PUBLIC;
extern bool		pdfioContentTextMoveLine(pdfio_stream_t *st, float tx, float ty) PDFIO_PUBLIC;
extern bool		pdfioContentTextMoveTo(pdfio_stream_t *st, float tx, float ty) PDFIO_PUBLIC;
extern bool		pdfioContentTextNextLine(pdfio_stream_t *st) PDFIO_PUBLIC;
extern bool		pdfioContentTextShow(pdfio_stream_t *st, const char *s, bool new_line) PDFIO_PUBLIC;
extern bool		pdfioContentTextShowJustified(pdfio_stream_t *st, size_t num_fragments, const float *offsets, const char * const *fragments) PDFIO_PUBLIC;

// Resource helpers...
extern pdfio_obj_t	*pdfioFileCreateFontObject(pdfio_file_t *pdf, const char *filename) PDFIO_PUBLIC;
extern pdfio_obj_t	*pdfioFileCreateICCProfileObject(pdfio_file_t *pdf, const char *filename) PDFIO_PUBLIC;
extern pdfio_obj_t	*pdfioFileCreateImageObject(pdfio_file_t *pdf, const char *filename, bool interpolate) PDFIO_PUBLIC;

// Image object helpers...
extern float		pdfioImageGetHeight(pdfio_obj_t *obj) PDFIO_PUBLIC;
extern float		pdfioImageGetWidth(pdfio_obj_t *obj) PDFIO_PUBLIC;

// Page dictionary helpers...
extern bool		pdfioPageDictAddICCColorSpace(pdfio_dict_t *dict, const char *name, pdfio_obj_t *obj) PDFIO_PUBLIC;
extern bool		pdfioPageDictAddCalibratedColorSpace(pdfio_dict_t *dict, const char *name, size_t num_colors, const float *white_point, float gamma) PDFIO_PUBLIC;
extern bool		pdfioPageDictAddFont(pdfio_dict_t *dict, const char *name, pdfio_obj_t *obj) PDFIO_PUBLIC;
extern bool		pdfioPageDictAddImage(pdfio_dict_t *dict, const char *name, pdfio_obj_t *obj) PDFIO_PUBLIC;


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !PDFIO_CONTENT_H
