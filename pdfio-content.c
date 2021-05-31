//
// Content helper functions for PDFio.
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pdfio-content.h"
#include "pdfio-private.h"
#include <math.h>


//
// Global constants...
//

const double	pdfioAdobeRGBGamma = 2.2;
const double	pdfioAdobeRGBMatrix[3][3] = { { 0.57667, 0.18556, 0.18823 }, { 0.29734, 0.62736, 0.07529 }, { 0.02703, 0.07069, 0.99134 } };
const double	pdfioAdobeRGBWhitePoint[3] = { 0.9505, 1.0, 1.0890 };
const double	pdfioDisplayP3Gamma = 2.2;
const double	pdfioDisplayP3Matrix[3][3] = { { 0.48657, 0.26567, 0.19822 }, {
0.22897, 0.69174, 0.07929 }, { 0.00000, 0.04511, 1.04394 } };
const double	pdfioDisplayP3WhitePoint[3] = { 0.9505, 1.0, 1.0890 };
const double	pdfioSRGBGamma = 2.2;
const double	pdfioSRGBMatrix[3][3] = { { 0.4124, 0.3576, 0.1805 }, { 0.2126, 0.7152, 0.0722 }, { 0.0193, 0.1192, 0.9505 } };
const double	pdfioSRGBWhitePoint[3] = { 0.9505, 1.0, 1.0890 };

//
// Local types...
//

typedef pdfio_obj_t *(*_pdfio_image_func_t)(pdfio_dict_t *dict, int fd);


//
// Local functions...
//

static pdfio_obj_t	*copy_jpeg(pdfio_dict_t *dict, int fd);
static pdfio_obj_t	*copy_png(pdfio_dict_t *dict, int fd);
static pdfio_array_t	*create_calcolor(pdfio_file_t *pdf, size_t num_colors, double gamma, const double matrix[3][3], const double white_point[3]);
static bool		write_string(pdfio_stream_t *st, const char *s, bool *newline);


//
// 'pdfioContentClip()' - Clip output to the current path.
//

bool					// O - `true` on success, `false` on failure
pdfioContentClip(
    pdfio_stream_t *st,			// I - Stream
    bool           even_odd)		// I - Even/odd fill vs. non-zero winding rule
{
  return (pdfioStreamPuts(st, even_odd ? "W*\n" : "W\n"));
}


//
// 'pdfioContentDrawImage()' - Draw an image object.
//
// The object name must be part of the page dictionary resources, typically
// using the @link pdfioPageDictAddImage@ function.
//

bool					// O - `true` on success, `false` on failure
pdfioContentDrawImage(
    pdfio_stream_t *st,			// I - Stream
    const char     *name,		// I - Image name
    double         x,			// I - X offset of image
    double         y,			// I - Y offset of image
    double         width,		// I - Width of image
    double         height)		// I - Height of image
{
  return (pdfioStreamPrintf(st, "q %g 0 0 %g %g %g cm/%s Do Q\n", width, height, x, y, name));
}


//
// 'pdfioContentFill()' - Fill the current path.
//

bool					// O - `true` on success, `false` on failure
pdfioContentFill(
    pdfio_stream_t *st,			// I - Stream
    bool           even_odd)		// I - Even/odd fill vs. non-zero winding rule
{
  return (pdfioStreamPuts(st, even_odd ? "f*\n" : "f\n"));
}


//
// 'pdfioContentFillAndStroke()' - Fill and stroke the current path.
//

bool					// O - `true` on success, `false` on failure
pdfioContentFillAndStroke(
    pdfio_stream_t *st,			// I - Stream
    bool           even_odd)		// I - Even/odd fill vs. non-zero winding
{
  return (pdfioStreamPuts(st, even_odd ? "B*\n" : "B\n"));
}


//
// 'pdfioContentMatrixConcat()' - Concatenate a matrix to the current graphics
//                                state.
//

bool					// O - `true` on success, `false` on failure
pdfioContentMatrixConcat(
    pdfio_stream_t *st,			// I - Stream
    pdfio_matrix_t m)			// I - Transform matrix
{
  return (pdfioStreamPrintf(st, "%g %g %g %g %g %g cm\n", m[0][0], m[0][1], m[1][0], m[1][1], m[2][0], m[2][1]));
}


//
// 'pdfioContentMatrixRotate()' - Rotate the current transform matrix.
//

bool					// O - `true` on success, `false` on failure
pdfioContentMatrixRotate(
    pdfio_stream_t *st,			// I - Stream
    double         degrees)		// I - Rotation angle in degrees counter-clockwise
{
  double dcos = cos(degrees / M_PI);	// Cosine
  double dsin = sin(degrees / M_PI);	// Sine

  return (pdfioStreamPrintf(st, "%g %g %g %g 0 0 cm\n", dcos, -dsin, dsin, dcos));
}


//
// 'pdfioContentMatrixScale()' - Scale the current transform matrix.
//

bool					// O - `true` on success, `false` on failure
pdfioContentMatrixScale(
    pdfio_stream_t *st,			// I - Stream
    double         sx,			// I - X scale
    double         sy)			// I - Y scale
{
  return (pdfioStreamPrintf(st, "%g 0 0 %g 0 0 cm\n", sx, sy));
}


