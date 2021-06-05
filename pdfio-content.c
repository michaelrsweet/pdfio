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
// Local constants...
//

#define _PDFIO_PNG_CHUNK_IDAT	0x49444154	// Image data
#define _PDFIO_PNG_CHUNK_IEND	0x49454e44	// Image end
#define _PDFIO_PNG_CHUNK_IHDR	0x49484452	// Image header
#define _PDFIO_PNG_CHUNK_PLTE	0x504c5445	// Palette
#define _PDFIO_PNG_CHUNK_cHRM	0x6348524d	// Cromacities and white point
#define _PDFIO_PNG_CHUNK_gAMA	0x67414d41	// Gamma correction
#define _PDFIO_PNG_CHUNK_tRNS	0x74524e53	// Transparency information

#define _PDIFO_PNG_TYPE_GRAY	0	// Grayscale
#define _PDFIO_PNG_TYPE_RGB	2	// RGB
#define _PDFIO_PNG_TYPE_INDEXED	3	// Indexed
#define _PDFIO_PNG_TYPE_GRAYA	4	// Grayscale + alpha
#define _PDFIO_PNG_TYPE_RGBA	6	// RGB + alpha


//
// Local types...
//

typedef pdfio_obj_t *(*_pdfio_image_func_t)(pdfio_dict_t *dict, int fd);


//
// Local functions...
//

static pdfio_obj_t	*copy_jpeg(pdfio_dict_t *dict, int fd);
static pdfio_obj_t	*copy_png(pdfio_dict_t *dict, int fd);
static bool		write_string(pdfio_stream_t *st, const char *s, bool *newline);


//
// 'pdfioArrayCreateCalibratedColorFromMatrix()' - Create a calibrated color space array using a CIE XYZ transform matrix.
//

