//
// Content helper functions for PDFio.
//
// Copyright © 2021-2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pdfio-private.h"
#include "pdfio-content.h"
#include "pdfio-base-font-widths.h"
#include "pdfio-cgats001-compat.h"
#include "ttf.h"
#ifdef HAVE_LIBPNG
#  include <png.h>
#endif // HAVE_LIBPNG
#include <math.h>
#ifndef M_PI
#  define M_PI	3.14159265358979323846264338327950288
#endif // M_PI


//
// Local constants...
//

#define _PDFIO_JPEG_SOF0	0xc0	// Start of frame (0)
#define _PDFIO_JPEG_SOF1	0xc1	// Start of frame (1)
#define _PDFIO_JPEG_SOF2	0xc2	// Start of frame (2)
#define _PDFIO_JPEG_SOF3	0xc3	// Start of frame (3)
#define _PDFIO_JPEG_SOF5	0xc5	// Start of frame (5)
#define _PDFIO_JPEG_SOF6	0xc6	// Start of frame (6)
#define _PDFIO_JPEG_SOF7	0xc7	// Start of frame (7)
#define _PDFIO_JPEG_SOF9	0xc9	// Start of frame (9)
#define _PDFIO_JPEG_SOF10	0xca	// Start of frame (10)
#define _PDFIO_JPEG_SOF11	0xcb	// Start of frame (11)
#define _PDFIO_JPEG_SOF13	0xcd	// Start of frame (13)
#define _PDFIO_JPEG_SOF14	0xce	// Start of frame (14)
#define _PDFIO_JPEG_SOF15	0xcf	// Start of frame (15)

#define _PDFIO_JPEG_SOI		0xd8	// Start of image
#define _PDFIO_JPEG_EOI		0xd9	// End of image
#define _PDFIO_JPEG_SOS		0xda	// Start of scan

#define _PDFIO_JPEG_APP0	0xe0	// APP0 extension
#define _PDFIO_JPEG_APP1	0xe1	// APP1 extension
#define _PDFIO_JPEG_APP2	0xe2	// APP2 extension

#define _PDFIO_JPEG_MARKER	0xff

#define _PDFIO_PNG_CHUNK_IDAT	0x49444154	// Image data
#define _PDFIO_PNG_CHUNK_IEND	0x49454e44	// Image end
#define _PDFIO_PNG_CHUNK_IHDR	0x49484452	// Image header
#define _PDFIO_PNG_CHUNK_PLTE	0x504c5445	// Palette
#define _PDFIO_PNG_CHUNK_cHRM	0x6348524d	// Cromacities and white point
#define _PDFIO_PNG_CHUNK_gAMA	0x67414d41	// Gamma correction
#define _PDFIO_PNG_CHUNK_tRNS	0x74524e53	// Transparency information

#define _PDFIO_PNG_COMPRESSION_FLATE 0	// Flate compression

#define _PDFIO_PNG_FILTER_ADAPTIVE 0	// Adaptive filtering

#define _PDFIO_PNG_INTERLACE_NONE 0	// No interlacing
#define _PDFIO_PNG_INTERLACE_ADAM 1	// "Adam7" interlacing

#define _PDFIO_PNG_TYPE_GRAY	0	// Grayscale
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
static bool		create_cp1252(pdfio_file_t *pdf);
static pdfio_obj_t	*create_font(pdfio_obj_t *file_obj, ttf_t *font, bool unicode);
static pdfio_obj_t	*create_image(pdfio_dict_t *dict, const unsigned char *data, size_t width, size_t height, size_t depth, size_t num_colors, bool alpha);
#ifdef HAVE_LIBPNG
static void		png_error_func(png_structp pp, png_const_charp message);
static void		png_read_func(png_structp png_ptr, png_bytep data, size_t length);
#endif // HAVE_LIBPNG
static void		ttf_error_cb(pdfio_file_t *pdf, const char *message);
#ifndef HAVE_LIBPNG
static unsigned		update_png_crc(unsigned crc, const unsigned char *buffer, size_t length);
#endif // !HAVE_LIBPNG
static bool		write_array(pdfio_stream_t *st, pdfio_array_t *a);
static bool		write_dict(pdfio_stream_t *st, pdfio_dict_t *dict);
static bool		write_string(pdfio_stream_t *st, bool unicode, const char *s, bool *newline);
static bool		write_value(pdfio_stream_t *st, _pdfio_value_t *v);


//
// Local globals...
//

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

#ifndef HAVE_LIBPNG
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
#endif // !HAVE_LIBPNG


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
  pdfioArrayAppendNumber(indexed_color, (double)(num_colors - 1));
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


  PDFIO_DEBUG("pdfioFileCreateCalibratedColorFromPrimaries(pdf=%p, num_colors=%lu, gamma=%.6f, wx=%.6f, wy=%.6f, rx=%.6f, ry=%.6f, gx=%.6f, gy=%.6f, bx=%.6f, by=%.6f)\n", (void *)pdf, (unsigned long)num_colors, gamma, wx, wy, rx, ry, gx, gy, bx, by);

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

  PDFIO_DEBUG("pdfioFileCreateCalibratedColorFromPrimaries: white_point=[%.6f %.6f %.6f]\n", white_point[0], white_point[1], white_point[2]);
  PDFIO_DEBUG("pdfioFileCreateCalibratedColorFromPrimaries: matrix=[%.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f]\n", matrix[0][0], matrix[1][0], matrix[2][0], matrix[0][1], matrix[1][1], matrix[2][2], matrix[0][2], matrix[1][2], matrix[2][1]);

  // Now that we have the white point and matrix, use those to make the color array...
  return (pdfioArrayCreateColorFromMatrix(pdf, num_colors, gamma, matrix, white_point));
}


//
// 'pdfioArrayCreateColorFromStandard()' - Create a color array for a standard color space.
//
// This function creates a color array for a standard `PDFIO_CS_` enumerated color space.
// The "num_colors" argument must be `1` for grayscale, `3` for RGB color, and
// `4` for CMYK color.
//

pdfio_array_t *				// O - Color array
pdfioArrayCreateColorFromStandard(
    pdfio_file_t *pdf,			// I - PDF file
    size_t       num_colors,		// I - Number of colors (1, 3, or 4)
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
  else if ((cs != PDFIO_CS_CGATS001 && num_colors != 1 && num_colors != 3) || (cs == PDFIO_CS_CGATS001 && num_colors != 4))
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

    case PDFIO_CS_CGATS001 :
        if (!pdf->cgats001_obj)
          pdf->cgats001_obj = pdfioFileCreateICCObjFromData(pdf, CGATS001Compat_v2_micro_icc, sizeof(CGATS001Compat_v2_micro_icc), num_colors);

        return (pdfioArrayCreateColorFromICCObj(pdf, pdf->cgats001_obj));

    default :
        _pdfioFileError(pdf, "Unsupported color space number %d.", (int)cs);
        return (NULL);
  }
}


//
// 'pdfioContentBeginMarked()' - Start marked content with an optional dictionary.
//
// This function starts an area of marked content with an optional dictionary.
// It must be paired with a call to the @link pdfioContentEndMarked@ function.
//
// @since PDFio 1.6@
//

bool					// O - `true` on success, `false` on failure
pdfioContentBeginMarked(
    pdfio_stream_t *st,			// I - Stream
    const char     *name,		// I - Name of marked content
    pdfio_dict_t   *dict)		// I - Dictionary of parameters or `NULL` if none
{
  if (!st || !name)
    return (false);

  // Send the BDC/BMC command...
  if (!pdfioStreamPrintf(st, "%N", name))
    return (false);

  if (dict)
  {
    // Write dictionary before BDC operator...
    if (!write_dict(st, dict))
      return (false);

    if (!pdfioStreamPuts(st, "BDC\n"))
      return (false);
  }
  else
  {
    // No dictionary so use the BMC operator...
    if (!pdfioStreamPuts(st, " BMC\n"))
      return (false);
  }

  // Make sure we have the MarkInfo dictionary in the catalog...
  if (!st->pdf->markinfo)
  {
    st->pdf->markinfo = pdfioDictCreate(st->pdf);
    pdfioDictSetBoolean(st->pdf->markinfo, "Marked", true);
    pdfioDictSetDict(pdfioObjGetDict(st->pdf->root_obj), "MarkInfo", st->pdf->markinfo);
  }

  return (true);
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
  return (pdfioStreamPrintf(st, "q %.6f 0 0 %.6f %.6f %.6f cm%N Do Q\n", width, height, x, y, name));
}


//
// 'pdfioContentEndMarked()' - End marked content.
//
// This function ends an area of marked content that was started using the
// @link pdfioContentBeginMarked@ function.
//
// @since PDFio 1.6@
//

bool					// O - `true` on success, `false` on failure
pdfioContentEndMarked(
    pdfio_stream_t *st)			// I - Stream
{
  return (pdfioStreamPuts(st, "EMC\n"));
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
  return (pdfioStreamPrintf(st, "%.6f %.6f %.6f %.6f %.6f %.6f cm\n", m[0][0], m[0][1], m[1][0], m[1][1], m[2][0], m[2][1]));
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


  return (pdfioStreamPrintf(st, "%.6f %.6f %.6f %.6f 0 0 cm\n", dcos, -dsin, dsin, dcos));
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
  return (pdfioStreamPrintf(st, "%.6f 0 0 %.6f 0 0 cm\n", sx, sy));
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
  return (pdfioStreamPrintf(st, "1 0 0 1 %.6f %.6f cm\n", tx, ty));
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
  return (pdfioStreamPrintf(st, "%.6f %.6f %.6f %.6f %.6f %.6f c\n", x1, y1, x2, y2, x3, y3));
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
  return (pdfioStreamPrintf(st, "%.6f %.6f %.6f %.6f v\n", x1, y1, x3, y3));
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
  return (pdfioStreamPrintf(st, "%.6f %.6f %.6f %.6f y\n", x2, y2, x3, y3));
}