//
// 'pdfioContentMatrixTranslate()' - Translate the current transform matrix.
//

bool					// O - `true` on success, `false` on failure
pdfioContentMatrixTranslate(
    pdfio_stream_t *st,			// I - Stream
    double         tx,			// I - X offset
    double         ty)			// I - Y offset
{
  return (pdfioStreamPrintf(st, "1 0 0 1 %g %g cm\n", tx, ty));
}


//
// 'pdfioContentPathClose()' - Close the current path.
//

bool					// O - `true` on success, `false` on failure
pdfioContentPathClose(
    pdfio_stream_t *st)			// I - Stream
{
  return (pdfioStreamPuts(st, "h\n"));
}


//
// 'pdfioContentPathCurve()' - Add a Bezier curve with two control points.
//

bool					// O - `true` on success, `false` on failure
pdfioContentPathCurve(
    pdfio_stream_t *st,			// I - Stream
    double         x1,			// I - X position 1
    double         y1,			// I - Y position 1
    double         x2,			// I - X position 2
    double         y2,			// I - Y position 2
    double         x3,			// I - X position 3
    double         y3)			// I - Y position 3
{
  return (pdfioStreamPrintf(st, "%g %g %g %g %g %g c\n", x1, y1, x2, y2, x3, y3));
}


//
// 'pdfioContentPathCurve13()' - Add a Bezier curve with an initial control point.
//

bool					// O - `true` on success, `false` on failure
pdfioContentPathCurve13(
    pdfio_stream_t *st,			// I - Stream
    double         x1,			// I - X position 1
    double         y1,			// I - Y position 1
    double         x3,			// I - X position 3
    double         y3)			// I - Y position 3
{
  return (pdfioStreamPrintf(st, "%g %g %g %g v\n", x1, y1, x3, y3));
}


//
// 'pdfioContentPathCurve23()' - Add a Bezier curve with a trailing control point.
//

bool					// O - `true` on success, `false` on failure
pdfioContentPathCurve23(
    pdfio_stream_t *st,			// I - Stream
    double         x2,			// I - X position 2
    double         y2,			// I - Y position 2
    double         x3,			// I - X position 3
    double         y3)			// I - Y position 3
{
  return (pdfioStreamPrintf(st, "%g %g %g %g y\n", x2, y2, x3, y3));
}


//
// 'pdfioContentPathLineTo()' - Add a straight line to the current path.
//

bool					// O - `true` on success, `false` on failure
pdfioContentPathLineTo(
    pdfio_stream_t *st,			// I - Stream
    double         x,			// I - X position
    double         y)			// I - Y position
{
  return (pdfioStreamPrintf(st, "%g %g l\n", x, y));
}


//
// 'pdfioContentPathMoveTo()' - Start a new subpath.
//

bool					// O - `true` on success, `false` on failure
pdfioContentPathMoveTo(
    pdfio_stream_t *st,			// I - Stream
    double         x,			// I - X position
    double         y)			// I - Y position
{
  return (pdfioStreamPrintf(st, "%g %g m\n", x, y));
}


//
// 'pdfioContentPathRect()' - Add a rectangle to the current path.
//

bool					// O - `true` on success, `false` on failure
pdfioContentPathRect(
    pdfio_stream_t *st,			// I - Stream
    double         x,			// I - X offset
    double         y,			// I - Y offset
    double         width,		// I - Width
    double         height)		// I - Height
{
  return (pdfioStreamPrintf(st, "%g %g %g %g re\n", x, y, width, height));
}


//
// 'pdfioContentRestore()' - Restore a previous graphics state.
//

bool					// O - `true` on success, `false` on failure
pdfioContentRestore(
    pdfio_stream_t *st)			// I - Stream
{
  return (pdfioStreamPuts(st, "Q\n"));
}


//
// 'pdfioContentSave()' - Save the current graphics state.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSave(pdfio_stream_t *st)	// I - Stream
{
  return (pdfioStreamPuts(st, "q\n"));
}


//
// 'pdfioContentSetDashPattern()' - Set the stroke pattern.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetDashPattern(
    pdfio_stream_t *st,			// I - Stream
    int            phase,		// I - Phase (offset within pattern)
    int            on,			// I - On length
    int            off)			// I - Off length
{
  return (pdfioStreamPrintf(st, "[%d %d] %d d\n", on, off, phase));
}


//
// 'pdfioContentSetFillColorDeviceCMYK()' - Set device CMYK fill color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetFillColorDeviceCMYK(
    pdfio_stream_t *st,			// I - Stream
    double         c,			// I - Cyan value (0.0 to 1.0)
    double         m,			// I - Magenta value (0.0 to 1.0)
    double         y,			// I - Yellow value (0.0 to 1.0)
    double         k)			// I - Black value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%g %g %g %g k\n", c, m, y, k));
}


//
// 'pdfioContentSetFillColorDeviceGray()' - Set the device gray fill color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetFillColorDeviceGray(
    pdfio_stream_t *st,			// I - Stream
    double         g)			// I - Gray value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%g g\n", g));
}


