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

#include "pdfio-private.h"
#include "pdfio-content.h"
#include "ttf.h"
#include <math.h>
#ifndef M_PI
#  define M_PI	3.14159265358979323846264338327950288
#endif // M_PI


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

#define _PDFIO_PNG_TYPE_GRAY	0	// Grayscale
#define _PDFIO_PNG_TYPE_RGB	2	// RGB
#define _PDFIO_PNG_TYPE_INDEXED	3	// Indexed
#define _PDFIO_PNG_TYPE_GRAYA	4	// Grayscale + alpha
#define _PDFIO_PNG_TYPE_RGBA	6	// RGB + alpha

static int	_pdfio_cp1252[] =	// CP1252-specific character mapping
{
  0x20AC,
  0x0000,
  0x201A,
  0x0192,
  0x201E,
  0x2026,
  0x2020,
  0x2021,
  0x02C6,
  0x2030,
  0x0160,
  0x2039,
  0x0152,
  0x0000,
  0x017D,
  0x0000,
  0x0000,
  0x2018,
  0x2019,
  0x201C,
  0x201D,
  0x2022,
  0x2013,
  0x2014,
  0x02DC,
  0x2122,
  0x0161,
  0x203A,
  0x0153,
  0x0000,
  0x017E,
  0x0178
};


//
// Local types...
//

typedef pdfio_obj_t *(*_pdfio_image_func_t)(pdfio_dict_t *dict, int fd);


//
// Local functions...
//

static pdfio_obj_t	*copy_jpeg(pdfio_dict_t *dict, int fd);
static pdfio_obj_t	*copy_png(pdfio_dict_t *dict, int fd);
static bool		create_cp1252(pdfio_file_t *pdf);
static void		ttf_error_cb(pdfio_file_t *pdf, const char *message);
static unsigned		update_png_crc(unsigned crc, const unsigned char *buffer, size_t length);
static bool		write_string(pdfio_stream_t *st, bool unicode, const char *s, bool *newline);


//
// Local globals...
//

static unsigned png_crc_table[256] =	// CRC-32 table for PNG files
{
  0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
  0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
  0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
  0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
  0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
  0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
  0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
  0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
  0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
  0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
  0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
  0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
  0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
  0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
  0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
  0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
  0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
  0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
  0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
  0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
  0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
  0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
  0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
  0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
  0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
  0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
  0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
  0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
  0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
  0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
  0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
  0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
  0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
  0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
  0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
  0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
  0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
  0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
  0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
  0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
  0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
  0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
  0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};


//
// 'pdfioArrayCreateColorFromICCObj()' - Create an ICC-based color space array.
//

pdfio_array_t *				// O - Color array
pdfioArrayCreateColorFromICCObj(
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
  pdfioArrayAppendObj(icc_color, icc_object);

  return (icc_color);
}


//
// 'pdfioArrayCreateColorFromMatrix()' - Create a calibrated color space array using a CIE XYZ transform matrix.
//

pdfio_array_t *				// O - Color space array
pdfioArrayCreateColorFromMatrix(
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
// 'pdfioArrayCreateColorFromPalette()' - Create an indexed color space array.
//

pdfio_array_t *				// O - Color array
pdfioArrayCreateColorFromPalette(
    pdfio_file_t        *pdf,		// I - PDF file
    size_t              num_colors,	// I - Number of colors
    const unsigned char *colors)	// I - RGB values for colors
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
  pdfioArrayAppendBinary(indexed_color, colors, num_colors * 3);

  return (indexed_color);
}


//
// 'pdfioArrayCreateColorFromPrimaries()' - Create a calibrated color sapce array using CIE xy primary chromacities.
//

pdfio_array_t *				// O - Color space array
pdfioArrayCreateColorFromPrimaries(
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
  return (pdfioArrayCreateColorFromMatrix(pdf, num_colors, gamma, matrix, white_point));
}


//
// 'pdfioArrayCreateColorFromStandard()' - Create a color array for a standard color space.
//
// This function creates a color array for a standard `PDFIO_CS_` enumerated color space.
// The "num_colors" argument must be `1` for grayscale and `3` for RGB color.
//