//
// 'pdfioContentPathEnd()' - Clear the current path.
//
// @since PDFio v1.1@
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
  return (pdfioStreamPrintf(st, "%.6f %.6f l\n", x, y));
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
  return (pdfioStreamPrintf(st, "%.6f %.6f m\n", x, y));
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
  return (pdfioStreamPrintf(st, "%.6f %.6f %.6f %.6f re\n", x, y, width, height));
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
// This function sets the stroke pattern when drawing lines.  If "on" and "off"
// are 0, a solid line is drawn.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetDashPattern(
    pdfio_stream_t *st,			// I - Stream
    double         phase,		// I - Phase (offset within pattern)
    double         on,			// I - On length
    double         off)			// I - Off length
{
  if (on <= 0.0 && off <= 0.0)
    return (pdfioStreamPrintf(st, "[] %.6f d\n", phase));
  else if (fabs(on - off) < 0.001)
    return (pdfioStreamPrintf(st, "[%.6f] %.6f d\n", on, phase));
  else
    return (pdfioStreamPrintf(st, "[%.6f %.6f] %.6f d\n", on, off, phase));
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
  return (pdfioStreamPrintf(st, "%.6f %.6f %.6f %.6f k\n", c, m, y, k));
}


//
// 'pdfioContentSetFillColorDeviceGray()' - Set the device gray fill color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetFillColorDeviceGray(
    pdfio_stream_t *st,			// I - Stream
    double         g)			// I - Gray value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%.6f g\n", g));
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
  return (pdfioStreamPrintf(st, "%.6f %.6f %.6f rg\n", r, g, b));
}


//
// 'pdfioContentSetFillColorGray()' - Set the calibrated gray fill color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetFillColorGray(
    pdfio_stream_t *st,			// I - Stream
    double         g)			// I - Gray value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%.6f sc\n", g));
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
  return (pdfioStreamPrintf(st, "%.6f %.6f %.6f sc\n", r, g, b));
}


//
// 'pdfioContentSetFillColorSpace()' - Set the fill colorspace.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetFillColorSpace(
    pdfio_stream_t *st,			// I - Stream
    const char     *name)		// I - Color space name
{
  return (pdfioStreamPrintf(st, "%N cs\n", name));
}


//
// 'pdfioContentSetFlatness()' - Set the flatness tolerance.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetFlatness(
    pdfio_stream_t *st,			// I - Stream
    double         flatness)		// I - Flatness value (0.0 to 100.0)
{
  return (pdfioStreamPrintf(st, "%.6f i\n", flatness));
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
  return (pdfioStreamPrintf(st, "%.6f w\n", width));
}


//
// 'pdfioContentSetMiterLimit()' - Set the miter limit.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetMiterLimit(
    pdfio_stream_t *st,			// I - Stream
    double         limit)		// I - Miter limit value
{
  return (pdfioStreamPrintf(st, "%.6f M\n", limit));
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
  return (pdfioStreamPrintf(st, "%.6f %.6f %.6f %.6f K\n", c, m, y, k));
}


//
// 'pdfioContentSetStrokeColorDeviceGray()' - Set the device gray stroke color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetStrokeColorDeviceGray(
    pdfio_stream_t *st,			// I - Stream
    double         g)			// I - Gray value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%.6f G\n", g));
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
  return (pdfioStreamPrintf(st, "%.6f %.6f %.6f RG\n", r, g, b));
}


//
// 'pdfioContentSetStrokeColorGray()' - Set the calibrated gray stroke color.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetStrokeColorGray(
    pdfio_stream_t *st,			// I - Stream
    double         g)			// I - Gray value (0.0 to 1.0)
{
  return (pdfioStreamPrintf(st, "%.6f SC\n", g));
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
  return (pdfioStreamPrintf(st, "%.6f %.6f %.6f SC\n", r, g, b));
}


//
// 'pdfioContentSetStrokeColorSpace()' - Set the stroke color space.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetStrokeColorSpace(
    pdfio_stream_t *st,			// I - Stream
    const char     *name)		// I - Color space name
{
  return (pdfioStreamPrintf(st, "%N CS\n", name));
}


//
// 'pdfioContentSetTextCharacterSpacing()' - Set the spacing between characters.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetTextCharacterSpacing(
    pdfio_stream_t *st,			// I - Stream
    double         spacing)		// I - Character spacing
{
  return (pdfioStreamPrintf(st, "%.6f Tc\n", spacing));
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
  return (pdfioStreamPrintf(st, "%N %.6f Tf\n", name, size));
}


//
// 'pdfioContentSetTextLeading()' - Set text leading (line height) value.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetTextLeading(
    pdfio_stream_t *st,			// I - Stream
    double         leading)		// I - Leading (line height) value
{
  return (pdfioStreamPrintf(st, "%.6f TL\n", leading));
}


//
// 'pdfioContentSetTextMatrix()' - Set the text transform matrix.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetTextMatrix(
    pdfio_stream_t *st,			// I - Stream
    pdfio_matrix_t m)			// I - Transform matrix
{
  return (pdfioStreamPrintf(st, "%.6f %.6f %.6f %.6f %.6f %.6f Tm\n", m[0][0], m[0][1], m[1][0], m[1][1], m[2][0], m[2][1]));
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
  return (pdfioStreamPrintf(st, "%.6f Ts\n", rise));
}


//
// 'pdfioContentSetTextWordSpacing()' - Set the inter-word spacing.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetTextWordSpacing(
    pdfio_stream_t *st,			// I - Stream
    double         spacing)		// I - Spacing between words
{
  return (pdfioStreamPrintf(st, "%.6f Tw\n", spacing));
}


//
// 'pdfioContentSetTextXScaling()' - Set the horizontal scaling value.
//