//
// 'pdfioContentSetFillColorDeviceRGB()' - Set the device RGB fill color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetFillColorDeviceRGB(
    pdfio_stream_t *st,			// I - Stream
    double         r,			// I - Red value (0.0 to 1.0)
    double         g,			// I - Green value (0.0 to 1.0)
    double         b)			// I - Blue value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%g %g %g rg\n", r, g, b));
}


//
// 'pdfioContentSetFillColorGray()' - Set the calibrated gray fill color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetFillColorGray(
    pdfio_stream_t *st,			// I - Stream
    double         g)			// I - Gray value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%g sc\n", g));
}


//
// 'pdfioContentSetFillColorRGB()' - Set the calibrated RGB fill color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetFillColorRGB(
    pdfio_stream_t *st,			// I - Stream
    double         r,			// I - Red value (0.0 to 1.0)
    double         g,			// I - Green value (0.0 to 1.0)
    double         b)			// I - Blue value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%g %g %g sc\n", r, g, b));
}


//
// 'pdfioContentSetFillColorSpace()' - Set the fill colorspace.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetFillColorSpace(
    pdfio_stream_t *st,			// I - Stream
    const char     *name)		// I - Color space name
{
  return (pdfioStreamPrintf(st, "/%s cs\n", name));
}


//
// 'pdfioContentSetFlatness()' - Set the flatness tolerance.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetFlatness(
    pdfio_stream_t *st,			// I - Stream
    double         flatness)		// I - Flatness value (0.0 to 100.0)
{
  return (pdfioStreamPrintf(st, "%g i\n", flatness));
}


//
// 'pdfioContentSetLineCap()' - Set the line ends style.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetLineCap(
    pdfio_stream_t  *st,		// I - Stream
    pdfio_linecap_t lc)			// I - Line cap value
{
  return (pdfioStreamPrintf(st, "%d J\n", lc));
}


//
// 'pdfioContentSetLineJoin()' - Set the line joining style.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetLineJoin(
    pdfio_stream_t   *st,		// I - Stream
    pdfio_linejoin_t lj)		// I - Line join value
{
  return (pdfioStreamPrintf(st, "%d j\n", lj));
}


//
// 'pdfioContentSetLineWidth()' - Set the line width.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetLineWidth(
    pdfio_stream_t *st,			// I - Stream
    double         width)		// I - Line width value
{
  return (pdfioStreamPrintf(st, "%g w\n", width));
}


//
// 'pdfioContentSetMiterLimit()' - Set the miter limit.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetMiterLimit(
    pdfio_stream_t *st,			// I - Stream
    double         limit)		// I - Miter limit value
{
  return (pdfioStreamPrintf(st, "%g M\n", limit));
}


//
// 'pdfioContentSetStrokeColorDeviceCMYK()' - Set the device CMYK stroke color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetStrokeColorDeviceCMYK(
    pdfio_stream_t *st,			// I - Stream
    double         c,			// I - Cyan value (0.0 to 1.0)
    double         m,			// I - Magenta value (0.0 to 1.0)
    double         y,			// I - Yellow value (0.0 to 1.0)
    double         k)			// I - Black value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%g %g %g %g K\n", c, m, y, k));
}


//
// 'pdfioContentSetStrokeColorDeviceGray()' - Set the device gray stroke color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetStrokeColorDeviceGray(
    pdfio_stream_t *st,			// I - Stream
    double         g)			// I - Gray value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%g G\n", g));
}


//
// 'pdfioContentSetStrokeColorDeviceRGB()' - Set the device RGB stroke color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetStrokeColorDeviceRGB(
    pdfio_stream_t *st,			// I - Stream
    double         r,			// I - Red value (0.0 to 1.0)
    double         g,			// I - Green value (0.0 to 1.0)
    double         b)			// I - Blue value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%g %g %g RG\n", r, g, b));
}


//
// 'pdfioContentSetStrokeColorGray()' - Set the calibrated gray stroke color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetStrokeColorGray(
    pdfio_stream_t *st,			// I - Stream
    double         g)			// I - Gray value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%g SC\n", g));
}


//
// 'pdfioContentSetStrokeColorRGB()' - Set the calibrated RGB stroke color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetStrokeColorRGB(
    pdfio_stream_t *st,			// I - Stream
    double         r,			// I - Red value (0.0 to 1.0)
    double         g,			// I - Green value (0.0 to 1.0)
    double         b)			// I - Blue value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%g %g %g SC\n", r, g, b));
}


//
// 'pdfioContentSetStrokeColorSpace()' - Set the stroke color space.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetStrokeColorSpace(
    pdfio_stream_t *st,			// I - Stream
    const char     *name)		// I - Color space name
{
  return (pdfioStreamPrintf(st, "/%s CS\n", name));
}


//
// 'pdfioContentSetTextCharacterSpacing()' - Set the spacing between characters.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetTextCharacterSpacing(
    pdfio_stream_t *st,			// I - Stream
    double         spacing)		// I - Character spacing
{
  return (pdfioStreamPrintf(st, "%g Tc\n", spacing));
}


//
// 'pdfioContentSetTextFont()' - Set the text font and size.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetTextFont(
    pdfio_stream_t *st,			// I - Stream
    const char     *name,		// I - Font name
    double         size)		// I - Font size
{
  return (pdfioStreamPrintf(st, "/%s %g Tf\n", name, size));
}