pdfio_array_t *				// O - Color space array
pdfioArrayCreateCalibratedColorFromMatrix(
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


  // Range check input...
  if (!pdf || (num_colors != 1 && num_colors != 3) || gamma <= 0.0)
    return (NULL);

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
// 'pdfioArrayCreateCalibratedColorFromPrimaries()' - Create a calibrated color sapce array using CIE xy primary chromacities.
//

pdfio_array_t *				// O - Color space array
pdfioArrayCreateCalibratedColorFromPrimaries(
    pdfio_file_t *pdf,			// I - PDF file
    size_t       num_colors,		// I - Number of colors (1 or 3)
    double       gamma,			// I - Gama value
    double       wx,			// I - White point X chromacity
    double       wy,			// I - White point Y chromacity
    double       rx,			// I - Red X chromacity
    double       ry,			// I - Red Y chromacity
    double       gx,			// I - Green X chromacity
    double       gy,			// I - Green Y chromacity
    double       bx,			// I - Blue X chromacity
    double       by)			// I - Blue Y chromacity
{
  double	z;			// Intermediate value
  double	Xa, Xb, Xc,		// Transform values
		Ya, Yb, Yc,
		Za, Zb, Zc;
  double	white_point[3];		// White point CIE XYZ value
  double	matrix[3][3];		// CIE XYZ transform matrix


  PDFIO_DEBUG("pdfioFileCreateCalibratedColorFromPrimaries(pdf=%p, num_colors=%lu, gamma=%g, wx=%g, wy=%g, rx=%g, ry=%g, gx=%g, gy=%g, bx=%g, by=%g)\n", pdf, (unsigned long)num_colors, gamma, wx, wy, rx, ry, gx, gy, bx, by);

  // Range check input...
  if (!pdf || (num_colors != 1 && num_colors != 3) || gamma <= 0.0 || ry == 0.0 || gy == 0.0 || by == 0.0)
    return (NULL);

  // Calculate the white point and transform matrix per the PDF spec...
  z = wy * ((gx - bx) * ry - (rx - bx) * gy + (rx - gx) * by);

  if (z == 0.0)
    return (NULL);			// Undefined

  Ya = ry * ((gx - bx) * wy - (wx - bx) * gy + (wx - gx) * by) / z;
  Xa = Ya * rx / ry;
  Za = Ya * ((1.0 - rx) / ry - 1.0);

  Yb = gy * ((rx - bx) * wy - (wx - bx) * ry + (wx - rx) * by) / z;
  Xb = Yb * gx / gy;
  Zb = Yb * ((1.0 - gx) / gy - 1.0);

  Yc = gy * ((rx - gx) * wy - (wx - gx) * ry + (wx - rx) * gy) / z;
  Xc = Yc * bx / by;
  Zc = Yc * ((1.0 - bx) / by - 1.0);

  white_point[0] = Xa + Xb + Xc;
  white_point[1] = Ya + Yb + Yc;
  white_point[2] = Za + Zb + Zc;

  matrix[0][0] = Xa;
  matrix[0][1] = Ya;
  matrix[0][2] = Za;
  matrix[1][0] = Xb;
  matrix[1][1] = Yb;
  matrix[1][2] = Zb;
  matrix[2][0] = Xc;
  matrix[2][1] = Yc;
  matrix[2][2] = Zc;

  PDFIO_DEBUG("pdfioFileCreateCalibratedColorFromPrimaries: white_point=[%g %g %g]\n", white_point[0], white_point[1], white_point[2]);
  PDFIO_DEBUG("pdfioFileCreateCalibratedColorFromPrimaries: matrix=[%g %g %g %g %g %g %g %g %g]\n", matrix[0][0], matrix[1][0], matrix[2][0], matrix[0][1], matrix[1][1], matrix[2][2], matrix[0][2], matrix[1][2], matrix[2][1]);

  // Now that we have the white point and matrix, use those to make the color array...
  return (pdfioArrayCreateCalibratedColorFromMatrix(pdf, num_colors, gamma, matrix, white_point));
}


//
// 'pdfioArrayCreateICCBasedColor()' - Create an ICC-based color space array.
//

pdfio_array_t *				// O - Color array
pdfioArrayCreateICCBasedColor(
    pdfio_file_t *pdf,			// I - PDF file
    pdfio_obj_t  *icc_object)		// I - ICC profile object
{
  pdfio_array_t	*icc_color;		// Color array


  // Range check input...
  if (!pdf || !icc_object)
    return (NULL);

  // Create the array with two values - a name and an object reference...
  if ((icc_color = pdfioArrayCreate(pdf)) == NULL)
    return (NULL);

  pdfioArrayAppendName(icc_color, "ICCBased");
  pdfioArrayAppendObject(icc_color, icc_object);

  return (icc_color);
}


//
// 'pdfioArrayCreateIndexedColor()' - Create an indexed color space array.
//

pdfio_array_t *				// O - Color array
pdfioArrayCreateIndexedColor(
    pdfio_file_t        *pdf,		// I - PDF file
    size_t              num_colors,	// I - Number of colors
    const unsigned char colors[][3])	// I - RGB values for colors
{
  pdfio_array_t	*indexed_color;		// Color array


  // Range check input...
  if (!pdf || num_colors < 1 || !colors)
    return (NULL);

  // Create the array with four values...
  if ((indexed_color = pdfioArrayCreate(pdf)) == NULL)
    return (NULL);

  pdfioArrayAppendName(indexed_color, "Indexed");
  pdfioArrayAppendName(indexed_color, "DeviceRGB");
  pdfioArrayAppendNumber(indexed_color, num_colors - 1);
  pdfioArrayAppendBinary(indexed_color, colors[0], num_colors * 3);

  return (indexed_color);
}


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
    return (pdfioStreamPuts(st, "Tj T*\n"));
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
    return (pdfioStreamPuts(st, "Tj T*\n"));
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
// 'pdfioImageGetBytesPerLine()' - Get the number of bytes to read for each line.
//

size_t					// O - Number of bytes per line
pdfioImageGetBytesPerLine(
    pdfio_obj_t *obj)			// I - Image object
{
  pdfio_dict_t	*params;		// DecodeParms value
  int		width,			// Width of image
		bpc,			// BitsPerComponent of image
		colors;			// Number of colors in image


  if (!obj || obj->value.type != PDFIO_VALTYPE_DICT)
    return (0);

  params = pdfioDictGetDict(obj->value.value.dict, "DecodeParms");
  bpc    = (int)pdfioDictGetNumber(params, "BitsPerComponent");
  colors = (int)pdfioDictGetNumber(params, "Colors");
  width  = (int)pdfioDictGetNumber(params, "Columns");

  if (width == 0)
    width = (int)pdfioDictGetNumber(obj->value.value.dict, "Width");

  if (bpc == 0)
  {
    if ((bpc = (int)pdfioDictGetNumber(obj->value.value.dict, "BitsPerComponent")) == 0)
      bpc = 8;
  }

  if (colors == 0)
  {
    const char	*cs_name;		// ColorSpace name
    pdfio_array_t *cs_array;		// ColorSpace array

    if ((cs_name = pdfioDictGetName(obj->value.value.dict, "ColorSpace")) == NULL)
    {
      if ((cs_array = pdfioDictGetArray(obj->value.value.dict, "ColorSpace")) != NULL)
        cs_name = pdfioArrayGetName(cs_array, 0);
    }

    if (!cs_name || strstr(cs_name, "RGB"))
      colors = 3;
    else if (strstr(cs_name, "CMYK"))
      colors = 4;
    else
      colors = 1;
  }

  return ((size_t)((width * colors * bpc + 7) / 8));
}


//
// 'pdfioImageGetHeight()' - Get the height of an image object.
//

double					// O - Height in lines
pdfioImageGetHeight(pdfio_obj_t *obj)	// I - Image object
{
  if (obj)
    return (pdfioDictGetNumber(obj->value.value.dict, "Height"));
  else
    return (0.0);
}


//
// 'pdfioImageGetWidth()' - Get the width of an image object.
//

double					// O - Width in columns
pdfioImageGetWidth(pdfio_obj_t *obj)	// I - Image object
{
  if (obj)
    return (pdfioDictGetNumber(obj->value.value.dict, "Width"));
  else
    return (0.0);
}


//
// 'pdfioPageDictAddColorSpace()' - Add a color space to the page dictionary.
//
// This function adds a named color space to the page dictionary.
//
// The names "DefaultCMYK", "DefaultGray", and "DefaultRGB" specify the default
// device color space used for the page.
//
// The "data" array contains a calibrated, indexed, or ICC-based color space
// array that was created using the
// @link pdfioArrayCreateCalibratedColorFromMatrix@,
// @link pdfioArrayCreateCalibratedColorFromPrimaries@,
// @link pdfioArrayCreateICCBasedColor@, or
// @link pdfioArrayCreateIndexedColor@ functions.
//

bool					// O - `true` on success, `false` on failure
pdfioPageDictAddColorSpace(
    pdfio_dict_t  *dict,		// I - Page dictionary
    const char    *name,		// I - Color space name
    pdfio_array_t *data)		// I - Color space array
{
  pdfio_dict_t	*resources;		// Resource dictionary
  pdfio_dict_t	*colorspace;		// ColorSpace dictionary


  // Range check input...
  if (!dict || !name || !data)
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
  return (pdfioDictSetArray(colorspace, name, data));
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
  pdfioDictSetArray(dict, "ColorSpace", pdfioArrayCreateCalibratedColorFromMatrix(dict->pdf, num_colors, pdfioSRGBGamma, pdfioSRGBMatrix, pdfioSRGBWhitePoint));
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
  pdfio_obj_t	*obj = NULL;		// Object
  pdfio_stream_t *st = NULL;		// Stream for JPEG data
  ssize_t	bytes;			// Bytes read
  unsigned char	buffer[16384];		// Read buffer
  unsigned	length,			// Length
		type,			// Chunk code
		crc,			// CRC-32
		width = 0,		// Width
		height = 0;		// Height
  unsigned char	bit_depth = 0,		// Bit depth
		color_type = 0;		// Color type


  // Read the file header...
  if (read(fd, buffer, 8) != 8)
    return (NULL);

  // Then read chunks until we have the image data...
  while (read(fd, buffer, 8) == 8)
  {
    // Get the chunk length and type values...
    length = (unsigned)((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]);
    type   = (unsigned)((buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7]);

    switch (type)
    {
      case _PDFIO_PNG_CHUNK_IDAT : // Image data
          break;

      case _PDFIO_PNG_CHUNK_IEND : // Image end
          break;

      case _PDFIO_PNG_CHUNK_IHDR : // Image header
          break;

      case _PDFIO_PNG_CHUNK_PLTE : // Palette
          break;

      case _PDFIO_PNG_CHUNK_cHRM : // Cromacities and white point
          break;

      case _PDFIO_PNG_CHUNK_gAMA : // Gamma correction
          break;

      case _PDFIO_PNG_CHUNK_tRNS : // Transparency information
          break;

      default :
          break;
    }
  }

  return (NULL);
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
    {
      // UTF-8, allow Unicode up to 255...
      if ((*ptr & 0xe0) == 0xc0 && (*ptr & 0x3f) <= 3 && (ptr[1] & 0xc0) == 0x80)
      {
        ptr ++;
        continue;
      }

      break;
    }
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

    if (!pdfioStreamPuts(st, "("))
      return (false);

    for (ptr = start; *ptr; ptr ++)
    {
      if (*ptr == '\n' && newline)
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
      else if ((*ptr & 0xe0) == 0xc0)
      {
        // Two-byte UTF-8
        unsigned char ch = (unsigned char)(((ptr[0] & 0x1f) << 6) | (ptr[1] & 0x3f));
					// Unicode character

        if (ptr > start)
        {
          if (!pdfioStreamWrite(st, start, (size_t)(ptr - start)))
            return (false);
	}

	if (!pdfioStreamWrite(st, &ch, 1))
	  return (false);

        ptr ++;
	start = ptr + 1;
      }
      else if (*ptr == '\\' || *ptr == '(' || *ptr == ')' || *ptr < ' ')
      {
        if (ptr > start)
        {
          if (!pdfioStreamWrite(st, start, (size_t)(ptr - start)))
            return (false);
	}

	start = ptr + 1;

        if (*ptr < ' ')
        {
          if (!pdfioStreamPrintf(st, "\\%03o", *ptr))
            return (false);
        }
        else if (!pdfioStreamPrintf(st, "\\%c", *ptr))
          return (false);
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