bool					// O - `true` on success, `false` on failure
pdfioContentSetTextXScaling(
    pdfio_stream_t *st,			// I - Stream
    double         percent)		// I - Horizontal scaling in percent
{
  return (pdfioStreamPrintf(st, "%.6f Tz\n", percent));
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
// 'pdfioContentTextMeasure()' - Measure a text string and return its width.
//
// This function measures the given text string "s" and returns its width based
// on "size". The text string must always use the UTF-8 (Unicode) encoding but
// any control characters (such as newlines) are ignored.
//
// @since PDFio v1.2@
//

double					// O - Width
pdfioContentTextMeasure(
    pdfio_obj_t *font,			// I - Font object created by @link pdfioFileCreateFontObjFromFile@
    const char  *s,			// I - UTF-8 string
    double      size)			// I - Font size/height
{
  const char	*basefont;		// Base font name
  ttf_t		*ttf = (ttf_t *)_pdfioObjGetExtension(font);
					// TrueType font data
  ttf_rect_t	extents;		// Text extents
  int		ch;			// Unicode character
  char		temp[1024],		// Temporary string
		*tempptr;		// Pointer into temporary string


  if (!ttf && (basefont = pdfioDictGetName(pdfioObjGetDict(font), "BaseFont")) != NULL)
  {
    // Measure the width using the compiled-in base font tables...
    const short	*widths;		// Widths
    int		width = 0;		// Current width

    if (strcmp(basefont, "Symbol") && strcmp(basefont, "Zapf-Dingbats"))
    {
      // Map non-CP1282 characters to '?', everything else as-is...
      tempptr = temp;

      while (*s && tempptr < (temp + sizeof(temp) - 3))
      {
	if ((*s & 0xe0) == 0xc0)
	{
	  // Two-byte UTF-8
	  ch = ((s[0] & 0x1f) << 6) | (s[1] & 0x3f);
	  s += 2;
	}
	else if ((*s & 0xf0) == 0xe0)
	{
	  // Three-byte UTF-8
	  ch = ((s[0] & 0x0f) << 12) | ((s[1] & 0x3f) << 6) | (s[2] & 0x3f);
	  s += 3;
	}
	else if ((*s & 0xf8) == 0xf0)
	{
	  // Four-byte UTF-8
	  ch = ((s[0] & 0x07) << 18) | ((s[1] & 0x3f) << 12) | ((s[2] & 0x3f) << 6) | (s[3] & 0x3f);
	  s += 4;
	}
	else
	{
	  ch = *s++;
	}

	if (ch > 255)
	{
	  // Try mapping from Unicode to CP1252...
	  size_t i;			// Looping var

	  for (i = 0; i < (sizeof(_pdfio_cp1252) / sizeof(_pdfio_cp1252[0])); i ++)
	  {
	    if (ch == _pdfio_cp1252[i])
	      break;
	  }

	  if (i < (sizeof(_pdfio_cp1252) / sizeof(_pdfio_cp1252[0])))
	    ch = (int)(i + 0x80);	// Extra characters from 0x80 to 0x9f
	  else
	    ch = '?';			// Unsupported chars map to ?
	}

	*tempptr++ = (char)ch;
      }

      *tempptr = '\0';
      s        = temp;
    }

    // Choose the appropriate table...
    if (!strcmp(basefont, "Courier"))
      widths = courier_widths;
    else if (!strcmp(basefont, "Courier-Bold"))
      widths = courier_bold_widths;
    else if (!strcmp(basefont, "Courier-BoldOblique"))
      widths = courier_boldoblique_widths;
    else if (!strcmp(basefont, "Courier-Oblique"))
      widths = courier_oblique_widths;
    else if (!strcmp(basefont, "Helvetica"))
      widths = helvetica_widths;
    else if (!strcmp(basefont, "Helvetica-Bold"))
      widths = helvetica_bold_widths;
    else if (!strcmp(basefont, "Helvetica-BoldOblique"))
      widths = helvetica_boldoblique_widths;
    else if (!strcmp(basefont, "Helvetica-Oblique"))
      widths = helvetica_oblique_widths;
    else if (!strcmp(basefont, "Symbol"))
      widths = symbol_widths;
    else if (!strcmp(basefont, "Times-Bold"))
      widths = times_bold_widths;
    else if (!strcmp(basefont, "Times-BoldItalic"))
      widths = times_bolditalic_widths;
    else if (!strcmp(basefont, "Times-Italic"))
      widths = times_italic_widths;
    else if (!strcmp(basefont, "Times-Roman"))
      widths = times_roman_widths;
    else if (!strcmp(basefont, "ZapfDingbats"))
      widths = zapfdingbats_widths;
    else
      return (0.0);

    // Calculate the width using the corresponding table...
    while (*s)
    {
      width += widths[*s & 255];
      s ++;
    }

    return (size * 0.001 * width);
  }

  // If we get here then we need to measure using the TrueType library...
  ttfGetExtents(ttf, (float)size, s, &extents);

  return (extents.right - extents.left);
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
  return (pdfioStreamPrintf(st, "%.6f %.6f TD\n", tx, ty));
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
  return (pdfioStreamPrintf(st, "%.6f %.6f Td\n", tx, ty));
}


//
// 'pdfioContentTextNewLine()' - Move to the next line.
//
// @since PDFio v1.2@
//

bool					// O - `true` on success, `false` on failure
pdfioContentTextNewLine(
    pdfio_stream_t *st)			// I - Stream
{
  return (pdfioStreamPuts(st, "T*\n"));
}


//
// 'pdfioContentTextNextLine()' - Legacy function name preserved for binary compatibility.
//
// @private@
//

bool					// O - `true` on success, `false` on failure
pdfioContentTextNextLine(
    pdfio_stream_t *st)			// I - Stream
{
  return (pdfioStreamPuts(st, "T*\n"));
}


//
// 'pdfioContentTextNewLineShow()' - Move to the next line and show text.
//
// This function moves to the next line and then shows some text with optional
// word and character spacing in a PDF content stream. The "unicode" argument
// specifies that the current font maps to full Unicode.  The "s" argument
// specifies a UTF-8 encoded string.
//
// @since PDFio v1.2@
//

bool					// O - `true` on success, `false` on failure
pdfioContentTextNewLineShow(
    pdfio_stream_t *st,			// I - Stream
    double         ws,			// I - Word spacing or `0.0` for none
    double         cs,			// I - Character spacing or `0.0` for none
    bool           unicode,		// I - Unicode text?
    const char     *s)			// I - String to show
{
  bool	newline = false;		// New line?
  char	op;				// Text operator


  // Write word and/or character spacing as needed...
  if (ws > 0.0 || cs > 0.0)
  {
    // Use " operator to show text with word and character spacing...
    if (!pdfioStreamPrintf(st, "%.6f %.6f", ws, cs))
      return (false);

    op = '\"';
  }
  else
  {
    // Use ' operator to show text with the defaults...
    op = '\'';
  }

  // Write the string...
  if (!write_string(st, unicode, s, &newline))
    return (false);

  // Draw it...
  if (newline)
    return (pdfioStreamPrintf(st, "%c T*\n", op));
  else
    return (pdfioStreamPrintf(st, "%c\n", op));
}


//
// 'pdfioContentTextNewLineShowf()' - Show formatted text.
//
// This function moves to the next line and shows some formatted text with
// optional word and character spacing in a PDF content stream. The "unicode"
// argument specifies that the current font maps to full Unicode.  The "format"
// argument specifies a UTF-8 encoded `printf`-style format string.
//
// @since PDFio v1.2@
//

bool					// O - `true` on success, `false` on failure
pdfioContentTextNewLineShowf(
    pdfio_stream_t *st,			// I - Stream
    double         ws,			// I - Word spacing or `0.0` for none
    double         cs,			// I - Character spacing or `0.0` for none
    bool           unicode,		// I - Unicode text?
    const char     *format,		// I - `printf`-style format string
    ...)				// I - Additional arguments as needed
{
  char		buffer[8192];		// Text buffer
  va_list	ap;			// Argument pointer


  // Format the string...
  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  // Show it...
  return (pdfioContentTextNewLineShow(st, ws, cs, unicode, buffer));
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
// This function shows some formatted text in a PDF content stream. The
// "unicode" argument specifies that the current font maps to full Unicode.
// The "format" argument specifies a UTF-8 encoded `printf`-style format string.
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
      if (!pdfioStreamPrintf(st, "%.6f", offsets[i]))
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
// 'pdfioFileAddOutputIntent()' - Add an OutputIntent to a file.
//
// This function adds an OutputIntent dictionary to the PDF file catalog.
// The "subtype" argument specifies the intent subtype and is typically
// "GTS_PDFX" for PDF/X, "GTS_PDFA1" for PDF/A, or "ISO_PDFE1" for PDF/E.
// Passing `NULL` defaults the subtype to "GTS_PDFA1".
//
// The "condition" argument specifies a short name for the output intent, while
// the "info" argument specifies a longer description for the output intent.
// Both can be `NULL` to omit this information.
//
// The "cond_id" argument specifies a unique identifier such as a registration
// ("CGATS001") or color space name ("sRGB").  The "reg_name" argument provides
// a URL for the identifier.
//
// The "profile" argument specifies an ICC profile object for the output
// condition.  If `NULL`, the PDF consumer will attempt to look up the correct
// profile using the "cond_id" value.
//
// @since PDFio 1.6@
//

void
pdfioFileAddOutputIntent(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *subtype,		// I - Intent subtype (standard)
    const char   *condition,		// I - Condition name or `NULL` for none
    const char   *cond_id,		// I - Identifier such as registration name or `NULL` for none
    const char   *reg_name,		// I - Registry URL or `NULL` for none
    const char   *info,			// I - Description or `NULL` for none
    pdfio_obj_t  *profile)		// I - ICC profile object or `NULL` for none
{
  pdfio_array_t	*output_intents;	// OutputIntents array in catalog
  pdfio_dict_t	*intent;		// Current output intent


  // Range check input...
  if (!pdf)
    return;

  if (!subtype)
  {
    _pdfioFileError(pdf, "Output intent subtype cannot be NULL.");
    return;
  }

  // Get the OutputIntents array...
  if ((output_intents = pdfioDictGetArray(pdfioFileGetCatalog(pdf), "OutputIntents")) != NULL)
  {
    // See if we already have an intent for the given subtype...
    size_t	i,			// Looping var
		count;			// Number of output intents

    for (i = 0, count = pdfioArrayGetSize(output_intents); i < count; i ++)
    {
      if ((intent = pdfioArrayGetDict(output_intents, i)) != NULL)
      {
        const char *csubtype = pdfioDictGetName(intent, "S");
					// Current subtype

        if (csubtype && !strcmp(csubtype, subtype))
          return;
      }
    }
  }
  else
  {
    // Create the OutputIntents array...
    if ((output_intents = pdfioArrayCreate(pdf)) == NULL)
      return;

    pdfioDictSetArray(pdfioFileGetCatalog(pdf), "OutputIntents", output_intents);
  }

  // Create an intent dictionary...
  if ((intent = pdfioDictCreate(pdf)) == NULL)
    return;

  pdfioDictSetName(intent, "Type", "OutputIntent");
  pdfioDictSetName(intent, "S", pdfioStringCreate(pdf, subtype));
  if (condition)
    pdfioDictSetString(intent, "OutputCondition", pdfioStringCreate(pdf, condition));
  if (cond_id)
    pdfioDictSetString(intent, "OutputConditionIdentifier", pdfioStringCreate(pdf, cond_id));
  if (reg_name)
    pdfioDictSetString(intent, "RegistryName", pdfioStringCreate(pdf, reg_name));
  if (info)
    pdfioDictSetString(intent, "Info", pdfioStringCreate(pdf, info));
  if (profile)
    pdfioDictSetObj(intent, "DestOutputProfile", profile);

  // Add the dictionary to the output intents...
  pdfioArrayAppendDict(output_intents, intent);
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
// Aside from "Symbol" and "Zapf-Dingbats", Base fonts use the Windows CP1252
// (ISO-8859-1 with additional characters such as the Euro symbol) subset of
// Unicode.
//

pdfio_obj_t *				// O - Font object
pdfioFileCreateFontObjFromBase(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *name)			// I - Font name
{
  pdfio_dict_t	*dict;			// Font dictionary
  pdfio_obj_t	*obj;			// Font object

  if (pdf && pdf->pdfa != _PDFIO_PDFA_NONE)
  {
    _pdfioFileError(pdf, "Base fonts are not allowed in PDF/A files; use pdfioFileCreateFontObjFromFile to embed a font.");
    return (NULL);
  }

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
// 'pdfioFileCreateFontObjFromData()' - Add a font in memory to a PDF file.
//
// This function embeds TrueType/OpenType font data into a PDF file.  The
// "unicode" parameter controls whether the font is encoded for two-byte
// characters (potentially full Unicode, but more typically a subset)
// or to only support the Windows CP1252 (ISO-8859-1 with additional
// characters such as the Euro symbol) subset of Unicode.
//
// @since PDFio v1.6@
//

pdfio_obj_t *				// O - Font object
pdfioFileCreateFontObjFromData(
    pdfio_file_t *pdf,			// I - PDF file
    const void   *data,			// I - Font data in memory
    size_t       datasize,		// I - Size of font in memory
    bool         unicode)		// I - Force Unicode
{
  ttf_t		*font;			// TrueType font
  pdfio_obj_t	*obj,			// Font object
		*file_obj = NULL;	// File object
  pdfio_dict_t	*file;			// Font file dictionary
  pdfio_stream_t *st = NULL;		// Font stream


  // Range check input...
  if (!pdf)
    return (NULL);

  if (!data || !datasize)
  {
    _pdfioFileError(pdf, "No TrueType/OpenType data specified.");
    return (NULL);
  }

  // Create a TrueType font object from the data...
  if ((font = ttfCreateData(data, datasize, 0, (ttf_err_cb_t)ttf_error_cb, pdf)) == NULL)
    return (NULL);

  // Create the font file dictionary and object...
  if ((file = pdfioDictCreate(pdf)) == NULL)
    goto error;

  pdfioDictSetName(file, "Filter", "FlateDecode");

  if ((file_obj = pdfioFileCreateObj(pdf, file)) == NULL)
    goto error;

  if ((st = pdfioObjCreateStream(file_obj, PDFIO_FILTER_FLATE)) == NULL)
    goto error;

  if (!pdfioStreamWrite(st, data, datasize))
    goto error;

  pdfioStreamClose(st);

  // Create the font object...
  if ((obj = create_font(file_obj, font, unicode)) == NULL)
    ttfDelete(font);

  return (obj);

  // If we get here we had an unrecoverable error...
  error:

  if (st)
    pdfioStreamClose(st);
  else
    pdfioObjClose(file_obj);

  ttfDelete(font);

  return (NULL);
}


//
// 'pdfioFileCreateFontObjFromFile()' - Add a font file to a PDF file.
//
// This function embeds a TrueType/OpenType font file into a PDF file.  The
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
  pdfio_dict_t	*file;			// Font file dictionary
  pdfio_obj_t	*obj,			// Font object
		*file_obj = NULL;	// Font file object
  pdfio_stream_t *st = NULL;		// Font stream
  int		fd = -1;		// File
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
    goto error;

  // Create the font file dictionary and object...
  if ((file = pdfioDictCreate(pdf)) == NULL)
    goto error;

  pdfioDictSetName(file, "Filter", "FlateDecode");

  if ((file_obj = pdfioFileCreateObj(pdf, file)) == NULL)
    goto error;

  if ((st = pdfioObjCreateStream(file_obj, PDFIO_FILTER_FLATE)) == NULL)
    goto error;

  while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
  {
    if (!pdfioStreamWrite(st, buffer, (size_t)bytes))
      goto error;
  }

  close(fd);

  pdfioStreamClose(st);

  // Create the font object...
  if ((obj = create_font(file_obj, font, unicode)) == NULL)
    ttfDelete(font);

  return (obj);

  // If we get here we had an unrecoverable error...
  error:

  if (fd >= 0)
    close(fd);

  if (st)
    pdfioStreamClose(st);
  else
    pdfioObjClose(file_obj);

  ttfDelete(font);

  return (NULL);
}


//
// 'pdfioFileCreateICCObjFromData()' - Add ICC profile data to a PDF file.
//

pdfio_obj_t *				// O - Object
pdfioFileCreateICCObjFromData(
    pdfio_file_t        *pdf,		// I - PDF file
    const unsigned char *data,		// I - ICC profile buffer
    size_t              datalen,	// I - Length of ICC profile
    size_t              num_colors)	// I - Number of color components (1, 3, or 4)
{
  pdfio_dict_t	*dict;			// ICC profile dictionary
  pdfio_obj_t	*obj;			// ICC profile object
  pdfio_stream_t *st;			// ICC profile stream


  // Range check input...
  if (!pdf)
    return (NULL);

  if (!data || !datalen)
  {
    _pdfioFileError(pdf, "No ICC profile data specified.");
    return (NULL);
  }

  if (num_colors != 1 && num_colors != 3 && num_colors != 4)
  {
    _pdfioFileError(pdf, "Unsupported number of colors (%lu) for ICC profile.", (unsigned long)num_colors);
    return (NULL);
  }

  // Create the ICC profile object...
  if ((dict = pdfioDictCreate(pdf)) == NULL)
    return (NULL);

  pdfioDictSetNumber(dict, "N", (double)num_colors);
  pdfioDictSetName(dict, "Filter", "FlateDecode");

  if ((obj = pdfioFileCreateObj(pdf, dict)) == NULL)
    return (NULL);

  if ((st = pdfioObjCreateStream(obj, PDFIO_FILTER_FLATE)) == NULL)
    return (NULL);

  if (!pdfioStreamWrite(st, data, datalen))
    obj = NULL;

  pdfioStreamClose(st);

  return (obj);
}


//
// 'pdfioFileCreateICCObjFromFile()' - Add an ICC profile file to a PDF file.
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

  pdfioDictSetNumber(dict, "N", (double)num_colors);
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
  pdfio_dict_t		*dict;		// Image dictionary
  static const char	*defcolors[] =	// Default ColorSpace values
  {
    NULL,
    "DeviceGray",
    NULL,
    "DeviceRGB",
    "DeviceCMYK"
  };


  if (pdf && (pdf->pdfa == _PDFIO_PDFA_1A || pdf->pdfa == _PDFIO_PDFA_1B) && alpha)
  {
    _pdfioFileError(pdf, "Images with transparency (alpha channels) are not allowed in PDF/A-1 files.");
    return (NULL);
  }

  // Range check input...
  if (!pdf || !data || !width || !height || num_colors < 1 || num_colors == 2 || num_colors > 4)
    return (NULL);

  // Create the image dictionary...
  if ((dict = pdfioDictCreate(pdf)) == NULL)
    return (NULL);

  pdfioDictSetName(dict, "Type", "XObject");
  pdfioDictSetName(dict, "Subtype", "Image");
  pdfioDictSetBoolean(dict, "Interpolate", interpolate);
  pdfioDictSetNumber(dict, "Width", (double)width);
  pdfioDictSetNumber(dict, "Height", (double)height);
  pdfioDictSetNumber(dict, "BitsPerComponent", 8);

  if (color_data)
    pdfioDictSetArray(dict, "ColorSpace", color_data);
  else
    pdfioDictSetName(dict, "ColorSpace", defcolors[num_colors]);

  // Create the image object(s)...
  return (create_image(dict, data, width, height, 8, num_colors, alpha));
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


  PDFIO_DEBUG("pdfioFileCreateImageObjFromFile(pdf=%p, filename=\"%s\", interpolate=%s)\n", (void *)pdf, filename, interpolate ? "true" : "false");

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

  PDFIO_DEBUG("pdfioFileCreateImageObjFromFile: buffer=<%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X>\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8], buffer[9], buffer[10], buffer[11], buffer[12], buffer[13], buffer[14], buffer[15], buffer[16], buffer[17], buffer[18], buffer[19], buffer[20], buffer[21], buffer[22], buffer[23], buffer[24], buffer[25], buffer[26], buffer[27], buffer[28], buffer[29], buffer[30], buffer[31]);

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

size_t					// O - Number of bytes per line or `0` on error
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

  if (width < 0)
  {
    _pdfioFileError(obj->pdf, "Invalid image width %d.", width);
    return (0);
  }
  else if (bpc != 1 && bpc != 2 && bpc != 4 && bpc != 8 && bpc != 16)
  {
    _pdfioFileError(obj->pdf, "Invalid image bits per component %d.", bpc);
    return (0);
  }
  else if (colors < 1 || colors > 4)
  {
    _pdfioFileError(obj->pdf, "Invalid image number of colors %d.", colors);
    return (0);
  }
  else
  {
    return ((size_t)((width * colors * bpc + 7) / 8));
  }
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

  // See if this name is already set...
  if (_pdfioDictGetValue(colorspace, name))
    return (false);			// Yes, return false

  // Now set the color space reference and return...
  return (pdfioDictSetArray(colorspace, name, data));
}


//
// 'pdfioPageDictAddFont()' - Add a font object to the page dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioPageDictAddFont(
    pdfio_dict_t *dict,			// I - Page dictionary
    const char   *name,			// I - Font name; must not contain spaces
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
  pdfio_obj_t	*obj = NULL;		// Object
  pdfio_stream_t *st;			// Stream for JPEG data
  ssize_t	bytes;			// Bytes read
  unsigned char	buffer[16384],		// Read buffer
		*bufptr,		// Pointer into buffer
		*bufend;		// End of buffer
  int		marker;			// JFIF marker byte
  size_t	length;			// Length of chunk
  unsigned	width = 0,		// Width in columns
		height = 0,		// Height in lines
		num_colors = 0;		// Number of colors
  unsigned char	*icc_data = NULL;	// ICC profile data, if any
  size_t	icc_datalen = 0;	// Length of ICC profile data
  pdfio_obj_t	*icc_obj;		// ICC profile object


  // Scan the file for APPn and SOFn markers to get the dimensions and color profile...
  bytes = read(fd, buffer, sizeof(buffer));

  for (bufptr = buffer + 2, bufend = buffer + bytes; bufptr < bufend;)
  {
    if ((bufptr + 16) >= bufend)
    {
      // Read more of the file...
      if ((bytes = bufend - bufptr) > 0)
	memmove(buffer, bufptr, (size_t)bytes);

      bufptr = buffer;
      bufend = buffer + bytes;

      if ((bytes = read(fd, bufend, sizeof(buffer) - (size_t)bytes)) <= 0)
      {
	_pdfioFileError(dict->pdf, "Unable to read JPEG data - %s", strerror(errno));
	goto finish;
      }

      bufend += bytes;
    }

    if (*bufptr != _PDFIO_JPEG_MARKER)
    {
      _pdfioFileError(dict->pdf, "Invalid JPEG data: <%02X>", *bufptr);
      goto finish;
    }

    // Start of a marker in the file...
    bufptr ++;

    marker = *bufptr;
    length = (size_t)((bufptr[1] << 8) | bufptr[2]);
    bufptr += 3;

    PDFIO_DEBUG("copy_jpeg: JPEG X'FF%02X' (length %u)\n", marker, (unsigned)length);

    if (marker == _PDFIO_JPEG_EOI || marker == _PDFIO_JPEG_SOS || length < 2)
      break;

    length -= 2;

    if ((marker >= _PDFIO_JPEG_SOF0 && marker <= _PDFIO_JPEG_SOF3) || (marker >= _PDFIO_JPEG_SOF5 && marker <= _PDFIO_JPEG_SOF7) || (marker >= _PDFIO_JPEG_SOF9 && marker <= _PDFIO_JPEG_SOF11) || (marker >= _PDFIO_JPEG_SOF13 && marker <= _PDFIO_JPEG_SOF15))
    {
      // SOFn marker, look for dimensions...
      //
      // Byte(s)  Description
      // -------  -------------------
      // 0        Bits per component
      // 1-2      Height
      // 3-4      Width
      // 5        Number of colors
      if (bufptr[0] != 8)
      {
	_pdfioFileError(dict->pdf, "Unable to load %d-bit JPEG image.", bufptr[0]);
	goto finish;
      }

      width      = (unsigned)((bufptr[3] << 8) | bufptr[4]);
      height     = (unsigned)((bufptr[1] << 8) | bufptr[2]);
      num_colors = bufptr[5];
    }
    else if (marker == _PDFIO_JPEG_APP2 && length > 14 && memcmp(bufptr, "ICC_PROFILE", 12))
    {
      // Portion of ICC profile
      int	n = bufptr[12],		// Chunk number in profile (1-based)
	      count = bufptr[13];	// Number of chunks
      unsigned char *icc_temp;	// New ICC buffer

      // Discard "ICC_PROFILE\0" and chunk number/count...
      bufptr += 14;
      length -= 14;

      // Expand our ICC buffer...
      if ((icc_temp = realloc(icc_data, icc_datalen + length)) == NULL)
      {
        free(icc_data);
	return (NULL);
      }
      else
      {
	icc_data = icc_temp;
      }

      // Read the chunk into the ICC buffer...
      do
      {
	if (bufptr >= bufend)
	{
	  // Read more of the marker...
	  if ((bytes = read(fd, buffer, sizeof(buffer))) <= 0)
	  {
	    _pdfioFileError(dict->pdf, "Unable to read JPEG data - %s", strerror(errno));
	    goto finish;
	  }

	  bufptr = buffer;
	  bufend = buffer + bytes;
	}

	// Copy from the file buffer to the ICC buffer
	if ((bytes = bufend - bufptr) > (ssize_t)length)
	  bytes = (ssize_t)length;

	memcpy(icc_data + icc_datalen, bufptr, bytes);
	icc_datalen += (size_t)bytes;
	bufptr      += bytes;
	length      -= (size_t)bytes;
      }
      while (length > 0);

      if (n == count && width > 0 && height > 0 && num_colors > 0)
      {
	// Have everything we need...
	break;
      }
      else
      {
	// Continue reading...
	continue;
      }
    }

    // Skip past this marker...
    while (length > 0)
    {
      bytes = bufend - bufptr;

      if (length > (size_t)bytes)
      {
	// Consume everything we have and grab more...
	length -= (size_t)bytes;

	if ((bytes = read(fd, buffer, sizeof(buffer))) <= 0)
	{
	  _pdfioFileError(dict->pdf, "Unable to read JPEG data - %s", strerror(errno));
	  goto finish;
	}

	bufptr = buffer;
	bufend = buffer + bytes;
      }
      else
      {
	// Enough at the end of the buffer...
	bufptr += length;
	length = 0;
      }
    }
  }

  if (width == 0 || height == 0 || (num_colors != 1 && num_colors != 3 && num_colors != 4))
  {
    _pdfioFileError(dict->pdf, "Unable to find JPEG dimensions or image data.");
    goto finish;
  }

  // Create the image object...
  pdfioDictSetNumber(dict, "Width", width);
  pdfioDictSetNumber(dict, "Height", height);
  pdfioDictSetNumber(dict, "BitsPerComponent", 8);
  pdfioDictSetName(dict, "Filter", "DCTDecode");
  if (icc_datalen > 0)
  {
    icc_obj = pdfioFileCreateICCObjFromData(dict->pdf, icc_data, icc_datalen, num_colors);
    pdfioDictSetArray(dict, "ColorSpace", pdfioArrayCreateColorFromICCObj(dict->pdf, icc_obj));
  }
  else //if (pdfioDictGetArray(dict, "ColorSpace") == NULL)
  {
    // The default JPEG color space is sRGB or CGATS001 (CMYK)...
    if (num_colors == 4)
      pdfioDictSetArray(dict, "ColorSpace", pdfioArrayCreateColorFromStandard(dict->pdf, num_colors, PDFIO_CS_CGATS001));
    else
      pdfioDictSetArray(dict, "ColorSpace", pdfioArrayCreateColorFromStandard(dict->pdf, num_colors, PDFIO_CS_SRGB));
  }

  obj = pdfioFileCreateObj(dict->pdf, dict);
  st  = pdfioObjCreateStream(obj, PDFIO_FILTER_NONE);

  // Copy the file to a stream...
  lseek(fd, 0, SEEK_SET);

  while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
  {
    if (!pdfioStreamWrite(st, buffer, (size_t)bytes))
    {
      obj = NULL;
      break;
    }
  }

  if (!pdfioStreamClose(st))
    obj = NULL;

  finish:

  free(icc_data);

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
  double	gamma = 2.2,		// Gamma value
		wx = 0.0, wy = 0.0,	// White point chromacity
		rx = 0.0, ry = 0.0,	// Red chromacity
		gx = 0.0, gy = 0.0,	// Green chromacity
		bx = 0.0, by = 0.0;	// Blue chromacity
#ifdef HAVE_LIBPNG
  png_structp	pp = NULL;		// PNG read pointer
  png_infop	info = NULL;		// PNG info pointers
  png_bytep	*rows = NULL;		// PNG row pointers
  unsigned char	*pixels = NULL;		// PNG image data
  unsigned	i,			// Looping var
		color_type,		// PNG color mode
		width,			// Width in columns
		height,			// Height in lines
		depth,			// Bit depth
		num_colors = 0,		// Number of colors
		linesize;		// Bytes per line
  bool		alpha;			// Alpha transparency?
  int		srgb_intent;		// sRGB color?
  png_color	*palette;		// Color palette information
  int		num_palette;		// Number of colors
  int		num_trans;		// Number of transparent colors
  png_color_16	*trans;			// Transparent colors
  png_charp	icc_name;		// ICC profile name
  png_bytep	icc_data;		// ICC profile data
  png_uint_32	icc_datalen;		// Length of ICC profile data


  PDFIO_DEBUG("copy_png(dict=%p, fd=%d)\n", (void *)dict, fd);

  // Allocate memory for PNG reader structures...
  if ((pp = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)dict->pdf, png_error_func, png_error_func)) == NULL)
  {
    _pdfioFileError(dict->pdf, "Unable to allocate memory for PNG file: %s", strerror(errno));
    goto finish_png;
  }

  if ((info = png_create_info_struct(pp)) == NULL)
  {
    _pdfioFileError(dict->pdf, "Unable to allocate memory for PNG file: %s", strerror(errno));
    goto finish_png;
  }

  PDFIO_DEBUG("copy_png: pp=%p, info=%p\n", (void *)pp, (void *)info);

  if (setjmp(png_jmpbuf(pp)))
  {
    // If we get here, PNG loading failed and any errors/warnings were logged
    // via the corresponding callback functions...
    fputs("copy_png: setjmp error handler called.\n", stderr);
    goto finish_png;
  }

  // Set max image size to 16384x16384...
  png_set_user_limits(pp, 16384, 16384);

  // Read from the file descriptor...
  png_set_read_fn(pp, &fd, png_read_func);

  // Don't throw errors with "invalid" sRGB profiles produced by Adobe apps.
#  if defined(PNG_SKIP_sRGB_CHECK_PROFILE) && defined(PNG_SET_OPTION_SUPPORTED)
  png_set_option(pp, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);
#  endif // PNG_SKIP_sRGB_CHECK_PROFILE && PNG_SET_OPTION_SUPPORTED

  // Get the image dimensions and depth...
  png_read_info(pp, info);

  width      = png_get_image_width(pp, info);
  height     = png_get_image_height(pp, info);
  depth      = png_get_bit_depth(pp, info);
  color_type = png_get_color_type(pp, info);

  if ((dict->pdf->pdfa == _PDFIO_PDFA_1A || dict->pdf->pdfa == _PDFIO_PDFA_1B) && (color_type & PNG_COLOR_MASK_ALPHA))
  {
    _pdfioFileError(dict->pdf, "PNG images with transparency (alpha channels) are not allowed in PDF/A-1 files.");
    goto finish_png;
  }

  if (color_type & PNG_COLOR_MASK_PALETTE)
    num_colors = 1;
  else if (color_type & PNG_COLOR_MASK_COLOR)
    num_colors = 3;
  else
    num_colors = 1;

  PDFIO_DEBUG("copy_png: width=%u, height=%u, depth=%u, color_type=%u, num_colors=%d\n", width, height, depth, color_type, num_colors);

  // Set decoding options...
  alpha    = (color_type & PNG_COLOR_MASK_ALPHA) != 0;
  linesize = (width * num_colors * depth + 7) / 8;
  if (alpha)
    linesize += width;

  PDFIO_DEBUG("copy_png: alpha=%s, linesize=%u\n", alpha ? "true" : "false", (unsigned)linesize);

  // Allocate memory for the image...
  if ((pixels = (unsigned char *)calloc(height, linesize)) == NULL)
  {
    _pdfioFileError(dict->pdf, "Unable to allocate memory for PNG image: %s", strerror(errno));
    goto finish_png;
  }

  if ((rows = (png_bytep *)calloc((size_t)height, sizeof(png_bytep))) == NULL)
  {
    _pdfioFileError(dict->pdf, "Unable to allocate memory for PNG image: %s", strerror(errno));
    goto finish_png;
  }

  for (i = 0; i < height; i ++)
    rows[i] = pixels + i * linesize;

  // Read the image...
  for (i = png_set_interlace_handling(pp); i > 0; i --)
    png_read_rows(pp, rows, NULL, (png_uint_32)height);

  // Add image dictionary information...
  pdfioDictSetNumber(dict, "Width", width);
  pdfioDictSetNumber(dict, "Height", height);
  pdfioDictSetNumber(dict, "BitsPerComponent", depth);

  // Grab any color space/palette information...
  if (png_get_PLTE(pp, info, &palette, &num_palette))
  {
    pdfioDictSetArray(dict, "ColorSpace", pdfioArrayCreateColorFromPalette(dict->pdf, num_palette, (unsigned char *)palette));
  }
  else if (png_get_iCCP(pp, info, &icc_name, /*compression_type*/NULL, &icc_data, &icc_datalen))
  {
    PDFIO_DEBUG("copy_png: ICC color profile '%s'\n", icc_name);
    pdfio_obj_t	*icc_obj = pdfioFileCreateICCObjFromData(dict->pdf, icc_data, icc_datalen, num_colors);
    pdfioDictSetArray(dict, "ColorSpace", pdfioArrayCreateColorFromICCObj(dict->pdf, icc_obj));
  }
  else if (png_get_sRGB(pp, info, &srgb_intent))
  {
    PDFIO_DEBUG("copy_png: Explicit sRGB\n");
    pdfioDictSetArray(dict, "ColorSpace", pdfioArrayCreateColorFromStandard(dict->pdf, num_colors, PDFIO_CS_SRGB));
  }
  else if (png_get_cHRM(pp, info, &wx, &wy, &rx, &ry, &gx, &gy, &bx, &by) && png_get_gAMA(pp, info, &gamma))
  {
    PDFIO_DEBUG("copy_png: Color primaries [%.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f], gamma %.6f\n", wx, wy, rx, ry, gx, gy, bx, by, gamma);
    pdfioDictSetArray(dict, "ColorSpace", pdfioArrayCreateColorFromPrimaries(dict->pdf, num_colors, gamma, wx, wy, rx, ry, gx, gy, bx, by));
  }
  else
  {
    // Default to sRGB...
    pdfioDictSetArray(dict, "ColorSpace", pdfioArrayCreateColorFromStandard(dict->pdf, num_colors, PDFIO_CS_SRGB));
  }

  if (png_get_tRNS(pp, info, /*trans_alpha*/NULL, &num_trans, &trans))
  {
    int			m;		// Looping var
    pdfio_array_t	*mask;		// Mask array

    mask = pdfioArrayCreate(dict->pdf);

    if (color_type & PNG_COLOR_MASK_PALETTE)
    {
      // List color indices that are transparent...
      for (m = 0; m < num_trans; m ++)
      {
	pdfioArrayAppendNumber(mask, trans[m].index);
	pdfioArrayAppendNumber(mask, trans[m].index);
      }
    }
    else if (num_colors == 1)
    {
      // List grayscale values that are transparent...
      for (m = 0; m < num_trans; m ++)
      {
	pdfioArrayAppendNumber(mask, trans[m].gray >> (16 - depth));
	pdfioArrayAppendNumber(mask, trans[m].gray >> (16 - depth));
      }
    }
    else
    {
      // List RGB color values that are transparent...
      for (m = 0; m < num_trans; m ++)
      {
	pdfioArrayAppendNumber(mask, trans[m].red >> (16 - depth));
	pdfioArrayAppendNumber(mask, trans[m].green >> (16 - depth));
	pdfioArrayAppendNumber(mask, trans[m].blue >> (16 - depth));
	pdfioArrayAppendNumber(mask, trans[m].red >> (16 - depth));
	pdfioArrayAppendNumber(mask, trans[m].green >> (16 - depth));
	pdfioArrayAppendNumber(mask, trans[m].blue >> (16 - depth));
      }
    }

    pdfioDictSetArray(dict, "Mask", mask);
  }

  // Create the image object...
  obj = create_image(dict, pixels, width, height, depth, num_colors, alpha);

  finish_png:

  if (pp && info)
  {
    png_read_end(pp, info);
    png_destroy_read_struct(&pp, &info, NULL);

    pp   = NULL;
    info = NULL;
  }

  free(pixels);
  pixels = NULL;

  free(rows);
  rows = NULL;

  return (obj);

#else
  pdfio_dict_t	*decode = NULL;		// Parameters for PNG decode
  pdfio_stream_t *st = NULL;		// Stream for PNG data
  ssize_t	bytes;			// Bytes read
  unsigned char	buffer[16384];		// Read buffer
  unsigned	i,			// Looping var
		length,			// Length
		type,			// Chunk code
		crc,			// CRC-32
		temp,			// Temporary value
		width = 0,		// Width
		height = 0,		// Height
		num_colors = 0;		// Number of colors
  unsigned char	bit_depth = 0,		// Bit depth
		color_type = 0,		// Color type
		interlace = 0;		// Interlace type
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
	    PDFIO_DEBUG("copy_png: wx=%.6f, wy=%.6f, rx=%.6f, ry=%.6f, gx=%.6f, gy=%.6f, bx=%.6f, by=%.6f\n", wx, wy, rx, ry, gx, gy, bx, by);
	    PDFIO_DEBUG("copy_png: gamma=%.6f\n", gamma);

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

	  crc    = update_png_crc(crc, buffer, length);
	  width  = (unsigned)((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]);
	  height = (unsigned)((buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7]);

	  if (width == 0 || height == 0)
	  {
	    _pdfioFileError(dict->pdf, "Unsupported PNG dimensions %ux%u.", (unsigned)width, (unsigned)height);
	    return (NULL);
	  }

	  bit_depth = buffer[8];
	  color_type = buffer[9];

	  if (color_type != _PDFIO_PNG_TYPE_GRAY && color_type != _PDFIO_PNG_TYPE_RGB && color_type != _PDFIO_PNG_TYPE_INDEXED)
	  {
	    _pdfioFileError(dict->pdf, "Unsupported PNG color type %u.", color_type);
	    return (NULL);
	  }

	  if ((color_type == _PDFIO_PNG_TYPE_GRAY && bit_depth != 1 && bit_depth != 2 && bit_depth != 4 && bit_depth != 8 && bit_depth != 16) ||
	      (color_type == _PDFIO_PNG_TYPE_RGB && bit_depth != 8 && bit_depth != 16) ||
	      (color_type == _PDFIO_PNG_TYPE_INDEXED && bit_depth != 1 && bit_depth != 2 && bit_depth != 4 && bit_depth != 8))
	  {
	    _pdfioFileError(dict->pdf, "Unsupported PNG bit depth %u for color type %u.", bit_depth, color_type);
	    return (NULL);
	  }

	  if (buffer[10] != _PDFIO_PNG_COMPRESSION_FLATE)
	  {
	    _pdfioFileError(dict->pdf, "Unsupported PNG compression %u.", buffer[10]);
	    return (NULL);
	  }

	  if (buffer[11] != _PDFIO_PNG_FILTER_ADAPTIVE)
	  {
	    _pdfioFileError(dict->pdf, "Unsupported PNG filtering %u.", buffer[11]);
	    return (NULL);
	  }

          interlace = buffer[12];

	  if (interlace != _PDFIO_PNG_INTERLACE_NONE)
	  {
	    _pdfioFileError(dict->pdf, "Unsupported PNG interlacing %u.", interlace);
	    return (NULL);
	  }

          num_colors = color_type == _PDFIO_PNG_TYPE_RGB ? 3 : 1;

	  pdfioDictSetNumber(dict, "Width", width);
	  pdfioDictSetNumber(dict, "Height", height);
	  pdfioDictSetNumber(dict, "BitsPerComponent", bit_depth);
	  pdfioDictSetName(dict, "Filter", "FlateDecode");

	  if ((decode = pdfioDictCreate(dict->pdf)) == NULL)
	    return (NULL);

	  pdfioDictSetNumber(decode, "BitsPerComponent", bit_depth);
	  pdfioDictSetNumber(decode, "Colors", num_colors);
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
#endif // HAVE_LIBPNG
}