//
// 'pdfioContentSetTextLeading()' - Set text leading (line height) value.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetTextLeading(
    pdfio_stream_t *st,			// I - Stream
    double         leading)		// I - Leading (line height) value
{
  return (pdfioStreamPrintf(st, "%g TL\n", leading));
}


//
// 'pdfioContentSetTextMatrix()' - Set the text transform matrix.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetTextMatrix(
    pdfio_stream_t *st,			// I - Stream
    pdfio_matrix_t m)			// I - Transform matrix
{
  return (pdfioStreamPrintf(st, "%g %g %g %g %g %g Tm\n", m[0][0], m[0][1], m[1][0], m[1][1], m[2][0], m[2][1]));
}


//
// 'pdfioContentSetTextRenderingMode()' - Set the text rendering mode.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetTextRenderingMode(
    pdfio_stream_t        *st,		// I - Stream
    pdfio_textrendering_t mode)		// I - Text rendering mode
{
  return (pdfioStreamPrintf(st, "%d Tr\n", mode));
}


//
// 'pdfioContentSetTextRise()' - Set the text baseline offset.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetTextRise(
    pdfio_stream_t *st,			// I - Stream
    double         rise)		// I - Y offset
{
  return (pdfioStreamPrintf(st, "%g Ts\n", rise));
}


//
// 'pdfioContentSetTextWordSpacing()' - Set the inter-word spacing.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetTextWordSpacing(
    pdfio_stream_t *st,			// I - Stream
    double         spacing)		// I - Spacing between words
{
  return (pdfioStreamPrintf(st, "%g Tw\n", spacing));
}


//
// 'pdfioContentSetTextXScaling()' - Set the horizontal scaling value.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetTextXScaling(
    pdfio_stream_t *st,			// I - Stream
    double         percent)		// I - Horizontal scaling in percent
{
  return (pdfioStreamPrintf(st, "%g Tz\n", percent));
}


//
// 'pdfioContentStroke()' - Stroke the current path.
//

bool					// O - `true` on success, `false` on failure
pdfioContentStroke(pdfio_stream_t *st)	// I - Stream
{
  return (pdfioStreamPuts(st, "S\n"));
}


//
// 'pdfioContentTextBegin()' - Begin a text block.
//

bool					// O - `true` on success, `false` on failure
pdfioContentTextBegin(
    pdfio_stream_t *st)			// I - Stream
{
  return (pdfioStreamPuts(st, "BT\n"));
}


//
// 'pdfioContentTextEnd()' - End a text block.
//

bool					// O - `true` on success, `false` on failure
pdfioContentTextEnd(pdfio_stream_t *st)	// I - Stream
{
  return (pdfioStreamPuts(st, "ET\n"));
}


//
// 'pdfioContentTextMoveLine()' - Move to the next line and offset.
//

bool					// O - `true` on success, `false` on failure
pdfioContentTextMoveLine(
    pdfio_stream_t *st,			// I - Stream
    double         tx,			// I - X offset
    double         ty)			// I - Y offset
{
  return (pdfioStreamPrintf(st, "%g %g TD\n", tx, ty));
}


//
// 'pdfioContentTextMoveTo()' - Offset within the current line.
//

bool					// O - `true` on success, `false` on failure
pdfioContentTextMoveTo(
    pdfio_stream_t *st,			// I - Stream
    double         tx,			// I - X offset
    double         ty)			// I - Y offset
{
  return (pdfioStreamPrintf(st, "%g %g Td\n", tx, ty));
}


//
// 'pdfioContentTextNextLine()' - Move to the next line.
//

bool					// O - `true` on success, `false` on failure
pdfioContentTextNextLine(
    pdfio_stream_t *st)			// I - Stream
{
  return (pdfioStreamPuts(st, "T*\n"));
}


//
// 'pdfioContentTextShow()' - Show text.
//

bool					// O - `true` on success, `false` on failure
pdfioContentTextShow(
    pdfio_stream_t *st,			// I - Stream
    const char     *s)			// I - String to show
{
  bool	newline = false;		// New line?


  // Write the string...
  if (!write_string(st, s, &newline))
    return (false);

  // Draw it...
  if (newline)
    return (pdfioStreamPuts(st, "\'\n"));
  else
    return (pdfioStreamPuts(st, "Tj\n"));
}


//
// 'pdfioContentTextShowf()' - Show formatted text.
//

bool
pdfioContentTextShowf(
    pdfio_stream_t *st,			// I - Stream
    const char     *format,		// I - `printf`-style format string
    ...)				// I - Additional arguments as needed
{
  bool		newline = false;	// New line?
  char		buffer[8192];		// Text buffer
  va_list	ap;			// Argument pointer


  // Format the string...
  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  // Write the string...
  if (!write_string(st, buffer, &newline))
    return (false);

  // Draw it...
  if (newline)
    return (pdfioStreamPuts(st, "\'\n"));
  else
    return (pdfioStreamPuts(st, "Tj\n"));
}


//
// 'pdfioContentTextShowJustified()' - Show justified text.
//