pdfio_array_t *				// O - Color array
pdfioArrayCreateColorFromStandard(
    pdfio_file_t *pdf,			// I - PDF file
    size_t       num_colors,		// I - Number of colors (1 or 3)
    pdfio_cs_t   cs)			// I - Color space enumeration
{
  static const double	adobe_matrix[3][3] = { { 0.57667, 0.18556, 0.18823 }, { 0.29734, 0.62736, 0.07529 }, { 0.02703, 0.07069, 0.99134 } };
  static const double	d65_white_point[3] = { 0.9505, 1.0, 1.0890 };
  static const double	p3_d65_matrix[3][3] = { { 0.48657, 0.26567, 0.19822 }, { 0.22897, 0.69174, 0.07929 }, { 0.00000, 0.04511, 1.04394 } };
  static const double	srgb_matrix[3][3] = { { 0.4124, 0.3576, 0.1805 }, { 0.2126, 0.7152, 0.0722 }, { 0.0193, 0.1192, 0.9505 } };


  // Range check input...
  if (!pdf)
  {
    return (NULL);
  }
  else if (num_colors != 1 && num_colors != 3)
  {
    _pdfioFileError(pdf, "Unsupported number of colors %u.", (unsigned)num_colors);
    return (NULL);
  }

  switch (cs)
  {
    case PDFIO_CS_ADOBE :
        return (pdfioArrayCreateColorFromMatrix(pdf, num_colors, 2.2, adobe_matrix, d65_white_point));
    case PDFIO_CS_P3_D65 :
        return (pdfioArrayCreateColorFromMatrix(pdf, num_colors, 2.2, p3_d65_matrix, d65_white_point));
    case PDFIO_CS_SRGB :
        return (pdfioArrayCreateColorFromMatrix(pdf, num_colors, 2.2, srgb_matrix, d65_white_point));

    default :
        _pdfioFileError(pdf, "Unsupported color space number %d.", (int)cs);
        return (NULL);
  }
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
  double dcos = cos(M_PI * degrees / 180.0);
					// Cosine
  double dsin = sin(M_PI * degrees / 180.0);
					// Sine


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
// 'pdfioContentPathEnd()' - Clear the current path.
//

bool					// O - `true` on success, `false` on failure
pdfioContentPathEnd(pdfio_stream_t *st)	// I - Stream
{
  return (pdfioStreamPuts(st, "n\n"));
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
    double         phase,		// I - Phase (offset within pattern)
    double         on,			// I - On length
    double         off)			// I - Off length
{
  return (pdfioStreamPrintf(st, "[%g %g] %g d\n", on, off, phase));
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
// This function shows some text in a PDF content stream. The "unicode" argument
// specifies that the current font maps to full Unicode.  The "s" argument
// specifies a UTF-8 encoded string.
//

bool					// O - `true` on success, `false` on failure
pdfioContentTextShow(
    pdfio_stream_t *st,			// I - Stream
    bool           unicode,		// I - Unicode text?
    const char     *s)			// I - String to show
{
  bool	newline = false;		// New line?


  // Write the string...
  if (!write_string(st, unicode, s, &newline))
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
// This function shows some text in a PDF content stream. The "unicode" argument
// specifies that the current font maps to full Unicode.  The "format" argument
// specifies a UTF-8 encoded `printf`-style format string.
//

bool
pdfioContentTextShowf(
    pdfio_stream_t *st,			// I - Stream
    bool           unicode,		// I - Unicode text?
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
  if (!write_string(st, unicode, buffer, &newline))
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
// This function shows some text in a PDF content stream. The "unicode" argument
// specifies that the current font maps to full Unicode.  The "fragments"
// argument specifies an array of UTF-8 encoded strings.
//

bool					// O - `true` on success, `false` on failure
pdfioContentTextShowJustified(
    pdfio_stream_t     *st,		// I - Stream
    bool               unicode,		// I - Unicode text?
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
      if (!write_string(st, unicode, fragments[i], NULL))
        return (false);
    }
  }

  return (pdfioStreamPuts(st, "]TJ\n"));
}


//
// 'pdfioFileCreateBaseFontObj()' - Create one of the base 14 PDF fonts.
//
// This function creates one of the base 14 PDF fonts. The "name" parameter
// specifies the font nane:
//
// - "Courier"
// - "Courier-Bold"
// - "Courier-BoldItalic"
// - "Courier-Italic"
// - "Helvetica"
// - "Helvetica-Bold"
// - "Helvetica-BoldOblique"
// - "Helvetica-Oblique"
// - "Symbol"
// - "Times-Bold"
// - "Times-BoldItalic"
// - "Times-Italic"
// - "Times-Roman"
// - "ZapfDingbats"
//
// Base fonts always use the Windows CP1252 (ISO-8859-1 with additional
// characters such as the Euro symbol) subset of Unicode.
//

pdfio_obj_t *				// O - Font object
pdfioFileCreateFontObjFromBase(
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

  if (strcmp(name, "Symbol") && strcmp(name, "ZapfDingbats"))
  {
    if (!pdf->cp1252_obj && !create_cp1252(pdf))
      return (NULL);

    pdfioDictSetObj(dict, "Encoding", pdf->cp1252_obj);
  }

  if ((obj = pdfioFileCreateObj(dict->pdf, dict)) != NULL)
    pdfioObjClose(obj);

  return (obj);
}


//
// 'pdfioFileCreateFontObjFromFile()' - Add a font object to a PDF file.
//
// This function embeds a TrueType/OpenType font into a PDF file.  The
// "unicode" parameter controls whether the font is encoded for two-byte
// characters (potentially full Unicode, but more typically a subset)
// or to only support the Windows CP1252 (ISO-8859-1 with additional
// characters such as the Euro symbol) subset of Unicode.
//

pdfio_obj_t *				// O - Font object
pdfioFileCreateFontObjFromFile(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *filename,		// I - Filename
    bool         unicode)		// I - Force Unicode
{
  ttf_t		*font;			// TrueType font
  ttf_rect_t	bounds;			// Font bounds
  pdfio_dict_t	*dict,			// Font dictionary
		*desc,			// Font descriptor
		*file;			// Font file dictionary
  pdfio_obj_t	*obj,			// Font object
		*desc_obj,		// Font descriptor object
		*file_obj;		// Font file object
  const char	*basefont;		// Base font name
  pdfio_array_t	*bbox;			// Font bounding box array
  pdfio_stream_t *st;			// Font stream
  int		fd;			// File
  unsigned char	buffer[16384];		// Read buffer
  ssize_t	bytes;			// Bytes read


  // Range check input...
  if (!pdf)
    return (NULL);

  if (!filename)
  {
    _pdfioFileError(pdf, "No TrueType/OpenType filename specified.");
    return (NULL);
  }

  if ((fd = open(filename, O_RDONLY | O_BINARY)) < 0)
  {
    _pdfioFileError(pdf, "Unable to open font file '%s': %s", filename, strerror(errno));
    return (NULL);
  }

  if ((font = ttfCreate(filename, 0, (ttf_err_cb_t)ttf_error_cb, pdf)) == NULL)
  {
    close(fd);
    return (NULL);
  }

  // Create the font file dictionary and object...
  if ((file = pdfioDictCreate(pdf)) == NULL)
  {
    ttfDelete(font);
    close(fd);
    return (NULL);
  }

  pdfioDictSetName(file, "Filter", "FlateDecode");

  if ((file_obj = pdfioFileCreateObj(pdf, file)) == NULL)
  {
    ttfDelete(font);
    close(fd);
    return (NULL);
  }

  if ((st = pdfioObjCreateStream(file_obj, PDFIO_FILTER_FLATE)) == NULL)
  {
    ttfDelete(font);
    close(fd);
    return (NULL);
  }

  while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
  {
    if (!pdfioStreamWrite(st, buffer, (size_t)bytes))
    {
      ttfDelete(font);
      close(fd);
      pdfioStreamClose(st);
      return (NULL);
    }
  }

  close(fd);
  pdfioStreamClose(st);

  // Create the font descriptor dictionary and object...
  if ((bbox = pdfioArrayCreate(pdf)) == NULL)
  {
    ttfDelete(font);
    return (NULL);
  }

  ttfGetBounds(font, &bounds);

  pdfioArrayAppendNumber(bbox, bounds.left);
  pdfioArrayAppendNumber(bbox, bounds.bottom);
  pdfioArrayAppendNumber(bbox, bounds.right);
  pdfioArrayAppendNumber(bbox, bounds.top);

  if ((desc = pdfioDictCreate(pdf)) == NULL)
  {
    ttfDelete(font);
    return (NULL);
  }

  basefont = pdfioStringCreate(pdf, ttfGetPostScriptName(font));

  pdfioDictSetName(desc, "Type", "FontDescriptor");
  pdfioDictSetName(desc, "FontName", basefont);
  pdfioDictSetObj(desc, "FontFile2", file_obj);
  pdfioDictSetNumber(desc, "Flags", ttfIsFixedPitch(font) ? 0x21 : 0x20);
  pdfioDictSetArray(desc, "FontBBox", bbox);
  pdfioDictSetNumber(desc, "ItalicAngle", ttfGetItalicAngle(font));
  pdfioDictSetNumber(desc, "Ascent", ttfGetAscent(font));
  pdfioDictSetNumber(desc, "Descent", ttfGetDescent(font));
  pdfioDictSetNumber(desc, "CapHeight", ttfGetCapHeight(font));
  pdfioDictSetNumber(desc, "XHeight", ttfGetXHeight(font));
  // Note: No TrueType value exists for this but PDF requires it, so we
  // calculate a generic value from 50 to 250 based on the weight...
  pdfioDictSetNumber(desc, "StemV", ttfGetWeight(font) / 4 + 25);

  if ((desc_obj = pdfioFileCreateObj(pdf, desc)) == NULL)
  {
    ttfDelete(font);
    return (NULL);
  }

  pdfioObjClose(desc_obj);

  if (unicode)
  {
    // Unicode (CID) font...
    pdfio_dict_t	*cid2gid;	// CIDToGIDMap dictionary
    pdfio_obj_t		*cid2gid_obj;	// CIDToGIDMap object
    size_t		i,		// Looping var
			num_cmap;	// Number of CMap entries
    const int		*cmap;		// CMap entries
    unsigned char	*bufptr,	// Pointer into buffer
			*bufend;	// End of buffer
    pdfio_dict_t	*type2;		// CIDFontType2 font dictionary
    pdfio_obj_t		*type2_obj;	// CIDFontType2 font object
    pdfio_array_t	*descendants;	// Decendant font list
    pdfio_dict_t	*sidict;	// CIDSystemInfo dictionary

    // Create a CIDToGIDMap object for the Unicode font...
    if ((cid2gid = pdfioDictCreate(pdf)) == NULL)
    {
      ttfDelete(font);
      return (NULL);
    }

#ifndef DEBUG
    pdfioDictSetName(cid2gid, "Filter", "FlateDecode");
#endif // !DEBUG

    if ((cid2gid_obj = pdfioFileCreateObj(pdf, cid2gid)) == NULL)
    {
      ttfDelete(font);
      return (NULL);
    }

#ifdef DEBUG
    if ((st = pdfioObjCreateStream(cid2gid_obj, PDFIO_FILTER_NONE)) == NULL)
#else
    if ((st = pdfioObjCreateStream(cid2gid_obj, PDFIO_FILTER_FLATE)) == NULL)
#endif // DEBUG
    {
      ttfDelete(font);
      return (NULL);
    }

    cmap = ttfGetCMap(font, &num_cmap);

    for (i = 0, bufptr = buffer, bufend = buffer + sizeof(buffer); i < num_cmap; i ++)
    {
      if (cmap[i] < 0)
      {
        // Map undefined glyph to .notdef...
        *bufptr++ = 0;
        *bufptr++ = 0;
      }
      else
      {
        // Map to specified glyph...
        *bufptr++ = (unsigned char)(cmap[i] >> 8);
        *bufptr++ = (unsigned char)(cmap[i] & 255);
      }

      if (bufptr >= bufend)
      {
        // Flush buffer...
        if (!pdfioStreamWrite(st, buffer, (size_t)(bufptr - buffer)))
        {
	  pdfioStreamClose(st);
          ttfDelete(font);
          return (NULL);
        }

        bufptr = buffer;
      }
    }

    if (bufptr > buffer)
    {
      // Flush buffer...
      if (!pdfioStreamWrite(st, buffer, (size_t)(bufptr - buffer)))
      {
	pdfioStreamClose(st);
	ttfDelete(font);
	return (NULL);
      }
    }

    pdfioStreamClose(st);

    // Create a CIDFontType2 dictionary for the Unicode font...
    if ((type2 = pdfioDictCreate(pdf)) == NULL)
    {
      ttfDelete(font);
      return (NULL);
    }

    if ((sidict = pdfioDictCreate(pdf)) == NULL)
    {
      ttfDelete(font);
      return (NULL);
    }

    // CIDSystemInfo mapping to Adobe UCS2 v0 (Unicode)
    pdfioDictSetString(sidict, "Registry", "Adobe");
    pdfioDictSetString(sidict, "Ordering", "Identity");
    pdfioDictSetNumber(sidict, "Supplement", 0);

    // Then the dictionary for the CID base font...
    pdfioDictSetName(type2, "Type", "Font");
    pdfioDictSetName(type2, "Subtype", "CIDFontType2");
    pdfioDictSetName(type2, "BaseFont", basefont);
    pdfioDictSetDict(type2, "CIDSystemInfo", sidict);
    pdfioDictSetObj(type2, "CIDToGIDMap", cid2gid_obj);
    pdfioDictSetObj(type2, "FontDescriptor", desc_obj);

    if ((type2_obj = pdfioFileCreateObj(pdf, type2)) == NULL)
    {
      ttfDelete(font);
      return (NULL);
    }

    pdfioObjClose(type2_obj);

    // Create a Type 0 font object...
    if ((descendants = pdfioArrayCreate(pdf)) == NULL)
    {
      ttfDelete(font);
      return (NULL);
    }

    pdfioArrayAppendObj(descendants, type2_obj);

    if ((dict = pdfioDictCreate(pdf)) == NULL)
    {
      ttfDelete(font);
      return (NULL);
    }

    pdfioDictSetName(dict, "Type", "Font");
    pdfioDictSetName(dict, "Subtype", "Type0");
    pdfioDictSetName(dict, "BaseFont", basefont);
    pdfioDictSetArray(dict, "DescendantFonts", descendants);
    pdfioDictSetName(dict, "Encoding", "Identity-H");

    if ((obj = pdfioFileCreateObj(pdf, dict)) == NULL)
      return (NULL);

    pdfioObjClose(obj);
  }
  else
  {
    // Simple (CP1282 or custom encoding) 8-bit font...
    if (ttfGetMaxChar(font) >= 255 && !pdf->cp1252_obj && !create_cp1252(pdf))
    {
      ttfDelete(font);
      return (NULL);
    }

    // Create a TrueType font object...
    if ((dict = pdfioDictCreate(pdf)) == NULL)
    {
      ttfDelete(font);
      return (NULL);
    }

    pdfioDictSetName(dict, "Type", "Font");
    pdfioDictSetName(dict, "Subtype", "TrueType");
    pdfioDictSetName(dict, "BaseFont", basefont);
    if (ttfGetMaxChar(font) >= 255)
      pdfioDictSetObj(dict, "Encoding", pdf->cp1252_obj);

    pdfioDictSetObj(dict, "FontDescriptor", desc_obj);

    if ((obj = pdfioFileCreateObj(pdf, dict)) == NULL)
    {
      ttfDelete(font);
      return (NULL);
    }

    pdfioObjClose(obj);
  }

  ttfDelete(font);

  return (obj);
}


//
// 'pdfioFileCreateICCObjFromFile()' - Add an ICC profile object to a PDF file.
//

pdfio_obj_t *				// O - Object
pdfioFileCreateICCObjFromFile(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *filename,		// I - Filename
    size_t       num_colors)		// I - Number of color components (1, 3, or 4)
{
  pdfio_dict_t	*dict;			// ICC profile dictionary
  pdfio_obj_t	*obj;			// ICC profile object
  pdfio_stream_t *st;			// ICC profile stream
  int		fd;			// File
  unsigned char	buffer[16384];		// Read buffer
  ssize_t	bytes;			// Bytes read


  // Range check input...
  if (!pdf)
    return (NULL);

  if (!filename)
  {
    _pdfioFileError(pdf, "No ICC profile filename specified.");
    return (NULL);
  }

  if ((fd = open(filename, O_RDONLY | O_BINARY)) < 0)
  {
    _pdfioFileError(pdf, "Unable to open ICC profile '%s': %s", filename, strerror(errno));
    return (NULL);
  }

  if (num_colors != 1 && num_colors != 3 && num_colors != 4)
  {
    _pdfioFileError(pdf, "Unsupported number of colors (%lu) for ICC profile.", (unsigned long)num_colors);
    close(fd);
    return (NULL);
  }

  // Create the ICC profile object...
  if ((dict = pdfioDictCreate(pdf)) == NULL)
  {
    close(fd);
    return (NULL);
  }

  pdfioDictSetNumber(dict, "N", num_colors);
  pdfioDictSetName(dict, "Filter", "FlateDecode");

  if ((obj = pdfioFileCreateObj(pdf, dict)) == NULL)
  {
    close(fd);
    return (NULL);
  }

  if ((st = pdfioObjCreateStream(obj, PDFIO_FILTER_FLATE)) == NULL)
  {
    close(fd);
    return (NULL);
  }

  while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
  {
    if (!pdfioStreamWrite(st, buffer, (size_t)bytes))
    {
      close(fd);
      pdfioStreamClose(st);
      return (NULL);
    }
  }

  close(fd);
  pdfioStreamClose(st);

  return (obj);
}


//
// 'pdfioFileCreateImageObjFromData()' - Add image object(s) to a PDF file from memory.
//
// This function creates image object(s) in a PDF file from a data buffer in
// memory.  The "data" parameter points to the image data as 8-bit color values.
// The "width" and "height" parameters specify the image dimensions.  The
// "num_colors" parameter specifies the number of color components (`1` for
// grayscale, `3` for RGB, and `4` for CMYK) and the "alpha" parameter specifies
// whether each color tuple is followed by an alpha value.  The "color_data"
// parameter specifies an optional color space array for the image - if `NULL`,
// the image is encoded in the corresponding device color space.  The
// "interpolate" parameter specifies whether to interpolate when scaling the
// image on the page.
//
// Note: When creating an image object with alpha, a second image object is
// created to hold the "soft mask" data for the primary image.
//

pdfio_obj_t *				// O - Object
pdfioFileCreateImageObjFromData(
    pdfio_file_t        *pdf,		// I - PDF file
    const unsigned char *data,		// I - Pointer to image data
    size_t              width,		// I - Width of image
    size_t              height,		// I - Height of image
    size_t              num_colors,	// I - Number of colors
    pdfio_array_t       *color_data,	// I - Colorspace data or `NULL` for default
    bool                alpha,		// I - `true` if data contains an alpha channel
    bool                interpolate)	// I - Interpolate image data?
{
  pdfio_dict_t		*dict,		// Image dictionary
			*decode;	// DecodeParms dictionary
  pdfio_obj_t		*obj,		// Image object
			*mask_obj = NULL;
					// Mask image object, if any
  pdfio_stream_t	*st;		// Image stream
  size_t		x, y,		// X and Y position in image
			bpp,		// Bytes per pixel
			linelen;	// Line length
  const unsigned char	*dataptr;	// Pointer into image data
  unsigned char		*line = NULL,	// Current line
			*lineptr;	// Pointer into line
  static const char	*defcolors[] =	// Default ColorSpace values
  {
    NULL,
    "DeviceGray",
    NULL,
    "DeviceRGB",
    "DeviceCMYK"
  };


  // Range check input...
  if (!pdf || !data || !width || !height || num_colors < 1 || num_colors == 2 || num_colors > 4)
    return (NULL);

  // Allocate memory for one line of data...
  bpp     = alpha ? num_colors + 1 : num_colors;
  linelen = num_colors * width;

  if ((line = malloc(linelen)) == NULL)
    return (NULL);

  // Generate a mask image, as needed...
  if (alpha)
  {
    // Create the image mask dictionary...
    if ((dict = pdfioDictCreate(pdf)) == NULL)
    {
      free(line);
      return (NULL);
    }

    pdfioDictSetName(dict, "Type", "XObject");
    pdfioDictSetName(dict, "Subtype", "Image");
    pdfioDictSetNumber(dict, "Width", width);
    pdfioDictSetNumber(dict, "Height", height);
    pdfioDictSetNumber(dict, "BitsPerComponent", 8);
    pdfioDictSetName(dict, "ColorSpace", "DeviceGray");
    pdfioDictSetName(dict, "Filter", "FlateDecode");

    if ((decode = pdfioDictCreate(pdf)) == NULL)
    {
      free(line);
      return (NULL);
    }

    pdfioDictSetNumber(decode, "BitsPerComponent", 8);
    pdfioDictSetNumber(decode, "Colors", 1);
    pdfioDictSetNumber(decode, "Columns", width);
    pdfioDictSetNumber(decode, "Predictor", _PDFIO_PREDICTOR_PNG_AUTO);
    pdfioDictSetDict(dict, "DecodeParms", decode);

    // Create the mask object and write the mask image...
    if ((mask_obj = pdfioFileCreateObj(pdf, dict)) == NULL)
    {
      free(line);
      return (NULL);
    }

    if ((st = pdfioObjCreateStream(mask_obj, PDFIO_FILTER_FLATE)) == NULL)
    {
      free(line);
      pdfioObjClose(mask_obj);
      return (NULL);
    }

    for (y = height, dataptr = data + num_colors; y > 0; y --)
    {
      for (x = width, lineptr = line; x > 0; x --, dataptr += bpp)
        *lineptr++ = *dataptr;

      pdfioStreamWrite(st, line, width);
    }

    pdfioStreamClose(st);
  }

  // Now create the image...
  if ((dict = pdfioDictCreate(pdf)) == NULL)
  {
    free(line);
    return (NULL);
  }

  pdfioDictSetName(dict, "Type", "XObject");
  pdfioDictSetName(dict, "Subtype", "Image");
  pdfioDictSetBoolean(dict, "Interpolate", interpolate);
  pdfioDictSetNumber(dict, "Width", width);
  pdfioDictSetNumber(dict, "Height", height);
  pdfioDictSetNumber(dict, "BitsPerComponent", 8);
  pdfioDictSetName(dict, "Filter", "FlateDecode");

  if (color_data)
    pdfioDictSetArray(dict, "ColorSpace", color_data);
  else
    pdfioDictSetName(dict, "ColorSpace", defcolors[num_colors]);

  if (mask_obj)
    pdfioDictSetObj(dict, "SMask", mask_obj);

  if ((decode = pdfioDictCreate(pdf)) == NULL)
  {
    free(line);
    return (NULL);
  }

  pdfioDictSetNumber(decode, "BitsPerComponent", 8);
  pdfioDictSetNumber(decode, "Colors", num_colors);
  pdfioDictSetNumber(decode, "Columns", width);
  pdfioDictSetNumber(decode, "Predictor", _PDFIO_PREDICTOR_PNG_AUTO);
  pdfioDictSetDict(dict, "DecodeParms", decode);

  if ((obj = pdfioFileCreateObj(pdf, dict)) == NULL)
  {
    free(line);
    return (NULL);
  }

  if ((st = pdfioObjCreateStream(obj, PDFIO_FILTER_FLATE)) == NULL)
  {
    free(line);
    pdfioObjClose(obj);
    return (NULL);
  }

  for (y = height, dataptr = data; y > 0; y --)
  {
    if (alpha)
    {
      switch (num_colors)
      {
	case 1 :
	    for (x = width, lineptr = line; x > 0; x --, dataptr += bpp)
	      *lineptr++ = *dataptr;
	    break;
	case 3 :
	    for (x = width, lineptr = line; x > 0; x --, dataptr += bpp)
	    {
	      *lineptr++ = dataptr[0];
	      *lineptr++ = dataptr[1];
	      *lineptr++ = dataptr[2];
	    }
	    break;
	case 4 :
	    for (x = width, lineptr = line; x > 0; x --, dataptr += bpp)
	    {
	      *lineptr++ = dataptr[0];
	      *lineptr++ = dataptr[1];
	      *lineptr++ = dataptr[2];
	      *lineptr++ = dataptr[3];
	    }
	    break;
      }

      pdfioStreamWrite(st, line, linelen);
    }
    else
    {
      pdfioStreamWrite(st, dataptr, linelen);
      dataptr += linelen;
    }
  }

  free(line);
  pdfioStreamClose(st);

  return (obj);
}


//
// 'pdfioFileCreateImageObjFromFile()' - Add an image object to a PDF file from a file.
//
// This function creates an image object in a PDF file from a JPEG or PNG file.
// The "filename" parameter specifies the name of the JPEG or PNG file, while
// the "interpolate" parameter specifies whether to interpolate when scaling the
// image on the page.
//
// > Note: Currently PNG support is limited to grayscale, RGB, or indexed files
// > without interlacing or alpha.  Transparency (masking) based on color/index
// > is supported.
//

pdfio_obj_t *				// O - Object
pdfioFileCreateImageObjFromFile(
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
  return (pdfioDictSetObj(font, name, obj));
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
  return (pdfioDictSetObj(xobject, name, obj));
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
  pdfioDictSetArray(dict, "ColorSpace", pdfioArrayCreateColorFromStandard(dict->pdf, num_colors, PDFIO_CS_SRGB));
  pdfioDictSetName(dict, "Filter", "DCTDecode");

  obj = pdfioFileCreateObj(dict->pdf, dict);
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
  pdfio_stream_t *st = NULL;		// Stream for PNG data
  pdfio_dict_t	*decode = NULL;		// Parameters for PNG decode
  ssize_t	bytes;			// Bytes read
  unsigned char	buffer[16384];		// Read buffer
  unsigned	i,			// Looping var
		length,			// Length
		type,			// Chunk code
		crc,			// CRC-32
		temp,			// Temporary value
		width = 0,		// Width
		height = 0;		// Height
  unsigned char	bit_depth = 0,		// Bit depth
		color_type = 0;		// Color type
  double	gamma = 2.2,		// Gamma value
		wx = 0.0, wy = 0.0,	// White point chromacity
		rx = 0.0, ry = 0.0,	// Red chromacity
		gx = 0.0, gy = 0.0,	// Green chromacity
		bx = 0.0, by = 0.0;	// Blue chromacity
  pdfio_array_t	*mask = NULL;		// Color masking array


  // Read the file header...
  if (read(fd, buffer, 8) != 8)
    return (NULL);

  // Then read chunks until we have the image data...
  while (read(fd, buffer, 8) == 8)
  {
    // Get the chunk length and type values...
    length = (unsigned)((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]);
    type   = (unsigned)((buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7]);
    crc    = update_png_crc(0xffffffff, buffer + 4, 4);

    switch (type)
    {
      case _PDFIO_PNG_CHUNK_IDAT : // Image data
          if (!width || !height)
          {
            _pdfioFileError(dict->pdf, "Image data seen in PNG file before header.");
            return (NULL);
	  }

          if (!st)
          {
	    PDFIO_DEBUG("copy_png: wx=%g, wy=%g, rx=%g, ry=%g, gx=%g, gy=%g, bx=%g, by=%g\n", wx, wy, rx, ry, gx, gy, bx, by);
	    PDFIO_DEBUG("copy_png: gamma=%g\n", gamma);

            if (!pdfioDictGetArray(dict, "ColorSpace"))
            {
              PDFIO_DEBUG("copy_png: Adding %s ColorSpace value.\n", color_type == _PDFIO_PNG_TYPE_GRAY ? "CalGray" : "CalRGB");

	      if (wx != 0.0)
		pdfioDictSetArray(dict, "ColorSpace", pdfioArrayCreateColorFromPrimaries(dict->pdf, color_type == _PDFIO_PNG_TYPE_GRAY ? 1 : 3, gamma, wx, wy, rx, ry, gx, gy, bx, by));
              else
		pdfioDictSetArray(dict, "ColorSpace", pdfioArrayCreateColorFromStandard(dict->pdf, color_type == _PDFIO_PNG_TYPE_GRAY ? 1 : 3, PDFIO_CS_SRGB));
            }

	    obj = pdfioFileCreateObj(dict->pdf, dict);

	    if ((st = pdfioObjCreateStream(obj, PDFIO_FILTER_NONE)) == NULL)
	    {
	      pdfioObjClose(obj);
	      return (NULL);
	    }
          }

          while (length > 0)
          {
            if (length > sizeof(buffer))
              bytes = (ssize_t)sizeof(buffer);
	    else
	      bytes = (ssize_t)length;

            if ((bytes = read(fd, buffer, (size_t)bytes)) <= 0)
	    {
	      pdfioStreamClose(st);
	      _pdfioFileError(dict->pdf, "Early end-of-file in image file.");
	      return (NULL);
	    }

            crc = update_png_crc(crc, buffer, (size_t)bytes);

            if (!pdfioStreamWrite(st, buffer, (size_t)bytes))
            {
	      pdfioStreamClose(st);
	      _pdfioFileError(dict->pdf, "Unable to copy image data.");
	      return (NULL);
            }

            length -= bytes;
          }
          break;

      case _PDFIO_PNG_CHUNK_IEND : // Image end
          if (st)
          {
            pdfioStreamClose(st);
            return (obj);
          }
          break;

      case _PDFIO_PNG_CHUNK_IHDR : // Image header
          if (st)
          {
	    pdfioStreamClose(st);
	    _pdfioFileError(dict->pdf, "Unexpected image header.");
	    return (NULL);
          }

          if (length != 13)
          {
	    _pdfioFileError(dict->pdf, "Early end-of-file in image file.");
	    return (NULL);
          }

          if (read(fd, buffer, length) != length)
          {
	    _pdfioFileError(dict->pdf, "Early end-of-file in image file.");
	    return (NULL);
          }

	  crc        = update_png_crc(crc, buffer, length);
	  width      = (unsigned)((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]);
	  height     = (unsigned)((buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7]);
	  bit_depth  = buffer[8];
	  color_type = buffer[9];

	  if (width == 0 || height == 0 || (bit_depth != 1 && bit_depth != 2 && bit_depth != 4 && bit_depth != 8 && bit_depth != 16) || (color_type != _PDFIO_PNG_TYPE_GRAY && color_type != _PDFIO_PNG_TYPE_RGB && color_type != _PDFIO_PNG_TYPE_INDEXED) || buffer[10] || buffer[11] || buffer[12])
	  {
	    _pdfioFileError(dict->pdf, "Unsupported PNG image.");
	    return (NULL);
	  }

	  pdfioDictSetNumber(dict, "Width", width);
	  pdfioDictSetNumber(dict, "Height", height);
	  pdfioDictSetNumber(dict, "BitsPerComponent", bit_depth);
	  pdfioDictSetName(dict, "Filter", "FlateDecode");

	  if ((decode = pdfioDictCreate(dict->pdf)) == NULL)
	    return (NULL);

	  pdfioDictSetNumber(decode, "BitsPerComponent", bit_depth);
	  pdfioDictSetNumber(decode, "Colors", color_type == _PDFIO_PNG_TYPE_RGB ? 3 : 1);
	  pdfioDictSetNumber(decode, "Columns", width);
	  pdfioDictSetNumber(decode, "Predictor", _PDFIO_PREDICTOR_PNG_AUTO);
	  pdfioDictSetDict(dict, "DecodeParms", decode);
          break;

      case _PDFIO_PNG_CHUNK_PLTE : // Palette
          if (length == 0 || (length % 3) != 0 || length > 768)
          {
	    pdfioStreamClose(st);
	    _pdfioFileError(dict->pdf, "Invalid color palette.");
	    return (NULL);
          }

          if (read(fd, buffer, length) != length)
          {
	    _pdfioFileError(dict->pdf, "Early end-of-file in image file.");
	    return (NULL);
          }

	  crc = update_png_crc(crc, buffer, length);

          PDFIO_DEBUG("copy_png: Adding Indexed ColorSpace value.\n");

          pdfioDictSetArray(dict, "ColorSpace", pdfioArrayCreateColorFromPalette(dict->pdf, length / 3, buffer));
          break;

      case _PDFIO_PNG_CHUNK_cHRM : // Cromacities and white point
          if (length != 32)
          {
	    _pdfioFileError(dict->pdf, "Early end-of-file in image file.");
	    return (NULL);
          }

          if (read(fd, buffer, length) != length)
          {
	    _pdfioFileError(dict->pdf, "Early end-of-file in image file.");
	    return (NULL);
          }

	  crc = update_png_crc(crc, buffer, length);

          wx = 0.00001 * ((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]);
          wy = 0.00001 * ((buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7]);
          rx = 0.00001 * ((buffer[8] << 24) | (buffer[9] << 16) | (buffer[10] << 8) | buffer[11]);
          ry = 0.00001 * ((buffer[12] << 24) | (buffer[13] << 16) | (buffer[14] << 8) | buffer[15]);
          gx = 0.00001 * ((buffer[16] << 24) | (buffer[17] << 16) | (buffer[18] << 8) | buffer[19]);
          gy = 0.00001 * ((buffer[20] << 24) | (buffer[21] << 16) | (buffer[22] << 8) | buffer[23]);
          bx = 0.00001 * ((buffer[24] << 24) | (buffer[25] << 16) | (buffer[26] << 8) | buffer[27]);
          by = 0.00001 * ((buffer[28] << 24) | (buffer[29] << 16) | (buffer[30] << 8) | buffer[31]);
          break;

      case _PDFIO_PNG_CHUNK_gAMA : // Gamma correction
          if (length != 4)
          {
	    _pdfioFileError(dict->pdf, "Early end-of-file in image file.");
	    return (NULL);
          }

          if (read(fd, buffer, length) != length)
          {
	    _pdfioFileError(dict->pdf, "Early end-of-file in image file.");
	    return (NULL);
          }

	  crc = update_png_crc(crc, buffer, length);

          gamma = 10000.0 / ((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]);
          break;

      case _PDFIO_PNG_CHUNK_tRNS : // Transparency information
          switch (color_type)
          {
            case _PDFIO_PNG_TYPE_INDEXED :
		if (length > 256)
		{
		  _pdfioFileError(dict->pdf, "Bad transparency chunk in image file.");
		  return (NULL);
		}

		if (read(fd, buffer, length) != length)
		{
		  _pdfioFileError(dict->pdf, "Early end-of-file in image file.");
		  return (NULL);
		}

                for (i = 0; i < length; i ++)
                {
                  if (!buffer[i])
                    break;
                }

                if (i < length)
                {
                  if ((mask = pdfioArrayCreate(dict->pdf)) == NULL)
                    return (NULL);

                  pdfioArrayAppendNumber(mask, i);

		  for (i ++; i < length; i ++)
		  {
		    if (buffer[i])
		      break;
		  }

		  pdfioArrayAppendNumber(mask, i - 1);
		}
		break;

	    case _PDFIO_PNG_TYPE_GRAY :
		if (length != 2)
		{
		  _pdfioFileError(dict->pdf, "Bad transparency chunk in image file.");
		  return (NULL);
		}

		if (read(fd, buffer, length) != length)
		{
		  _pdfioFileError(dict->pdf, "Early end-of-file in image file.");
		  return (NULL);
		}

		if ((mask = pdfioArrayCreate(dict->pdf)) == NULL)
		  return (NULL);

		pdfioArrayAppendNumber(mask, buffer[1]);
		pdfioArrayAppendNumber(mask, buffer[1]);
	        break;
	    case _PDFIO_PNG_TYPE_RGB :
		if (length != 6)
		{
		  _pdfioFileError(dict->pdf, "Bad transparency chunk in image file.");
		  return (NULL);
		}

		if (read(fd, buffer, length) != length)
		{
		  _pdfioFileError(dict->pdf, "Early end-of-file in image file.");
		  return (NULL);
		}

		if ((mask = pdfioArrayCreate(dict->pdf)) == NULL)
		  return (NULL);

		pdfioArrayAppendNumber(mask, buffer[1]);
		pdfioArrayAppendNumber(mask, buffer[3]);
		pdfioArrayAppendNumber(mask, buffer[5]);
		pdfioArrayAppendNumber(mask, buffer[1]);
		pdfioArrayAppendNumber(mask, buffer[3]);
		pdfioArrayAppendNumber(mask, buffer[5]);
	        break;
	  }

	  crc = update_png_crc(crc, buffer, length);

          if (mask)
            pdfioDictSetArray(dict, "Mask", mask);
          break;

      default : // Something else
          while (length > 0)
          {
            if (length > sizeof(buffer))
              bytes = (ssize_t)sizeof(buffer);
	    else
	      bytes = (ssize_t)length;

            if ((bytes = read(fd, buffer, (size_t)bytes)) <= 0)
	    {
	      pdfioStreamClose(st);
	      _pdfioFileError(dict->pdf, "Early end-of-file in image file.");
	      return (NULL);
	    }

            crc    = update_png_crc(crc, buffer, (size_t)bytes);
            length -= bytes;
          }
          break;
    }

    // Verify the CRC...
    crc ^= 0xffffffff;

    if (read(fd, buffer, 4) != 4)
    {
      pdfioStreamClose(st);
      _pdfioFileError(dict->pdf, "Unable to read CRC.");
      return (NULL);
    }

    temp = (unsigned)((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]);
    if (temp != crc)
    {
      pdfioStreamClose(st);
      _pdfioFileError(dict->pdf, "Bad CRC (0x%08x != 0x%08x).", temp, crc);
      return (NULL);
    }
  }

  return (NULL);
}


//
// 'create_cp1252()' - Create the CP1252 font encoding object.
//

static bool				// O - `true` on success, `false` on failure
create_cp1252(pdfio_file_t *pdf)	// I - PDF file
{
  int		ch;			// Current character
  bool		chindex;		// Need character index?
  pdfio_dict_t	*cp1252_dict;		// Encoding dictionary
  pdfio_array_t	*cp1252_array;		// Differences array
  static const char * const cp1252[] =	// Glyphs for CP1252 encoding
  {
    "space",
    "exclam",
    "quotedbl",
    "numbersign",
    "dollar",
    "percent",
    "ampersand",
    "quotesingle",
    "parenleft",
    "parenright",
    "asterisk",
    "plus",
    "comma",
    "hyphen",
    "period",
    "slash",
    "zero",
    "one",
    "two",
    "three",
    "four",
    "five",
    "six",
    "seven",
    "eight",
    "nine",
    "colon",
    "semicolon",
    "less",
    "equal",
    "greater",
    "question",
    "at",
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "bracketleft",
    "backslash",
    "bracketright",
    "asciicircum",
    "underscore",
    "grave",
    "a",
    "b",
    "c",
    "d",
    "e",
    "f",
    "g",
    "h",
    "i",
    "j",
    "k",
    "l",
    "m",
    "n",
    "o",
    "p",
    "q",
    "r",
    "s",
    "t",
    "u",
    "v",
    "w",
    "x",
    "y",
    "z",
    "braceleft",
    "bar",
    "braceright",
    "asciitilde",
    "",
    "Euro",
    "",
    "quotesinglbase",
    "florin",
    "quotedblbase",
    "ellipsis",
    "dagger",
    "daggerdbl",
    "circumflex",
    "perthousand",
    "Scaron",
    "guilsinglleft",
    "OE",
    "",
    "Zcaron",
    "",
    "",
    "quoteleft",
    "quoteright",
    "quotedblleft",
    "quotedblright",
    "bullet",
    "endash",
    "emdash",
    "tilde",
    "trademark",
    "scaron",
    "guilsinglright",
    "oe",
    "",
    "zcaron",
    "Ydieresis",
    "space",
    "exclamdown",
    "cent",
    "sterling",
    "currency",
    "yen",
    "brokenbar",
    "section",
    "dieresis",
    "copyright",
    "ordfeminine",
    "guillemotleft",
    "logicalnot",
    "minus",
    "registered",
    "macron",
    "degree",
    "plusminus",
    "twosuperior",
    "threesuperior",
    "acute",
    "mu",
    "paragraph",
    "periodcentered",
    "cedilla",
    "onesuperior",
    "ordmasculine",
    "guillemotright",
    "onequarter",
    "onehalf",
    "threequarters",
    "questiondown",
    "Agrave",
    "Aacute",
    "Acircumflex",
    "Atilde",
    "Adieresis",
    "Aring",
    "AE",
    "Ccedilla",
    "Egrave",
    "Eacute",
    "Ecircumflex",
    "Edieresis",
    "Igrave",
    "Iacute",
    "Icircumflex",
    "Idieresis",
    "Eth",
    "Ntilde",
    "Ograve",
    "Oacute",
    "Ocircumflex",
    "Otilde",
    "Odieresis",
    "multiply",
    "Oslash",
    "Ugrave",
    "Uacute",
    "Ucircumflex",
    "Udieresis",
    "Yacute",
    "Thorn",
    "germandbls",
    "agrave",
    "aacute",
    "acircumflex",
    "atilde",
    "adieresis",
    "aring",
    "ae",
    "ccedilla",
    "egrave",
    "eacute",
    "ecircumflex",
    "edieresis",
    "igrave",
    "iacute",
    "icircumflex",
    "idieresis",
    "eth",
    "ntilde",
    "ograve",
    "oacute",
    "ocircumflex",
    "otilde",
    "odieresis",
    "divide",
    "oslash",
    "ugrave",
    "uacute",
    "ucircumflex",
    "udieresis",
    "yacute",
    "thorn",
    "ydieresis"
  };


  if ((cp1252_dict = pdfioDictCreate(pdf)) == NULL || (cp1252_array = pdfioArrayCreate(pdf)) == NULL)
    return (false);

  for (ch = 0, chindex = true; ch < (int)(sizeof(cp1252) / sizeof(cp1252[0])); ch ++)
  {
    if (cp1252[ch][0])
    {
      // Add this character...
      if (chindex)
      {
	// Add the initial index...
	pdfioArrayAppendNumber(cp1252_array, ch + 32);
	chindex = false;
      }

      pdfioArrayAppendName(cp1252_array, cp1252[ch]);
    }
    else
    {
      // Flag that we need a new index...
      chindex = true;
    }
  }

  pdfioDictSetName(cp1252_dict, "Type", "Encoding");
  pdfioDictSetArray(cp1252_dict, "Differences", cp1252_array);

  if ((pdf->cp1252_obj = pdfioFileCreateObj(pdf, cp1252_dict)) == NULL)
    return (false);

  pdfioObjClose(pdf->cp1252_obj);

  return (true);
}


//
// 'ttf_error_cb()' - Relay a message from the TTF functions.
//

static void
ttf_error_cb(pdfio_file_t *pdf,		// I - PDF file
             const char   *message)	// I - Error message
{
  (pdf->error_cb)(pdf, message, pdf->error_data);
}


//
// 'update_png_crc()' - Update the CRC-32 value for a PNG chunk.
//

static unsigned				// O - CRC-32 value
update_png_crc(
    unsigned            crc,		// I - CRC-32 value
    const unsigned char *buffer,	// I - Buffer
    size_t              length)		// I - Length of buffer
{
  while (length > 0)
  {
    crc = png_crc_table[(crc ^ *buffer) & 0xff] ^ (crc >> 8);
    buffer ++;
    length --;
  }

  return (crc);
}


//
// 'write_string()' - Write a PDF string.
//

static bool				// O - `true` on success, `false` otherwise
write_string(pdfio_stream_t *st,	// I - Stream
             bool           unicode,	// I - Unicode text?
             const char     *s,		// I - String
             bool           *newline)	// O - Ends with a newline?
{
  int		ch;			// Unicode character
  const char	*ptr;			// Pointer into string


  // Start the string...
  if (!pdfioStreamPuts(st, unicode ? "<" : "("))
    return (false);

  // Loop through the string, handling UTF-8 as needed...
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

    if (unicode)
    {
      // Write a two-byte character...
      if (!pdfioStreamPrintf(st, "%04X", ch))
	return (false);
    }
    else
    {
      // Write a one-byte character...
      if (ch == '\\' || ch == '(' || ch == ')' || ch < ' ')
      {
        // Escaped character...
        if (ch < ' ')
        {
          if (!pdfioStreamPrintf(st, "\\%03o", ch))
            return (false);
        }
        else if (!pdfioStreamPrintf(st, "\\%c", ch))
          return (false);
      }
      else
      {
        // Non-escaped character...
        if (ch > 255)
	{
	  // Try mapping from Unicode to CP1252...
	  int i;				// Looping var

	  for (i = 0; i < (int)(sizeof(_pdfio_cp1252) / sizeof(_pdfio_cp1252[0])); i ++)
	  {
	    if (ch == _pdfio_cp1252[i])
	    {
	      ch = i + 128;
	      break;
	    }
	  }

	  if (ch > 255)
	    ch = '?';				// Unsupported chars map to ?
	}

        // Write the character...
        pdfioStreamPutChar(st, ch);
      }
    }
  }

  return (pdfioStreamPuts(st, unicode ? ">" : ")"));
}