//
// 'create_cp1252()' - Create the CP1252 font encoding object.
//

static bool				// O - `true` on success, `false` on failure
create_cp1252(pdfio_file_t *pdf)	// I - PDF file
{
  pdfio_dict_t	*cp1252_dict;		// Encoding dictionary


  if ((cp1252_dict = pdfioDictCreate(pdf)) == NULL)
    return (false);

  pdfioDictSetName(cp1252_dict, "Type", "Encoding");
  pdfioDictSetName(cp1252_dict, "BaseEncoding", "WinAnsiEncoding");

  if ((pdf->cp1252_obj = pdfioFileCreateObj(pdf, cp1252_dict)) == NULL)
    return (false);

  pdfioObjClose(pdf->cp1252_obj);

  return (true);
}


//
// 'create_font()' - Create the object for a TrueType font.
//

static pdfio_obj_t *			// O - Font object
create_font(pdfio_obj_t *file_obj,	// I - Font file object
            ttf_t       *font,		// I - TrueType font
            bool        unicode)	// I - Force Unicode
{
  ttf_rect_t	bounds;			// Font bounds
  pdfio_dict_t	*dict,			// Font dictionary
		*desc;			// Font descriptor
  pdfio_obj_t	*obj = NULL,		// Font object
		*desc_obj;		// Font descriptor object
  const char	*basefont;		// Base font name
  pdfio_array_t	*bbox;			// Font bounding box array
  pdfio_stream_t *st;			// Font stream


  // Create the font descriptor dictionary and object...
  if ((bbox = pdfioArrayCreate(file_obj->pdf)) == NULL)
    goto done;

  ttfGetBounds(font, &bounds);

  pdfioArrayAppendNumber(bbox, bounds.left);
  pdfioArrayAppendNumber(bbox, bounds.bottom);
  pdfioArrayAppendNumber(bbox, bounds.right);
  pdfioArrayAppendNumber(bbox, bounds.top);

  if ((desc = pdfioDictCreate(file_obj->pdf)) == NULL)
    goto done;

  if ((basefont = pdfioStringCreate(file_obj->pdf, ttfGetPostScriptName(font))) == NULL)
    goto done;

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

  if ((desc_obj = pdfioFileCreateObj(file_obj->pdf, desc)) == NULL)
    goto done;

  pdfioObjClose(desc_obj);

  if (unicode)
  {
    // Unicode (CID) font...
    pdfio_dict_t	*cid2gid,	// CIDToGIDMap dictionary
			*to_unicode;	// ToUnicode dictionary
    pdfio_obj_t		*cid2gid_obj,	// CIDToGIDMap object
			*to_unicode_obj;// ToUnicode object
    size_t		i, j,		// Looping vars
			num_cmap;	// Number of CMap entries
    const int		*cmap;		// CMap entries
    int			min_glyph,	// First glyph
			max_glyph;	// Last glyph
    unsigned short	glyphs[65536];	// Glyph to Unicode mapping
    unsigned char	buffer[16384],	// Read buffer
			*bufptr,	// Pointer into buffer
			*bufend;	// End of buffer
    pdfio_dict_t	*type2;		// CIDFontType2 font dictionary
    pdfio_obj_t		*type2_obj;	// CIDFontType2 font object
    pdfio_array_t	*descendants;	// Decendant font list
    pdfio_dict_t	*sidict;	// CIDSystemInfo dictionary
    pdfio_array_t	*w_array,	// Width array
			*temp_array;	// Temporary width sub-array
    int			w0, w1;		// Widths

    // Create a CIDSystemInfo mapping to Adobe UCS2 v0 (Unicode)
    if ((sidict = pdfioDictCreate(file_obj->pdf)) == NULL)
      goto done;

    pdfioDictSetString(sidict, "Registry", "Adobe");
    pdfioDictSetString(sidict, "Ordering", "Identity");
    pdfioDictSetNumber(sidict, "Supplement", 0);

    // Create a CIDToGIDMap object for the Unicode font...
    if ((cid2gid = pdfioDictCreate(file_obj->pdf)) == NULL)
      goto done;

#ifndef DEBUG
    pdfioDictSetName(cid2gid, "Filter", "FlateDecode");
#endif // !DEBUG

    if ((cid2gid_obj = pdfioFileCreateObj(file_obj->pdf, cid2gid)) == NULL)
      goto done;

#ifdef DEBUG
    if ((st = pdfioObjCreateStream(cid2gid_obj, PDFIO_FILTER_NONE)) == NULL)
#else
    if ((st = pdfioObjCreateStream(cid2gid_obj, PDFIO_FILTER_FLATE)) == NULL)
#endif // DEBUG
      goto done;

    cmap      = ttfGetCMap(font, &num_cmap);
    min_glyph = 65536;
    max_glyph = 0;
    memset(glyphs, 0, sizeof(glyphs));

    PDFIO_DEBUG("create_font: num_cmap=%u\n", (unsigned)num_cmap);

    for (i = 0, bufptr = buffer, bufend = buffer + sizeof(buffer); i < num_cmap; i ++)
    {
      PDFIO_DEBUG("create_font: cmap[%u]=%d\n", (unsigned)i, cmap[i]);
      if (cmap[i] < 0 || cmap[i] >= (int)(sizeof(glyphs) / sizeof(glyphs[0])))
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

        glyphs[cmap[i]] = (unsigned short)i;
        if (cmap[i] < min_glyph)
          min_glyph = cmap[i];
        if (cmap[i] > max_glyph)
          max_glyph = cmap[i];
      }

      if (bufptr >= bufend)
      {
        // Flush buffer...
        if (!pdfioStreamWrite(st, buffer, (size_t)(bufptr - buffer)))
        {
	  pdfioStreamClose(st);
	  goto done;
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
	goto done;
      }
    }

    pdfioStreamClose(st);

    // ToUnicode mapping object
    to_unicode = pdfioDictCreate(file_obj->pdf);
    pdfioDictSetName(to_unicode, "Type", "CMap");
    pdfioDictSetName(to_unicode, "CMapName", "Adobe-Identity-UCS2");
    pdfioDictSetDict(to_unicode, "CIDSystemInfo", sidict);

#ifndef DEBUG
    pdfioDictSetName(to_unicode, "Filter", "FlateDecode");
#endif // !DEBUG

    if ((to_unicode_obj = pdfioFileCreateObj(file_obj->pdf, to_unicode)) == NULL)
      goto done;

#ifdef DEBUG
    if ((st = pdfioObjCreateStream(to_unicode_obj, PDFIO_FILTER_NONE)) == NULL)
#else
    if ((st = pdfioObjCreateStream(to_unicode_obj, PDFIO_FILTER_FLATE)) == NULL)
#endif // DEBUG
      goto done;

    pdfioStreamPuts(st,
		    "stream\n"
		    "/CIDInit /ProcSet findresource begin\n"
		    "12 dict begin\n"
		    "begincmap\n"
		    "/CIDSystemInfo<<\n"
		    "/Registry (Adobe)\n"
		    "/Ordering (UCS2)\n"
		    "/Supplement 0\n"
		    ">> def\n"
		    "/CMapName /Adobe-Identity-UCS2 def\n"
		    "/CMapType 2 def\n"
		    "1 begincodespacerange\n"
		    "<0000> <FFFF>\n"
		    "endcodespacerange\n"
                    "endcmap\n"
                    "CMapName currentdict /CMap defineresource pop\n"
                    "end\n"
                    "end\n");

    pdfioStreamClose(st);

    // Create a CIDFontType2 dictionary for the Unicode font...
    if ((type2 = pdfioDictCreate(file_obj->pdf)) == NULL)
      goto done;

    // Width array
    if ((w_array = pdfioArrayCreate(file_obj->pdf)) == NULL)
      goto done;

    for (i = 0, w0 = ttfGetWidth(font, 0), w1 = 0; i < 65536; w0 = w1)
    {
      for (j = 1; (i + j) < 65536; j ++)
      {
        if ((w1 = ttfGetWidth(font, (int)(i + j))) != w0)
          break;
      }

      if (j >= 4)
      {
        // Encode a long sequence of zeros...
	// Encode a repeating sequence...
	pdfioArrayAppendNumber(w_array, (double)i);
	pdfioArrayAppendNumber(w_array, (double)(i + j - 1));
	pdfioArrayAppendNumber(w_array, w0);

	i += j;
      }
      else
      {
        // Encode a non-repeating sequence...
        pdfioArrayAppendNumber(w_array, (double)i);

        if ((temp_array = pdfioArrayCreate(file_obj->pdf)) == NULL)
	  goto done;

        pdfioArrayAppendNumber(temp_array, w0);
        for (i ++; i < 65536 && pdfioArrayGetSize(temp_array) < 8191; i ++, w0 = w1)
        {
          if ((w1 = ttfGetWidth(font, (int)i)) == w0 && i < 65530)
          {
            for (j = 1; j < 4; j ++)
            {
              if (ttfGetWidth(font, (int)(i + j)) != w0)
                break;
            }

            if (j >= 4)
	      break;
	  }

	  pdfioArrayAppendNumber(temp_array, w1);
        }

        pdfioArrayAppendArray(w_array, temp_array);
      }
    }

    // Then the dictionary for the CID base font...
    pdfioDictSetName(type2, "Type", "Font");
    pdfioDictSetName(type2, "Subtype", "CIDFontType2");
    pdfioDictSetName(type2, "BaseFont", basefont);
    pdfioDictSetDict(type2, "CIDSystemInfo", sidict);
    pdfioDictSetObj(type2, "CIDToGIDMap", cid2gid_obj);
    pdfioDictSetObj(type2, "FontDescriptor", desc_obj);
    pdfioDictSetArray(type2, "W", w_array);

    if ((type2_obj = pdfioFileCreateObj(file_obj->pdf, type2)) == NULL)
      goto done;

    pdfioObjClose(type2_obj);

    // Create a Type 0 font object...
    if ((descendants = pdfioArrayCreate(file_obj->pdf)) == NULL)
      goto done;

    pdfioArrayAppendObj(descendants, type2_obj);

    if ((dict = pdfioDictCreate(file_obj->pdf)) == NULL)
      goto done;

    pdfioDictSetName(dict, "Type", "Font");
    pdfioDictSetName(dict, "Subtype", "Type0");
    pdfioDictSetName(dict, "BaseFont", basefont);
    pdfioDictSetArray(dict, "DescendantFonts", descendants);
    pdfioDictSetName(dict, "Encoding", "Identity-H");
    pdfioDictSetObj(dict, "ToUnicode", to_unicode_obj);

    if ((obj = pdfioFileCreateObj(file_obj->pdf, dict)) != NULL)
      pdfioObjClose(obj);
  }
  else
  {
    // Simple (CP1282 or custom encoding) 8-bit font...
    int			ch;		// Character
    pdfio_array_t	*w_array;	// Widths array

    if (ttfGetMaxChar(font) >= 255 && !file_obj->pdf->cp1252_obj && !create_cp1252(file_obj->pdf))
      goto done;

    // Create a TrueType font object...
    if ((dict = pdfioDictCreate(file_obj->pdf)) == NULL)
      goto done;

    pdfioDictSetName(dict, "Type", "Font");
    pdfioDictSetName(dict, "Subtype", "TrueType");
    pdfioDictSetName(dict, "BaseFont", basefont);
    pdfioDictSetNumber(dict, "FirstChar", 32);
    if (ttfGetMaxChar(font) >= 255)
    {
      pdfioDictSetObj(dict, "Encoding", file_obj->pdf->cp1252_obj);
      pdfioDictSetNumber(dict, "LastChar", 255);
    }
    else
    {
      pdfioDictSetNumber(dict, "LastChar", ttfGetMaxChar(font));
    }

    // Build a Widths array for CP1252/WinAnsiEncoding
    if ((w_array = pdfioArrayCreate(file_obj->pdf)) == NULL)
      goto done;

    for (ch = 32; ch < 256 && ch < ttfGetMaxChar(font); ch ++)
    {
      if (ch >= 0x80 && ch < 0xa0)
	pdfioArrayAppendNumber(w_array, ttfGetWidth(font, _pdfio_cp1252[ch - 0x80]));
      else
	pdfioArrayAppendNumber(w_array, ttfGetWidth(font, ch));
    }

    pdfioDictSetArray(dict, "Widths", w_array);

    pdfioDictSetObj(dict, "FontDescriptor", desc_obj);

    if ((obj = pdfioFileCreateObj(file_obj->pdf, dict)) != NULL)
      pdfioObjClose(obj);
  }

  done:

  if (obj)
    _pdfioObjSetExtension(obj, font, (_pdfio_extfree_t)ttfDelete);

  return (obj);
}