bool					// O - `true` on success, `false` on failure
pdfioContentTextShowJustified(
    pdfio_stream_t     *st,		// I - Stream
    size_t             num_fragments,	// I - Number of text fragments
    const double       *offsets,	// I - Text offsets before fragments
    const char * const *fragments)	// I - Text fragments
{
  size_t	i;			// Looping var


  // Write an array of offsets and string fragments...
  if (!pdfioStreamPuts(st, "["))
    return (false);

  for (i = 0; i < num_fragments; i ++)
  {
    if (offsets[i] != 0.0f)
    {
      if (!pdfioStreamPrintf(st, "%g", offsets[i]))
        return (false);
    }

    if (fragments[i])
    {
      if (!write_string(st, fragments[i], NULL))
        return (false);
    }
  }

  return (pdfioStreamPuts(st, "]TJ\n"));
}


//
// 'pdfioPageDictAddICCColorSpace()' - Add an ICC color space to the page
//                                     dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioPageDictAddICCColorSpace(
    pdfio_dict_t *dict,			// I - Page dictionary
    const char   *name,			// I - Color space name
    pdfio_obj_t  *obj)			// I - ICC profile object
{
  (void)dict;
  (void)name;
  (void)obj;

  return (false);
}


//
// 'pdfioPageDictAddCalibratedColorSpace()' - Add a calibrated color space to
//                                            the page dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioPageDictAddCalibratedColorSpace(
    pdfio_dict_t *dict,			// I - Page dictionary
    const char   *name,			// I - Color space name
    size_t       num_colors,		// I - Number of color components
    double       gamma,			// I - Gamma value
    const double matrix[3][3],		// I - Color transform matrix
    const double white_point[3])	// I - CIE XYZ white point
{
  pdfio_dict_t	*resources;		// Resource dictionary
  pdfio_dict_t	*colorspace;		// ColorSpace dictionary


  // Range check input...
  if (!dict || !name || !num_colors || gamma <= 0.0)
    return (false);

  // Get the ColorSpace dictionary...
  if ((resources = pdfioDictGetDict(dict, "Resources")) == NULL)
  {
    if ((resources = pdfioDictCreate(dict->pdf)) == NULL)
      return (false);

    if (!pdfioDictSetDict(dict, "Resources", resources))
      return (false);
  }

  if ((colorspace = pdfioDictGetDict(resources, "ColorSpace")) == NULL)
  {
    if ((colorspace = pdfioDictCreate(dict->pdf)) == NULL)
      return (false);

    if (!pdfioDictSetDict(resources, "ColorSpace", colorspace))
      return (false);
  }

  // Now set the color space reference and return...
  return (pdfioDictSetArray(colorspace, name, create_calcolor(dict->pdf, num_colors, gamma, matrix, white_point)));
}


//
// 'pdfioPageDictAddFont()' - Add a font object to the page dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioPageDictAddFont(
    pdfio_dict_t *dict,			// I - Page dictionary
    const char   *name,			// I - Font name
    pdfio_obj_t  *obj)			// I - Font object
{
  pdfio_dict_t	*resources;		// Resource dictionary
  pdfio_dict_t	*font;			// Font dictionary


  // Range check input...
  if (!dict || !name || !obj)
    return (false);

  // Get the Resources dictionary...
  if ((resources = pdfioDictGetDict(dict, "Resources")) == NULL)
  {
    if ((resources = pdfioDictCreate(dict->pdf)) == NULL)
      return (false);

    if (!pdfioDictSetDict(dict, "Resources", resources))
      return (false);
  }

  // Get the Font dictionary...
  if ((font = pdfioDictGetDict(resources, "Font")) == NULL)
  {
    if ((font = pdfioDictCreate(dict->pdf)) == NULL)
      return (false);

    if (!pdfioDictSetDict(resources, "Font", font))
      return (false);
  }

  // Now set the image reference in the Font resource dictionary and return...
  return (pdfioDictSetObject(font, name, obj));
}


//
// 'pdfioPageDictAddImage()' - Add an image object to the page dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioPageDictAddImage(
    pdfio_dict_t *dict,			// I - Page dictionary
    const char   *name,			// I - Image name
    pdfio_obj_t  *obj)			// I - Image object
{
  pdfio_dict_t	*resources;		// Resource dictionary
  pdfio_dict_t	*xobject;		// XObject dictionary


  // Range check input...
  if (!dict || !name || !obj)
    return (false);

  // Get the Resources dictionary...
  if ((resources = pdfioDictGetDict(dict, "Resources")) == NULL)
  {
    if ((resources = pdfioDictCreate(dict->pdf)) == NULL)
      return (false);

    if (!pdfioDictSetDict(dict, "Resources", resources))
      return (false);
  }

  // Get the XObject dictionary...
  if ((xobject = pdfioDictGetDict(resources, "XObject")) == NULL)
  {
    if ((xobject = pdfioDictCreate(dict->pdf)) == NULL)
      return (false);

    if (!pdfioDictSetDict(resources, "XObject", xobject))
      return (false);
  }

  // Now set the image reference in the XObject resource dictionary and return...
  return (pdfioDictSetObject(xobject, name, obj));
}


//
// 'pdfioFileCreateBaseFontObject()' - Create one of the base 14 PDF fonts.
//
// This function creates one of the base 14 PDF fonts. The "name" parameter
// specifies the font nane:
//
// - `Courier`
// - `Courier-Bold`
// - `Courier-BoldItalic`
// - `Courier-Italic`
// - `Helvetica`
// - `Helvetica-Bold`
// - `Helvetica-BoldOblique`
// - `Helvetica-Oblique`
// - `Symbol`
// - `Times-Bold`
// - `Times-BoldItalic`
// - `Times-Italic`
// - `Times-Roman`
// - `ZapfDingbats`
//

pdfio_obj_t *				// O - Font object
pdfioFileCreateBaseFontObject(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *name)			// I - Font name
{
  pdfio_dict_t	*dict;			// Font dictionary
  pdfio_obj_t	*obj;			// Font object


  if ((dict = pdfioDictCreate(pdf)) == NULL)
    return (NULL);

  pdfioDictSetName(dict, "Type", "Font");
  pdfioDictSetName(dict, "Subtype", "Type1");
  pdfioDictSetName(dict, "BaseFont", pdfioStringCreate(pdf, name));
  pdfioDictSetName(dict, "Encoding", "WinAnsiEncoding");

  if ((obj = pdfioFileCreateObject(dict->pdf, dict)) != NULL)
    pdfioObjClose(obj);

  return (obj);
}


//
// 'pdfioFileCreateFontObject()' - Add a font object to a PDF file.
//

pdfio_obj_t *				// O - Font object
pdfioFileCreateFontObject(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *filename,		// I - Filename
    bool         unicode)		// I - Unicode font?
{
#if 0
  pdfio_dict_t	*dict;			// Font dictionary
  pdfio_obj_t	*obj;			// Font object
  pdfio_st_t	*st;			// Content stream
  int		fd;			// File
  char		buffer[16384];		// Copy buffer
  ssize_t	bytes;			// Bytes read
#endif // 0

  (void)pdf;
  (void)filename;
  (void)unicode;

  return (NULL);
}


//
// 'pdfioFileCreateICCProfileObject()' - Add an ICC profile object to a PDF file.
//

pdfio_obj_t *				// O - Object
pdfioFileCreateICCProfileObject(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *filename)		// I - Filename
{
  (void)pdf;
  (void)filename;

  return (NULL);
}


//
// 'pdfioFileCreateImageObject()' - Add an image object to a PDF file.
//
// Currently only GIF, JPEG, and PNG files are supported.
//

pdfio_obj_t *				// O - Object
pdfioFileCreateImageObject(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *filename,		// I - Filename
    bool         interpolate)		// I - Interpolate image data?
{
  pdfio_dict_t	*dict;			// Image dictionary
  pdfio_obj_t	*obj;			// Image object
  int		fd;			// File
  unsigned char	buffer[32];		// Read buffer
  _pdfio_image_func_t copy_func = NULL;	// Image copy function


  // Range check input...
  if (!pdf || !filename)
    return (NULL);

  // Try opening the file...
  if ((fd = open(filename, O_RDONLY | O_BINARY)) < 0)
  {
    _pdfioFileError(pdf, "Unable to open image file '%s': %s", filename, strerror(errno));
    return (NULL);
  }

  // Read the file header to determine the file format...
  if (read(fd, buffer, sizeof(buffer)) < (ssize_t)sizeof(buffer))
  {
    _pdfioFileError(pdf, "Unable to read header from image file '%s'.", filename);
    close(fd);
    return (NULL);
  }

  lseek(fd, 0, SEEK_SET);

  if (!memcmp(buffer, "\211PNG\015\012\032\012\000\000\000\015IHDR", 16))
  {
    // PNG image...
    copy_func = copy_png;
  }
  else if (!memcmp(buffer, "\377\330\377", 3))
  {
   // JPEG image...
    copy_func = copy_jpeg;
  }
  else
  {
    // Something else that isn't supported...
    _pdfioFileError(pdf, "Unsupported image file '%s'.", filename);
    close(fd);
    return (NULL);
  }

  // Create the base image dictionary the copy the file into an object...
  if ((dict = pdfioDictCreate(pdf)) == NULL)
  {
    close(fd);
    return (NULL);
  }

  pdfioDictSetName(dict, "Type", "XObject");
  pdfioDictSetName(dict, "Subtype", "Image");
  pdfioDictSetBoolean(dict, "Interpolate", interpolate);

  obj = (copy_func)(dict, fd);

  // Close the file and return the object...
  close(fd);

  return (obj);
}


//
// 'pdfioImageGetHeight()' - Get the height of an image object.
//

double					// O - Height in lines
pdfioImageGetHeight(pdfio_obj_t *obj)	// I - Image object
{
  return (pdfioDictGetNumber(obj->value.value.dict, "Height"));
}


//
// 'pdfioImageGetWidth()' - Get the width of an image object.
//

double					// O - Width in columns
pdfioImageGetWidth(pdfio_obj_t *obj)	// I - Image object
{
  return (pdfioDictGetNumber(obj->value.value.dict, "Width"));
}


//
// 'copy_jpeg()' - Copy a JPEG image.
//