//
// 'create_image()' - Create an image object from some data.
//

static pdfio_obj_t *			// O - PDF object or `NULL` on error
create_image(
    pdfio_dict_t        *dict,		// I - Image dictionary
    const unsigned char *data,		// I - Image data
    size_t              width,		// I - Width in columns
    size_t              height,		// I - Height in lines
    size_t              depth,		// I - Bit depth
    size_t              num_colors,	// I - Number of colors
    bool                alpha)		// I - `true` if there is transparency
{
  pdfio_file_t		*pdf = dict->pdf;
					// PDF file
  pdfio_dict_t		*mask_dict,	// Mask image dictionary
			*decode;	// DecodeParms dictionary
  pdfio_obj_t		*obj,		// Image object
			*mask_obj = NULL;
					// Mask image object, if any
  pdfio_stream_t	*st;		// Image stream
  size_t		x, y,		// X and Y position in image
			bpc = depth / 8,// Bytes per component
			bpp = num_colors * bpc,
					// Bytes per pixel (less alpha)
			linelen;	// Line length
  const unsigned char	*dataptr;	// Pointer into image data
  unsigned char		*line = NULL,	// Current line
			*lineptr;	// Pointer into line


  // Allocate memory for one line of data...
  linelen = (width * num_colors * depth + 7) / 8;

  if ((line = malloc(linelen)) == NULL)
    return (NULL);

  // Use Flate compression...
  pdfioDictSetName(dict, "Filter", "FlateDecode");

  // Generate a mask image, as needed...
  if (alpha)
  {
    // Create the image mask dictionary...
    if ((mask_dict = pdfioDictCopy(pdf, dict)) == NULL)
    {
      free(line);
      return (NULL);
    }

    // Transparency masks are always grayscale...
    pdfioDictSetName(mask_dict, "ColorSpace", "DeviceGray");

    // Set the automatic PNG predictor to optimize compression...
    if ((decode = pdfioDictCreate(pdf)) == NULL)
    {
      free(line);
      return (NULL);
    }

    pdfioDictSetNumber(decode, "BitsPerComponent", (double)depth);
    pdfioDictSetNumber(decode, "Colors", 1);
    pdfioDictSetNumber(decode, "Columns", (double)width);
    pdfioDictSetNumber(decode, "Predictor", _PDFIO_PREDICTOR_PNG_AUTO);
    pdfioDictSetDict(mask_dict, "DecodeParms", decode);

    // Create the mask object and write the mask image...
    if ((mask_obj = pdfioFileCreateObj(pdf, mask_dict)) == NULL)
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

    for (y = height, dataptr = data + bpp; y > 0; y --)
    {
      if (bpc == 1)
      {
	for (x = width, lineptr = line; x > 0; x --, dataptr += bpp)
	  *lineptr++ = *dataptr++;
      }
      else
      {
	for (x = width, lineptr = line; x > 0; x --, dataptr += bpp)
	{
	  *lineptr++ = *dataptr++;
	  *lineptr++ = *dataptr++;
	}
      }

      pdfioStreamWrite(st, line, width * bpc);
    }

    pdfioStreamClose(st);

    // Use the transparency mask...
    pdfioDictSetObj(dict, "SMask", mask_obj);
  }

  // Set the automatic PNG predictor to optimize compression...
  if ((decode = pdfioDictCreate(pdf)) == NULL)
  {
    free(line);
    return (NULL);
  }

  pdfioDictSetNumber(decode, "BitsPerComponent", (double)depth);
  pdfioDictSetNumber(decode, "Colors", (double)num_colors);
  pdfioDictSetNumber(decode, "Columns", (double)width);
  pdfioDictSetNumber(decode, "Predictor", _PDFIO_PREDICTOR_PNG_AUTO);
  pdfioDictSetDict(dict, "DecodeParms", decode);

  // Now create the image...
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
      if (bpp == 1)
      {
	for (x = width, lineptr = line; x > 0; x --, dataptr += bpc)
	  *lineptr++ = *dataptr++;
      }
      else
      {
	for (x = width, lineptr = line; x > 0; x --, dataptr += bpp + bpc, lineptr += bpp)
	  memcpy(lineptr, dataptr, bpp);
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


#ifdef HAVE_LIBPNG
//
// 'png_error_func()' - PNG error message function.
//

static void
png_error_func(
    png_structp     pp,			// I - PNG pointer
    png_const_charp message)		// I - Error message
{
  pdfio_file_t	*pdf = (pdfio_file_t *)png_get_error_ptr(pp);
					// PDF file


  _pdfioFileError(pdf, "Unable to create image object from PNG file: %s", message);
}


//
// 'png_read_func()' - Read from a PNG file.
//

static void
png_read_func(png_structp pp,		// I - PNG pointer
              png_bytep   data,		// I - Read buffer
              size_t      length)	// I - Number of bytes to read
{
  int		*fd = (int *)png_get_io_ptr(pp);
					// Pointer to file descriptor
  ssize_t	bytes;			// Bytes read


  PDFIO_DEBUG("png_read_func(pp=%p, data=%p, length=%lu)\n", (void *)pp, (void *)data, (unsigned long)length);

  if ((bytes = read(*fd, data, length)) < (ssize_t)length)
    png_error(pp, "Unable to read from PNG file.");
}
#endif // HAVE_LIBPNG


//
// 'ttf_error_cb()' - Relay a message from the TTF functions.
//

static void
ttf_error_cb(pdfio_file_t *pdf,		// I - PDF file
             const char   *message)	// I - Error message
{
  (pdf->error_cb)(pdf, message, pdf->error_data);
}


#ifndef HAVE_LIBPNG
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
#endif // !HAVE_LIBPNG


//
// 'write_array()' - Write an array value.
//

static bool				// O - `true` on success, `false` on error
write_array(pdfio_stream_t *st,		// I - Stream
            pdfio_array_t  *a)		// I - Array
{
  size_t	i;			// Looping var
  _pdfio_value_t *v;			// Current value


  // Arrays are surrounded by square brackets ([ ... ])
  if (!pdfioStreamPuts(st, "["))
    return (false);

  // Write each value...
  for (i = a->num_values, v = a->values; i > 0; i --, v ++)
  {
    if (!write_value(st, v))
      return (false);
  }

  // Closing bracket...
  return (pdfioStreamPuts(st, "]"));
}


//
// 'write_dict()' - Write a dictionary value.
//

static bool				// O - `true` on success, `false` on error
write_dict(pdfio_stream_t *st,		// I - Stream
           pdfio_dict_t   *dict)	// I - Dictionary
{
  size_t	i;			// Looping var
  _pdfio_pair_t	*pair;			// Current key/value pair


  // Dictionaries are bounded by "<<" and ">>"...
  if (!pdfioStreamPuts(st, "<<"))
    return (false);

  // Write all of the key/value pairs...
  for (i = dict->num_pairs, pair = dict->pairs; i > 0; i --, pair ++)
  {
    if (!pdfioStreamPrintf(st, "%N", pair->key))
      return (false);

    if (!write_value(st, &pair->value))
      return (false);
  }

  // Close it up...
  return (pdfioStreamPuts(st, ">>"));
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
      // Write UTF-16 in hex...
      if (ch < 0x100000)
      {
        // Two-byte UTF-16
	if (!pdfioStreamPrintf(st, "%04X", ch))
	  return (false);
      }
      else
      {
        // Four-byte UTF-16
	if (!pdfioStreamPrintf(st, "%04X%04X", 0xd800 | ((ch >> 10) & 0x03ff), 0xdc00 | (ch & 0x03ff)))
	  return (false);
      }
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


//
// 'write_value()' - Write a PDF value.
//

static bool				// O - `true` on success, `false` on error
write_value(pdfio_stream_t *st,		// I - Stream
            _pdfio_value_t *v)		// I - Value
{
  switch (v->type)
  {
    default :
        return (false);

    case PDFIO_VALTYPE_ARRAY :
        return (write_array(st, v->value.array));

    case PDFIO_VALTYPE_BINARY :
        {
          size_t	databytes;	// Bytes to write
          uint8_t	*dataptr;	// Pointer into data

          if (!pdfioStreamPuts(st, "<"))
	    return (false);

          for (dataptr = v->value.binary.data, databytes = v->value.binary.datalen; databytes > 1; databytes -= 2, dataptr += 2)
          {
            if (!pdfioStreamPrintf(st, "%02X%02X", dataptr[0], dataptr[1]))
              return (false);
          }

          if (databytes > 0 && !pdfioStreamPrintf(st, "%02X", dataptr[0]))
            return (false);

	  return (pdfioStreamPuts(st, ">"));
        }

    case PDFIO_VALTYPE_BOOLEAN :
        if (v->value.boolean)
          return (pdfioStreamPuts(st, " true"));
        else
          return (pdfioStreamPuts(st, " false"));

    case PDFIO_VALTYPE_DATE :
        {
          struct tm	date;		// Date values
          char		datestr[32];	// Formatted date value

#ifdef _WIN32
          gmtime_s(&date, &v->value.date);
#else
	  gmtime_r(&v->value.date, &date);
#endif // _WIN32

	  snprintf(datestr, sizeof(datestr), "D:%04d%02d%02d%02d%02d%02dZ", date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);

	  return (pdfioStreamPrintf(st, "%S", datestr));
        }

    case PDFIO_VALTYPE_DICT :
        return (write_dict(st, v->value.dict));

    case PDFIO_VALTYPE_INDIRECT :
        return (pdfioStreamPrintf(st, " %lu %u R", (unsigned long)v->value.indirect.number, v->value.indirect.generation));

    case PDFIO_VALTYPE_NAME :
        return (pdfioStreamPrintf(st, "%N", v->value.name));

    case PDFIO_VALTYPE_NULL :
        return (pdfioStreamPuts(st, " null"));

    case PDFIO_VALTYPE_NUMBER :
        return (pdfioStreamPrintf(st, " %.6f", v->value.number));

    case PDFIO_VALTYPE_STRING :
	return (pdfioStreamPrintf(st, "%S", v->value.string));
  }
}