static pdfio_obj_t *			// O - Object or `NULL` on error
copy_jpeg(pdfio_dict_t *dict,		// I - Dictionary
          int          fd)		// I - File descriptor
{
  pdfio_obj_t	*obj;			// Object
  pdfio_stream_t *st;			// Stream for JPEG data
  ssize_t	bytes;			// Bytes read
  unsigned char	buffer[16384],		// Read buffer
		*bufptr,		// Pointer into buffer
		*bufend;		// End of buffer
  size_t	length;			// Length of chunk
  unsigned	width = 0,		// Width in columns
		height = 0,		// Height in lines
		num_colors = 0;		// Number of colors


  // Scan the file for a SOFn marker, then we can get the dimensions...
  bytes = read(fd, buffer, sizeof(buffer));

  for (bufptr = buffer + 2, bufend = buffer + bytes; bufptr < bufend;)
  {
    if (*bufptr == 0xff)
    {
      bufptr ++;

      if (bufptr >= bufend)
      {
       /*
	* If we are at the end of the current buffer, re-fill and continue...
	*/

	if ((bytes = read(fd, buffer, sizeof(buffer))) <= 0)
	  break;

	bufptr = buffer;
	bufend = buffer + bytes;
      }

      if (*bufptr == 0xff)
	continue;

      if ((bufptr + 16) >= bufend)
      {
       /*
	* Read more of the marker...
	*/

	bytes = bufend - bufptr;

	memmove(buffer, bufptr, (size_t)bytes);
	bufptr = buffer;
	bufend = buffer + bytes;

	if ((bytes = read(fd, bufend, sizeof(buffer) - (size_t)bytes)) <= 0)
	  break;

	bufend += bytes;
      }

      length = (size_t)((bufptr[1] << 8) | bufptr[2]);

      PDFIO_DEBUG("copy_jpeg: JPEG X'FF%02X' (length %u)\n", *bufptr, (unsigned)length);

      if ((*bufptr >= 0xc0 && *bufptr <= 0xc3) || (*bufptr >= 0xc5 && *bufptr <= 0xc7) || (*bufptr >= 0xc9 && *bufptr <= 0xcb) || (*bufptr >= 0xcd && *bufptr <= 0xcf))
      {
        // SOFn marker, look for dimensions...
        if (bufptr[3] != 8)
        {
          _pdfioFileError(dict->pdf, "Unable to load %d-bit JPEG image.", bufptr[3]);
          return (NULL);
        }

	width      = (unsigned)((bufptr[6] << 8) | bufptr[7]);
	height     = (unsigned)((bufptr[4] << 8) | bufptr[5]);
	num_colors = bufptr[8];
	break;
      }

      // Skip past this marker...
      bufptr ++;
      bytes = bufend - bufptr;

      while (length >= (size_t)bytes)
      {
	length -= (size_t)bytes;

	if ((bytes = read(fd, buffer, sizeof(buffer))) <= 0)
	  break;

	bufptr = buffer;
	bufend = buffer + bytes;
      }

      if (length > (size_t)bytes)
	break;

      bufptr += length;
    }
  }

  if (width == 0 || height == 0 || (num_colors != 1 && num_colors != 3))
    return (NULL);

  // Create the image object...
  pdfioDictSetNumber(dict, "Width", width);
  pdfioDictSetNumber(dict, "Height", height);
  pdfioDictSetNumber(dict, "BitsPerComponent", 8);
  // TODO: Add proper JPEG CalRGB/Gray color spaces
  pdfioDictSetArray(dict, "ColorSpace", create_calcolor(dict->pdf, num_colors, pdfioSRGBGamma, pdfioSRGBMatrix, pdfioSRGBWhitePoint));
  pdfioDictSetName(dict, "Filter", "DCTDecode");

  obj = pdfioFileCreateObject(dict->pdf, dict);
  st  = pdfioObjCreateStream(obj, PDFIO_FILTER_NONE);

  // Copy the file to a stream...
  lseek(fd, 0, SEEK_SET);

  while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
  {
    if (!pdfioStreamWrite(st, buffer, (size_t)bytes))
      return (NULL);
  }

  if (!pdfioStreamClose(st))
    return (NULL);

  return (obj);
}


//
// 'copy_png()' - Copy a PNG image.
//

static pdfio_obj_t *			// O - Object or `NULL` on error
copy_png(pdfio_dict_t *dict,		// I - Dictionary
         int          fd)		// I - File descriptor
{
  // TODO: Implement copy_png
  (void)dict;
  (void)fd;
  return (NULL);
}


//
// 'create_calcolor()' - Create a CalGray or CalRGB color array.
//

static pdfio_array_t *			// O - Array
create_calcolor(
    pdfio_file_t *pdf,			// I - PDF file
    size_t       num_colors,		// I - Number of colors (1 or 3)
    double       gamma,			// I - Gamma value
    const double matrix[3][3],		// I - XYZ transform
    const double white_point[3])	// I - White point
{
  size_t	i;			// Looping var
  pdfio_array_t	*calcolor;		// Array to hold calibrated color space
  pdfio_dict_t	*dict;			// Dictionary to hold color values
  pdfio_array_t	*value;			// Value for white point, matrix, and gamma


  // Create the array with two values - a name and a dictionary...
  if ((calcolor = pdfioArrayCreate(pdf)) == NULL)
    return (NULL);

  if (num_colors == 1)
    pdfioArrayAppendName(calcolor, "CalGray");
  else
    pdfioArrayAppendName(calcolor, "CalRGB");

  if ((dict = pdfioDictCreate(pdf)) == NULL)
    return (NULL);

  pdfioArrayAppendDict(calcolor, dict);

  // Then add the values...
  if (num_colors == 1)
  {
    pdfioDictSetNumber(dict, "Gamma", gamma);
  }
  else
  {
    if ((value = pdfioArrayCreate(pdf)) == NULL)
      return (NULL);

    for (i = 0; i < num_colors; i ++)
      pdfioArrayAppendNumber(value, gamma);

    pdfioDictSetArray(dict, "Gamma", value);
  }

  if (white_point)
  {
    if ((value = pdfioArrayCreate(pdf)) == NULL)
      return (NULL);

    pdfioArrayAppendNumber(value, white_point[0]);
    pdfioArrayAppendNumber(value, white_point[1]);
    pdfioArrayAppendNumber(value, white_point[2]);

    pdfioDictSetArray(dict, "WhitePoint", value);
  }

  if (num_colors > 1 && matrix)
  {
    if ((value = pdfioArrayCreate(pdf)) == NULL)
      return (NULL);

    pdfioArrayAppendNumber(value, matrix[0][0]);
    pdfioArrayAppendNumber(value, matrix[1][0]);
    pdfioArrayAppendNumber(value, matrix[2][0]);
    pdfioArrayAppendNumber(value, matrix[0][1]);
    pdfioArrayAppendNumber(value, matrix[1][1]);
    pdfioArrayAppendNumber(value, matrix[2][1]);
    pdfioArrayAppendNumber(value, matrix[0][2]);
    pdfioArrayAppendNumber(value, matrix[1][2]);
    pdfioArrayAppendNumber(value, matrix[2][2]);

    pdfioDictSetArray(dict, "Matrix", value);
  }

  return (calcolor);
}


//
// 'write_string()' - Write a PDF string.
//

static bool				// O - `true` on success, `false` otherwise
write_string(pdfio_stream_t *st,	// I - Stream
             const char     *s,		// I - String
             bool           *newline)	// O - Ends with a newline?
{
  const char	*ptr;			// Pointer into string


  // Determine whether this is Unicode or just ASCII...
  for (ptr = s; *ptr; ptr ++)
  {
    if (*ptr & 0x80)
      break;
  }

  if (*ptr)
  {
    // Unicode string...
    int		ch;			// Unicode character

    if (!pdfioStreamPuts(st, "<"))
      return (false);

    for (ptr = s; *ptr; ptr ++)
    {
      if ((*ptr & 0xe0) == 0xc0)
      {
        // Two-byte UTF-8
        ch = ((ptr[0] & 0x1f) << 6) | (ptr[1] & 0x3f);
        ptr ++;
      }
      else if ((*ptr & 0xf0) == 0xe0)
      {
        // Three-byte UTF-8
        ch = ((ptr[0] & 0x0f) << 12) | ((ptr[1] & 0x3f) << 6) | (ptr[2] & 0x3f);
        ptr += 2;
      }
      else if ((*ptr & 0xf8) == 0xf0)
      {
        // Four-byte UTF-8
        ch = ((ptr[0] & 0x07) << 18) | ((ptr[1] & 0x3f) << 12) | ((ptr[2] & 0x3f) << 6) | (ptr[3] & 0x3f);
        ptr += 3;
      }
      else if (*ptr == '\n' && newline)
      {
        *newline = true;
        break;
      }
      else
        ch = *ptr & 255;

      // Write a two-byte character...
      if (!pdfioStreamPrintf(st, "%04X", ch))
        return (false);
    }
    if (!pdfioStreamPuts(st, ">"))
      return (false);
  }
  else
  {
    // ASCII string...
    const char	*start = s;		// Start of fragment
    int		level = 0;		// Paren level

    if (!pdfioStreamPuts(st, "("))
      return (false);

    for (ptr = start; *ptr; ptr ++)
    {
      if (*ptr == '\\' || (*ptr == ')' && level == 0) || *ptr < ' ')
      {
        if (ptr > start)
        {
          if (!pdfioStreamWrite(st, start, (size_t)(ptr - start)))
            return (false);

          start = ptr + 1;
	}

        if (*ptr < ' ')
        {
          if (!pdfioStreamPrintf(st, "\\%03o", *ptr))
            return (false);
        }
        else if (!pdfioStreamPrintf(st, "\\%c", *ptr))
          return (false);
      }
      else if (*ptr == '(')
        level ++;
      else if (*ptr == ')')
        level --;
      else if (*ptr == '\n' && newline)
      {
        if (ptr > start)
        {
          if (!pdfioStreamWrite(st, start, (size_t)(ptr - start)))
            return (false);

          start = ptr + 1;
	}

        *newline = true;
        break;
      }
    }

    if (ptr > start)
    {
      if (!pdfioStreamPrintf(st, "%s)", start))
        return (false);
    }
    else if (!pdfioStreamPuts(st, ")"))
      return (false);
  }

  return (true);
}
