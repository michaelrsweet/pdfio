//
// Test program for PDFio.
//
// Copyright © 2021-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Usage:
//
//   ./testpdfio OPTIONS [FILENAME {OBJECT-NUMBER,OUT-FILENAME}] ...
//
// Options:
//
//   --help                   Show help
//   --password PASSWORD      Set access password
//   --verbose                Be verbose
//

#include "pdfio-private.h"
#include "pdfio-content.h"
#include "test.h"
#include <math.h>
#include <locale.h>
#ifndef M_PI
#  define M_PI	3.14159265358979323846264338327950288
#endif // M_PI


//
// Local functions...
//

static int	do_crypto_tests(void);
static int	do_pdfa_tests(void);
static int	do_test_file(const char *filename, const char *outfile, int objnum, const char *password, bool verbose);
static int	do_unit_tests(void);
static int	draw_image(pdfio_stream_t *st, const char *name, double x, double y, double w, double h, const char *label);
static bool	error_cb(pdfio_file_t *pdf, const char *message, bool *error);
static bool	iterate_cb(pdfio_dict_t *dict, const char *key, void *cb_data);
static ssize_t	output_cb(int *fd, const void *buffer, size_t bytes);
static const char *password_cb(void *data, const char *filename);
static int	read_unit_file(const char *filename, size_t num_pages, size_t first_image, bool is_output);
static ssize_t	token_consume_cb(const char **s, size_t bytes);
static ssize_t	token_peek_cb(const char **s, char *buffer, size_t bytes);
static int	usage(FILE *fp);
static int	verify_image(pdfio_file_t *pdf, size_t number);
static int	write_alpha_test(pdfio_file_t *pdf, int number, pdfio_obj_t *font);
static int	write_color_patch(pdfio_stream_t *st, bool device);
static int	write_color_test(pdfio_file_t *pdf, int number, pdfio_obj_t *font);
static int	write_font_test(pdfio_file_t *pdf, int number, pdfio_obj_t *font, const char *textfontfile, bool unicode);
static int	write_header_footer(pdfio_stream_t *st, const char *title, int number);
static pdfio_obj_t *write_image_object(pdfio_file_t *pdf, _pdfio_predictor_t predictor);
static int	write_images_test(pdfio_file_t *pdf, int number, pdfio_obj_t *font);
static int	write_jpeg_test(pdfio_file_t *pdf, const char *title, int number, pdfio_obj_t *font, pdfio_obj_t *image);
static int	write_pdfa_file(const char *filename, const char *pdfa_version);
static int	write_png_tests(pdfio_file_t *pdf, int number, pdfio_obj_t *font);
static int	write_text_test(pdfio_file_t *pdf, int first_page, pdfio_obj_t *font, const char *filename);
static int	write_unit_file(pdfio_file_t *inpdf, const char *outname, pdfio_file_t *outpdf, size_t *num_pages, size_t *first_image);


//
// 'main()' - Main entry for test program.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		ret = 0;		// Return value


  if (argc > 1)
  {
    int		i;			// Looping var
    const char	*password = NULL;	// Password
    bool	verbose = false;	// Be verbose?

    for (i = 1; i < argc; i ++)
    {
      if (!strcmp(argv[i], "--help"))
      {
        return (usage(stdout));
      }
      else if (!strcmp(argv[i], "--password"))
      {
        i ++;
        if (i < argc)
        {
          password = argv[i];
        }
        else
        {
          fputs("testpdfio: Missing password after '--password'.\n", stderr);
          return (usage(stderr));
        }
      }
      else if (!strcmp(argv[i], "--verbose"))
      {
        verbose = true;
      }
      else if (argv[i][0] == '-')
      {
        fprintf(stderr, "testpdfio: Unknown option '%s'.\n", argv[i]);
        return (usage(stderr));
      }
      else if ((i + 1) < argc && isdigit(argv[i + 1][0] & 255))
      {
        // filename.pdf object-number
        if (do_test_file(argv[i], /*outfile*/NULL, atoi(argv[i + 1]), password, verbose))
	  ret = 1;

	i ++;
      }
      else
      {
        if (do_test_file(argv[i], argv[i + 1], /*objnum*/0, password, verbose))
          ret = 1;

        if (argv[i + 1])
          i ++;
      }
    }
  }
  else
  {
    fprintf(stderr, "testpdfio: Test locale is \"%s\".\n", setlocale(LC_ALL, getenv("LANG")));

#if _WIN32
    // Windows puts executables in Platform/Configuration subdirs...
    if (!_access("../../testfiles", 0))
      _chdir("../..");
#endif // _WIN32

    ret = do_unit_tests();
  }

  return (ret);
}


//
// 'do_crypto_tests()' - Test the various cryptographic functions in PDFio.
//

static int				// O - Exit status
do_crypto_tests(void)
{
  int		ret = 0;		// Return value
  size_t	i;			// Looping var
  _pdfio_aes_t	aes;			// AES context
  _pdfio_md5_t	md5;			// MD5 context
  _pdfio_rc4_t	rc4;			// RC4 context
  _pdfio_sha256_t sha256;		// SHA256 context
  uint8_t	key[32],		// Encryption/decryption key
	        iv[32],			// Initialization vector
	        buffer[256],		// Output buffer
	        buffer2[256];		// Second output buffer
  const char	*prefix, *suffix;	// Prefix/suffix strings
  static const char *text = "Hello, World! Now is the time for all good men to come to the aid of their country.\n";
					// Test text
  static uint8_t aes128key[] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c };
  static uint8_t aes128rounds[] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c, 0xa0, 0xfa, 0xfe, 0x17, 0x88, 0x54, 0x2c, 0xb1, 0x23, 0xa3, 0x39, 0x39, 0x2a, 0x6c, 0x76, 0x05, 0xf2, 0xc2, 0x95, 0xf2, 0x7a, 0x96, 0xb9, 0x43, 0x59, 0x35, 0x80, 0x7a, 0x73, 0x59, 0xf6, 0x7f, 0x3d, 0x80, 0x47, 0x7d, 0x47, 0x16, 0xfe, 0x3e, 0x1e, 0x23, 0x7e, 0x44, 0x6d, 0x7a, 0x88, 0x3b, 0xef, 0x44, 0xa5, 0x41, 0xa8, 0x52, 0x5b, 0x7f, 0xb6, 0x71, 0x25, 0x3b, 0xdb, 0x0b, 0xad, 0x00, 0xd4, 0xd1, 0xc6, 0xf8, 0x7c, 0x83, 0x9d, 0x87, 0xca, 0xf2, 0xb8, 0xbc, 0x11, 0xf9, 0x15, 0xbc, 0x6d, 0x88, 0xa3, 0x7a, 0x11, 0x0b, 0x3e, 0xfd, 0xdb, 0xf9, 0x86, 0x41, 0xca, 0x00, 0x93, 0xfd, 0x4e, 0x54, 0xf7, 0x0e, 0x5f, 0x5f, 0xc9, 0xf3, 0x84, 0xa6, 0x4f, 0xb2, 0x4e, 0xa6, 0xdc, 0x4f, 0xea, 0xd2, 0x73, 0x21, 0xb5, 0x8d, 0xba, 0xd2, 0x31, 0x2b, 0xf5, 0x60, 0x7f, 0x8d, 0x29, 0x2f, 0xac, 0x77, 0x66, 0xf3, 0x19, 0xfa, 0xdc, 0x21, 0x28, 0xd1, 0x29, 0x41, 0x57, 0x5c, 0x00, 0x6e, 0xd0, 0x14, 0xf9, 0xa8, 0xc9, 0xee, 0x25, 0x89, 0xe1, 0x3f, 0x0c, 0xc8, 0xb6, 0x63, 0x0c, 0xa6 };
					// FIPS-197 example key expansion
  static uint8_t aes128text[] =  { 0xfb, 0x77, 0xac, 0xce, 0x3c, 0x95, 0x40, 0xcf, 0xca, 0xc8, 0x26, 0xbf, 0xc0, 0x69, 0x73, 0x3c, 0x01, 0xfd, 0x72, 0x01, 0xeb, 0x4d, 0x6f, 0xf7, 0xb4, 0x72, 0x6d, 0x84, 0x69, 0x9f, 0x89, 0xab, 0xe6, 0x2b, 0x9a, 0x9a, 0x6e, 0xc1, 0x61, 0xd7, 0x9d, 0x83, 0x2d, 0x58, 0x55, 0xa7, 0x58, 0x50, 0x00, 0xad, 0x19, 0x7b, 0xee, 0x6a, 0x36, 0x6f, 0xd1, 0xa7, 0xa4, 0x6b, 0xc5, 0x78, 0x9a, 0x18, 0x05, 0xf0, 0x2c, 0xd4, 0x60, 0x25, 0xe0, 0xa7, 0xb1, 0x36, 0xdb, 0x18, 0xd3, 0xf7, 0x59, 0x29, 0x22, 0xec, 0x25, 0x77, 0x0d, 0x9e, 0x5a, 0x01, 0xcc, 0xf6, 0x29, 0xc2, 0x08, 0xc2, 0xfc, 0x4f };
					// Expected AES-128 CBC result
  static uint8_t aes256text[] =  { 0x2b, 0x94, 0x45, 0x9e, 0xed, 0xa0, 0x89, 0x7b, 0x35, 0x4e, 0xde, 0x06, 0x00, 0x4d, 0xda, 0x6b, 0x61, 0x2f, 0xb9, 0x06, 0xd5, 0x0f, 0x22, 0xed, 0xd2, 0xe3, 0x6b, 0x39, 0x5a, 0xa1, 0xe3, 0x7d, 0xa1, 0xcc, 0xd4, 0x0b, 0x6b, 0xa4, 0xff, 0xe9, 0x9c, 0x89, 0x0c, 0xc7, 0x95, 0x47, 0x19, 0x9b, 0x06, 0xdc, 0xc8, 0x7c, 0x5c, 0x5d, 0x56, 0x99, 0x1e, 0x90, 0x7d, 0x99, 0xc5, 0x7b, 0xc4, 0xe4, 0xfb, 0x02, 0x15, 0x50, 0x23, 0x2a, 0xe4, 0xc1, 0x20, 0xfd, 0xf4, 0x03, 0xfe, 0x6f, 0x15, 0x48, 0xd8, 0x62, 0x36, 0x98, 0x2a, 0x62, 0xf5, 0x2c, 0xa6, 0xfa, 0x7a, 0x43, 0x53, 0xcd, 0xad, 0x18 };
					// Expected AES-256 CBC result
  static uint8_t md5text[16] = { 0x74, 0x0c, 0x2c, 0xea, 0xe1, 0xab, 0x06, 0x7c, 0xdb, 0x1d, 0x49, 0x1d, 0x2d, 0x66, 0xf2, 0x93 };
					// Expected MD5 hash result
  static uint8_t rc4text[] = { 0xd2, 0xa2, 0xa0, 0xf6, 0x0f, 0xb1, 0x3e, 0xa0, 0xdd, 0xe1, 0x44, 0xfd, 0xec, 0xc4, 0x55, 0xf8, 0x25, 0x68, 0xad, 0xe6, 0xb0, 0x60, 0x7a, 0x0f, 0x4e, 0xfe, 0xed, 0x9c, 0x78, 0x3a, 0xf8, 0x73, 0x79, 0xbd, 0x82, 0x88, 0x39, 0x01, 0xc7, 0xd0, 0x34, 0xfe, 0x40, 0x16, 0x93, 0x5a, 0xec, 0x81, 0xda, 0x34, 0xdf, 0x5b, 0xd1, 0x47, 0x2c, 0xfa, 0xe0, 0x13, 0xc5, 0xe2, 0xb0, 0x57, 0x5c, 0x17, 0x62, 0xaa, 0x83, 0x1c, 0x4f, 0xa0, 0x0a, 0xed, 0x6c, 0x42, 0x41, 0x8a, 0x45, 0x03, 0xb8, 0x72, 0xa8, 0x99, 0xd7, 0x06 };
					// Expected RC4 result
  static uint8_t sha256text[32] = { 0x19, 0x71, 0x9b, 0xf0, 0xc6, 0xd8, 0x34, 0xc9, 0x6e, 0x8a, 0x56, 0xcc, 0x34, 0x45, 0xb7, 0x1d, 0x5b, 0x74, 0x9c, 0x52, 0x40, 0xcd, 0x30, 0xa2, 0xc2, 0x84, 0x53, 0x83, 0x16, 0xf8, 0x1a, 0xbb };
					// Expected SHA-256 hash result


  testBegin("_pdfioAESInit(128-bit sample key)");
  _pdfioCryptoAESInit(&aes, aes128key, sizeof(aes128key), NULL);
  if (!memcmp(aes128rounds, aes.round_key, sizeof(aes128rounds)))
  {
    testEnd(true);
  }
  else
  {
    for (i = 0; i < (sizeof(aes128rounds) - 4); i ++)
    {
      if (aes.round_key[i] != aes128rounds[i])
        break;
    }

    prefix = i > 0 ? "..." : "";
    suffix = i < (sizeof(aes128rounds) - 4) ? "..." : "";

    testEndMessage(false, "got '%s%02X%02X%02X%02X%s', expected '%s%02X%02X%02X%02X%s'", prefix, aes.round_key[i], aes.round_key[i + 1], aes.round_key[i + 2], aes.round_key[i + 3], suffix, prefix, aes128rounds[i], aes128rounds[i + 1], aes128rounds[i + 2], aes128rounds[i + 3], suffix);
    ret = 1;
  }

  testBegin("_pdfioAESInit/Encrypt(128-bit CBC)");
  for (i = 0; i < 16; i ++)
  {
    key[i] = (uint8_t)i + 1;
    iv[i]  = (uint8_t)(0xff - i);
  }

  _pdfioCryptoAESInit(&aes, key, 16, iv);
  _pdfioCryptoAESEncrypt(&aes, buffer, (uint8_t *)text, strlen(text));

  if (!memcmp(aes128text, buffer, sizeof(aes128text)))
  {
    testEnd(true);
  }
  else
  {
    for (i = 0; i < (sizeof(aes128text) - 4); i ++)
    {
      if (buffer[i] != aes128text[i])
        break;
    }

    prefix = i > 0 ? "..." : "";
    suffix = i < (sizeof(aes128text) - 4) ? "..." : "";

    testEndMessage(false, "got '%s%02X%02X%02X%02X%s', expected '%s%02X%02X%02X%02X%s'", prefix, buffer[i], buffer[i + 1], buffer[i + 2], buffer[i + 3], suffix, prefix, aes128text[i], aes128text[i + 1], aes128text[i + 2], aes128text[i + 3], suffix);
    ret = 1;
  }

  testBegin("_pdfioAESInit/Decrypt(128-bit CBC)");
  _pdfioCryptoAESInit(&aes, key, 16, iv);
  _pdfioCryptoAESDecrypt(&aes, buffer2, buffer, sizeof(aes128text));

  if (!memcmp(buffer2, text, strlen(text)))
  {
    testEnd(true);
  }
  else
  {
    for (i = 0; text[i + 4]; i ++)
    {
      if (buffer2[i] != text[i])
        break;
    }

    prefix = i > 0 ? "..." : "";
    suffix = text[i + 4] ? "..." : "";

    testEndMessage(false, "got '%s%02X%02X%02X%02X%s', expected '%s%02X%02X%02X%02X%s'", prefix, buffer2[i], buffer2[i + 1], buffer2[i + 2], buffer2[i + 3], suffix, prefix, text[i], text[i + 1], text[i + 2], text[i + 3], suffix);
    ret = 1;
  }

  testBegin("_pdfioAESInit/Encrypt(256-bit CBC)");
  for (i = 0; i < 32; i ++)
  {
    key[i] = (uint8_t)i + 1;
    iv[i]  = (uint8_t)(0xff - i);
  }

  _pdfioCryptoAESInit(&aes, key, 32, iv);
  _pdfioCryptoAESEncrypt(&aes, buffer, (uint8_t *)text, strlen(text));

  if (!memcmp(aes256text, buffer, sizeof(aes256text)))
  {
    testEnd(true);
  }
  else
  {
    for (i = 0; i < (sizeof(aes256text) - 4); i ++)
    {
      if (buffer[i] != aes256text[i])
        break;
    }

    prefix = i > 0 ? "..." : "";
    suffix = i < (sizeof(aes256text) - 4) ? "..." : "";

    testEndMessage(false, "got '%s%02X%02X%02X%02X%s', expected '%s%02X%02X%02X%02X%s'", prefix, buffer[i], buffer[i + 1], buffer[i + 2], buffer[i + 3], suffix, prefix, aes256text[i], aes256text[i + 1], aes256text[i + 2], aes256text[i + 3], suffix);
    ret = 1;
  }

  testBegin("_pdfioAESInit/Decrypt(256-bit CBC)");
  _pdfioCryptoAESInit(&aes, key, 32, iv);
  _pdfioCryptoAESDecrypt(&aes, buffer2, buffer, sizeof(aes256text));

  if (!memcmp(buffer2, text, strlen(text)))
  {
    testEnd(true);
  }
  else
  {
    for (i = 0; text[i + 4]; i ++)
    {
      if (buffer2[i] != text[i])
        break;
    }

    prefix = i > 0 ? "..." : "";
    suffix = text[i + 4] ? "..." : "";

    testEndMessage(false, "got '%s%02X%02X%02X%02X%s', expected '%s%02X%02X%02X%02X%s'", prefix, buffer2[i], buffer2[i + 1], buffer2[i + 2], buffer2[i + 3], suffix, prefix, text[i], text[i + 1], text[i + 2], text[i + 3], suffix);
    ret = 1;
  }

  testBegin("_pdfioMD5Init/Append/Finish");
  _pdfioCryptoMD5Init(&md5);
  _pdfioCryptoMD5Append(&md5, (uint8_t *)text, strlen(text));
  _pdfioCryptoMD5Finish(&md5, buffer);

  if (!memcmp(md5text, buffer, sizeof(md5text)))
  {
    testEnd(true);
  }
  else
  {
    testEndMessage(false, "got '%02X%02X%02X%02X...%02X%02X%02X%02X', expected '%02X%02X%02X%02X...%02X%02X%02X%02X'", buffer[0], buffer[1], buffer[2], buffer[3], buffer[12], buffer[13], buffer[14], buffer[15], md5text[0], md5text[1], md5text[2], md5text[3], md5text[12], md5text[13], md5text[14], md5text[15]);
    ret = 1;
  }

  testBegin("_pdfioRC4Init/Encrypt(128-bit)");
  for (i = 0; i < 16; i ++)
    key[i] = (uint8_t)i + 1;

  _pdfioCryptoRC4Init(&rc4, key, 16);
  _pdfioCryptoRC4Crypt(&rc4, buffer, (uint8_t *)text, strlen(text));

  if (!memcmp(rc4text, buffer, sizeof(rc4text)))
  {
    testEnd(true);
  }
  else
  {
    for (i = 0; i < (sizeof(rc4text) - 4); i ++)
    {
      if (buffer[i] != rc4text[i])
        break;
    }

    prefix = i > 0 ? "..." : "";
    suffix = i < (sizeof(rc4text) - 4) ? "..." : "";

    testEndMessage(false, "got '%s%02X%02X%02X%02X%s', expected '%s%02X%02X%02X%02X%s'", prefix, buffer[i], buffer[i + 1], buffer[i + 2], buffer[i + 3], suffix, prefix, rc4text[i], rc4text[i + 1], rc4text[i + 2], rc4text[i + 3], suffix);
    ret = 1;
  }

  testBegin("_pdfioRC4Init/Decrypt(128-bit)");
  _pdfioCryptoRC4Init(&rc4, key, 16);
  _pdfioCryptoRC4Crypt(&rc4, buffer2, buffer, strlen(text));

  if (!memcmp(buffer2, text, strlen(text)))
  {
    testEnd(true);
  }
  else
  {
    for (i = 0; text[i + 4]; i ++)
    {
      if (buffer2[i] != text[i])
        break;
    }

    prefix = i > 0 ? "..." : "";
    suffix = text[i + 4] ? "..." : "";

    testEndMessage(false, "got '%s%02X%02X%02X%02X%s', expected '%s%02X%02X%02X%02X%s'", prefix, buffer2[i], buffer2[i + 1], buffer2[i + 2], buffer2[i + 3], suffix, prefix, text[i], text[i + 1], text[i + 2], text[i + 3], suffix);
    ret = 1;
  }

  testBegin("_pdfioSHA256Init/Append/Finish");
  _pdfioCryptoSHA256Init(&sha256);
  _pdfioCryptoSHA256Append(&sha256, (uint8_t *)text, strlen(text));
  _pdfioCryptoSHA256Finish(&sha256, buffer);

  if (!memcmp(sha256text, buffer, sizeof(sha256text)))
  {
    testEnd(true);
  }
  else
  {
    testEndMessage(false, "got '%02X%02X%02X%02X...%02X%02X%02X%02X', expected '%02X%02X%02X%02X...%02X%02X%02X%02X'", buffer[0], buffer[1], buffer[2], buffer[3], buffer[28], buffer[29], buffer[30], buffer[31], sha256text[0], sha256text[1], sha256text[2], sha256text[3], sha256text[28], sha256text[29], sha256text[30], sha256text[31]);
    ret = 1;
  }

  return (ret);
}


//
// 'do_pdfa_tests()' - Run PDF/A generation and compliance tests.
//

static int				// O - 0 on success, 1 on error
do_pdfa_tests(void)
{
  int		status = 0;		// Overall status
  pdfio_file_t	*pdf;			// PDF file for encryption test
  bool		error = false;		// Error flag


  // Test creation of files using various PDF/A standards
  status |= write_pdfa_file("testpdfio-pdfa-1a.pdf", "PDF/A-1a");
  status |= write_pdfa_file("testpdfio-pdfa-1b.pdf", "PDF/A-1b");
  status |= write_pdfa_file("testpdfio-pdfa-2a.pdf", "PDF/A-2a");
  status |= write_pdfa_file("testpdfio-pdfa-2b.pdf", "PDF/A-2b");
  status |= write_pdfa_file("testpdfio-pdfa-2u.pdf", "PDF/A-2u");
  status |= write_pdfa_file("testpdfio-pdfa-3a.pdf", "PDF/A-3a");
  status |= write_pdfa_file("testpdfio-pdfa-3b.pdf", "PDF/A-3b");
  status |= write_pdfa_file("testpdfio-pdfa-3u.pdf", "PDF/A-3u");
  status |= write_pdfa_file("testpdfio-pdfa-4.pdf", "PDF/A-4");

  // Test that encryption is not allowed for PDF/A files
  testBegin("pdfioFileCreate(testpdfio-pdfa-rc4.pdf)");
  if ((pdf = pdfioFileCreate("testpdfio-pdfa-rc4.pdf", "PDF/A-1b", /*media_box*/NULL, /*crop_box*/NULL, (pdfio_error_cb_t)error_cb, &error)) == NULL)
  {
    testEnd(false);
    return (1);
  }

  testEnd(true);

  testBegin("pdfioFileSetPermissions(PDFIO_ENCRYPTION_RC4_128)");
  if (pdfioFileSetPermissions(pdf, PDFIO_PERMISSION_ALL, PDFIO_ENCRYPTION_RC4_128, "owner", "user"))
  {
    testEndMessage(false, "incorrectly allowed encryption");
    status = 1;
  }
  else
  {
    testEndMessage(true, "correctly prevented encryption");
  }

  pdfioFileClose(pdf);

  return (status);
}


//
// 'do_test_file()' - Try loading a PDF file and listing pages and objects.
//

static int				// O - Exit status
do_test_file(const char *filename,	// I - PDF filename
             const char *outfile,	// I - Output filename, if any
             int        objnum,		// I - Object number to dump, if any
             const char *password,	// I - Password for file
             bool       verbose)	// I - Be verbose?
{
  int		status = 0;		// Exit status
  bool		error = false;		// Have we shown an error yet?
  pdfio_file_t	*pdf,			// PDF file
		*outpdf;		// Output PDF file, if any
  size_t	n,			// Object/page index
		num_objs,		// Number of objects
		num_pages;		// Number of pages
  pdfio_obj_t	*obj;			// Object
  pdfio_dict_t	*dict;			// Object dictionary


  // Try opening the file...
  if (!objnum)
  {
    if (outfile)
      testBegin("%s -> %s", filename, outfile);
    else
      testBegin("pdfioFileOpen(%s)", filename);
  }

  if ((pdf = pdfioFileOpen(filename, password_cb, (void *)password, (pdfio_error_cb_t)error_cb, &error)) != NULL)
  {
    if (objnum)
    {
      const char	*filter;	// Stream filter
      pdfio_stream_t	*st;		// Stream
      char		buffer[8192];	// Read buffer
      ssize_t		bytes;		// Bytes read

      if ((obj = pdfioFileFindObj(pdf, (size_t)objnum)) == NULL)
      {
        puts("Not found.");
        pdfioFileClose(pdf);
        return (1);
      }

      if ((dict = pdfioObjGetDict(obj)) == NULL)
      {
        _pdfioValueDebug(&obj->value, stdout);
	putchar('\n');
        pdfioFileClose(pdf);
        return (0);
      }

      filter = pdfioDictGetName(dict, "Filter");

      if ((st = pdfioObjOpenStream(obj, filter && !strcmp(filter, "FlateDecode"))) == NULL)
      {
        _pdfioValueDebug(&obj->value, stdout);
	putchar('\n');
        pdfioFileClose(pdf);
        return (0);
      }

      while ((bytes = pdfioStreamRead(st, buffer, sizeof(buffer))) > 0)
        fwrite(buffer, 1, (size_t)bytes, stdout);

      pdfioStreamClose(st);
      pdfioFileClose(pdf);

      return (0);
    }
    else
    {
      if (outfile)
      {
        if (!access(outfile, 0))
        {
          testEndMessage(false, "output file already exists");
          pdfioFileClose(pdf);
	  return (1);
        }

        // Copy pages to the output file...
        if ((outpdf = pdfioFileCreate(outfile, pdfioFileGetVersion(pdf), /*media_box*/NULL, /*crop_box*/NULL, (pdfio_error_cb_t)error_cb, &error)) != NULL)
        {
          for (n = 0, num_pages = pdfioFileGetNumPages(pdf); n < num_pages; n ++)
          {
	    if (!pdfioPageCopy(outpdf, pdfioFileGetPage(pdf, n)))
	    {
	      status = 1;
	      break;
	    }
          }

          pdfioFileClose(outpdf);
          testEnd(true);
        }
      }
      else
      {
	// Show basic stats...
	num_objs  = pdfioFileGetNumObjs(pdf);
	num_pages = pdfioFileGetNumPages(pdf);

	testEndMessage(true, "PDF %s, %d pages, %d objects", pdfioFileGetVersion(pdf), (int)num_pages, (int)num_objs);

	if (verbose)
	{
	  // Show a summary of each page...
	  for (n = 0; n < num_pages; n ++)
	  {
	    testBegin("pdfioFileGetPage(%u)", (unsigned)n);

	    if ((obj = pdfioFileGetPage(pdf, n)) != NULL)
	    {
	      pdfio_rect_t	media_box;
					// MediaBox value
	      pdfio_stream_t	*st;	// Page content stream

	      memset(&media_box, 0, sizeof(media_box));
	      dict = pdfioObjGetDict(obj);

	      if (!pdfioDictGetRect(dict, "MediaBox", &media_box))
	      {
		if ((obj = pdfioDictGetObj(dict, "Parent")) != NULL)
		{
		  dict = pdfioObjGetDict(obj);
		  pdfioDictGetRect(dict, "MediaBox", &media_box);
		}
	      }

	      if ((st = pdfioPageOpenStream(obj, /*number*/0, /*decode*/true)) != NULL)
	      {
	        char	buffer[8192];	// Content buffer
	        ssize_t	bytes;		// Number of bytes read
	        size_t	length = 0;	// Content length

	        while ((bytes = pdfioStreamRead(st, buffer, sizeof(buffer))) > 0)
	          length += (size_t)bytes;

	        pdfioStreamClose(st);

		testEndMessage(true, "page #%d/obj %d is %gx%g, content is %lu bytes", (int)n + 1, (int)pdfioObjGetNumber(obj), media_box.x2, media_box.y2, (unsigned long)length);
	      }
	      else
	      {
	        testEndMessage(false, "unable to open content stream");
	        status = 1;
	      }
	    }
	    else
	    {
	      testEnd(false);
	      status = 1;
	    }
	  }

	  // Show the associated value with each object...
	  for (n = 0; n < num_objs; n ++)
	  {
	    testBegin("pdfioFileGetObj(%u)", (unsigned)n);

	    if ((obj = pdfioFileGetObj(pdf, n)) != NULL)
	    {
	      dict = pdfioObjGetDict(obj);

	      testEndMessage(true, "%u %u obj dict=%p/%lu pairs", (unsigned)pdfioObjGetNumber(obj), (unsigned)pdfioObjGetGeneration(obj), (void *)dict, dict ? (unsigned long)dict->num_pairs : 0UL);
	      fputs("        ", stderr);
	      _pdfioValueDebug(&obj->value, stderr);
	      fputs("\n", stderr);
	    }
	    else
	    {
	      testEnd(false);
	      status = 1;
	    }
	  }
	}
      }
    }

    // Close the file and return success...
    pdfioFileClose(pdf);
    return (status);
  }
  else
  {
    // Error message will already be displayed so just indicate failure...
    if (!objnum)
      testEnd(false);

    return (1);
  }
}


//
// 'do_unit_tests()' - Do unit tests.
//

static int				// O - Exit status
do_unit_tests(void)
{
  pdfio_file_t		*inpdf,		// Input PDF file
			*outpdf;	// Output PDF file
  int			outfd;		// Output file descriptor
  bool			error = false;	// Error callback data
  _pdfio_token_t	tb;		// Token buffer
  const char		*s;		// String buffer
  _pdfio_value_t	value;		// Value
  size_t		first_image,	// First image object
			num_pages;	// Number of pages written
  char			temppdf[1024];	// Temporary PDF file
  pdfio_dict_t		*dict;		// Test dictionary
  int			count = 0;	// Number of key/value pairs
  static const char	*complex_dict =	// Complex dictionary value
    "<</Annots 5457 0 R/Contents 5469 0 R/CropBox[0 0 595.4 842]/Group 725 0 R"
    "/MediaBox[0 0 595.4 842]/Parent 23513 0 R/Resources<</ColorSpace<<"
    "/CS0 21381 0 R/CS1 21393 0 R>>/ExtGState<</GS0 21420 0 R>>/Font<<"
    "/TT0 21384 0 R/TT1 21390 0 R/TT2 21423 0 R/TT3 21403 0 R/TT4 21397 0 R>>"
    "/ProcSet[/PDF/Text/ImageC]/Properties<</MC0 5472 0 R/MC1 5473 0 R>>"
    "/XObject<</E3Dp0QGN3h9EZL2X 23690 0 R/E6DU0TGl3s9NZT2C 23691 0 R"
    "/ENDB06GH3u9tZT2N 21391 0 R/ENDD0NGM339cZe2F 23692 0 R"
    "/ENDK00GK3c9DZN2n 23693 0 R/EPDB0NGN3Q9GZP2t 23695 0 R"
    "/EpDA0kG03o9rZX21 23696 0 R/Im0 5475 0 R>>>>/Rotate 0/StructParents 2105"
    "/Tabs/S/Type/Page>>";
  static const char	*cid_dict =	// CID font dictionary
    "<</BaseFont/ZGFJJN+Cambria/CIDSystemInfo 21459 0 R/CIDToGIDMap/I"
    "dentity/DW 1000/FontDescriptor 21458 0 R/Subtype/CIDFontType2/Ty"
    "pe/Font/W[0[658 0 220 220 623 611 563 662 575 537 611 687 324 30"
    "7 629 537 815 681 653 568 653 621 496 593 648 604 921 571 570 53"
    "8 623 623 623 623 623 623 623 623 623 623 866 866 563 563 563 56"
    "3 563 662 665 575 575 575 575 575 575 575 575 575 611 611 611 61"
    "1 687 691 324 324 324 324 324 324 324 324 324 631 307 629 537 53"
    "7 537 540 622 681 681 681 681 653 653 653 653 653 653 653 653 65"
    "3 653 929 621 621 621 496 496 496 496 496 593 593 593 648 648 64"
    "8 648 648 648 648 648 648 648 921 921 921 921 570 570 570 570 53"
    "8 538 538 681 665 574 488 547 441 555 488 303 494 552 278 266 52"
    "4 271 832 558 531 556 547 414 430 338 552 504 774 483 504 455 48"
    "8 488 488 488 488 488 488 488 488 488 752 752 441 441 441 441 44"
    "1 678 555 488 488 488 488 488 488 488 488 488 494 494 494 494 55"
    "2 552 278 278 278 278 278 278 278 278 278 544 266 524 531 271 38"
    "5 271 309 376 558 558 558 558 690 531 531 531 531 531 531 531 53"
    "1 531 531 818 414 414 414 430 430 430 430 430 605 279 451 338 35"
    "3 552 552 552 552 552 552 552 552 552 552 774 774 774 774 504 50"
    "4 504 504 455 455 455 551 530 547 545 555 492 583 511 479 538 61"
    "1 297 279 558 464 716 596 564 525 564 552 455 528 572 535 830 51"
    "3 511 482 545 545 545 545 545 545 545 545 545 545 765 765 492 49"
    "2 492 492 492 583 587 511 511 511 511 511 511 511 511 511 538 53"
    "8 538 538 611 616 297 297 297 297 297 297 297 297 297 576 279 55"
    "8 464 464 464 471 554 596 596 596 596 564 564 564 564 564 564 56"
    "4 564 564 564 809 552 552 552 455 455 455 455 455 528 528 528 57"
    "2 572 572 572 572 572 572 572 572 572 830 830 830 830 511 511 51"
    "1 511 482 482 482 596 587 533 367 409 333 413 366 240 371 411 22"
    "0 213 387 215 607 416 396 414 409 314 326 264 412 384 574 363 38"
    "4 343 852 1155 605 854 1157 581 883 569 872 827 1130 574 876 688"
    " 627 285 285 0 0 285 285 0 0 285 285 0 0 285 285 0 0 113 285 285"
    " 0 0 285 285 0 0 285 285 0 0 285 285 0 0 285 285 0 0 285 285 0 0"
    " 285 285 0 0 285 0 285 0 0 0 205 264 264 205 752 332 332 286 286"
    " 286 422 422 422 221 221 221 221 375 375 375 375 205 360 303 303"
    " 303 303 488 488 488 488 490 316 500 500]518[750 750 443 282 247"
    " 382 382 382 382 350 350 350 350 387 387 387 387 427 517 517 500"
    " 588 623 611 536 596 575 538 687 653 324 629 605 815 681 572 653"
    " 674 568 548 593 607 785 571 783 679 619 706 813 450 324 736 778"
    " 607 756 574 555 513 507 525 444 400 543 547 546 284 527 488 546"
    " 482 405 531 602 532 572 431 478 541 712 722 486 719 751 574 444"
    " 543 284 284 284 531 541 541 541 751 545 555 465 524 511 482 611"
    " 564 297 558 534 716 596 498 564 596 525 485 528 525 667 513 700"
    " 608 545 511 611 297 297 564 525 525 608 285 0 285 0 244 244 244"
    " 264 195 195 623 599 611 534 655 575 923 543 692 692 650 678 815"
    " 687 653 674 568 563 593 600 779 571 672 626 927 927 732 850 597"
    " 573 921 631 534 502 739 570 496 575 575 324 324 307 692 650 935"
    " 932 753 600 667 722 653 661 488 541 523 447 558 488 714 458 590"
    " 590 540 559 673 589 531 567 556 441 511 504 685 483 570 531 801"
    " 801 617 748 514 470 767 531 447 417 538 470 430 488 488 278 278"
    " 266 590 540 763 798 552 504 567 622 528 555 529 545 542 555 478"
    " 573 511 839 487 613 613 577 594 716 611 564 596 525 492 528 513"
    " 669 513 599 558 844 844 649 787 536 498 814 570 478 455 666 491"
    " 455 511 511 297 297 279 613 577 816 843 671 513 596 651 564 587"
    " 285 285 0 0 554 712 316 490 371 237 393 885 885 851 851 679 855"
    " 544 628 526 628 554 506 526 506 554 441 528 526 528 554 561 609"
    " 526 609 554 417 428 1039 619 526 343 460 461 526 464 527 460 52"
    "6 527 526 526 526 526 526 526 526 526 526 526 554 362 484 486 54"
    "9 489 544 481 554 543 554 554 554 554 554 554 554 554 554 554 40"
    "7 407 407 407 407 407 407 407 407 407 407 407 407 292 292 407 40"
    "7 407 407 407 407 407 407 407 407 407 407 407 292 292 407 407 40"
    "7 407 407 407 407 407 407 407 554 865 890 865 890 1263 554 554 5"
    "54 554 247 554 114 554 554 554 554 554 554 554 554 375 518 851 5"
    "45 425 657 596 679 544 812 708 539 427 407 407 407 407 407 407 4"
    "07 407 407 407 303 461 448 415 481 419 385 451 495 242 238 460 3"
    "88 585 493 479 417 479 450 372 420 479 447 669 425 424 392 461 4"
    "48 415 481 419 385 451 495 242 238 460 388 585 493 479 417 479 4"
    "50 372 420 479 447 669 425 424 392 367 409 333 413 366 240 371 4"
    "11 220 213 387 215 607 416 396 414 409 314 326 264 412 384 574 3"
    "63 384 343 461 448 383 442 419 392 495 479 242 460 450 585 493 4"
    "17 479 486 417 400 420 442 559 425 572 500 438 415 398 391 335 3"
    "04 416 407 242 405 390 415 361 315 396 445 396 431 325 337 402 5"
    "15 372 526 535 461 448 383 442 419 392 495 479 242 460 450 585 4"
    "93 417 479 486 417 400 420 442 559 425 572 500 438 415 398 391 3"
    "35 304 416 407 242 405 390 415 361 315 396 445 396 431 325 337 4"
    "02 515 372 526 535 554 387 270 270 413 413 620 413 413 413 395 3"
    "95 395 395 395 395 395 395 395 395 270 270 413 413 620 413 413 4"
    "13 395 395 395 395 395 395 395 395 395 395 0 0 0 0 0 0 0 0 0 0 0"
    " 0 0 0 65 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 "
    "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 "
    "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 539 558 607 707 796 543 653 531 "
    "530 429 537 433 472 432 556 547 570 530 438 266 653 457 457 500]"
    "1306[554 205 167 56 0 544 544 529 258 430 607 515 368 604 904 64"
    "7 568 571 878 636 692 805 588 657 835 720 851 778 691 764 759 74"
    "5 603 455 679 667 283 629 623 654 486 537 644 597 544 417 476 39"
    "7 912 653 576 795 732 601 478 478 561 808 694 612 411 423 676 69"
    "0 838 468 838 468 921 468 652 652 652 652 838 838 885 885 936 48"
    "7 936 487 865 865 872 468 872 468 488 823 823 891 891 1191 950 5"
    "79 566 566 566 566 635 565 809 809 665 904 782 782 838 838 433 4"
    "33 838 838 433 433 838 784 838 838 798 838 798 838 838 875 1079 "
    "875 866 586 866 586 1035 586 731 731 731 731 889 889 979 979 487"
    " 487 882 468 882 468 862 862 860 593 860 593 593 609 609 609 609"
    " 620 886 772 772 609 945 784 861 838 838 950 887 887 999 891 891"
    " 1072 628 522 587 604 628 596 623 640 554 623 623 554 781 812 74"
    "7 747 554 483 443 636 636 703 691 658 658 677 293 375 464 544 59"
    "0 590 681 681 586 923 1260 601 950 1286 633 632 650 573 573 305 "
    "593 723 935 776 735 712 736 807 687 366 712 747 747 747 747 747 "
    "747 736 747 736 747 747 747 747 747 747 747 747 935 935 747 747 "
    "747 747 747 747 747 839 747 747 747 747 747 747 747 747 747 891 "
    "891 479 747 747 747 747 747 747 747 747 747 747 747 747 747 733 "
    "733 733 733 736 736 733 733 747 747 747 747 747 747 747 747 747 "
    "747 607 607 607 747 747 747 747 715 715 995 995 995 995 995 995 "
    "995 995 995 977 977 977 977 674 662 663 465 663 465 520 520 720 "
    "753 890 905 720 720 814 905 554 554 723 723 723 723 1026 1026 90"
    "5 723 404 590 590 590 691 700 665 665 771 771 450 282 519 573 83"
    "0 754 754 715 715 723 590 590 716 716 760 760 686 648 749 749 12"
    "40 1240 751 751 749 749 733 733 733 733 747 747 747 747 747 747 "
    "736 736 739 739 748 755 311 856 618 618 733 624 555 624 624 555 "
    "624 624 733 624 555 633 555 730 350 350 350 350 359 359 1228 122"
    "8 758 758 758 758 758 758 350 350 350 350 350 350 685 685 685 68"
    "5 685 685 685 1228 525 722 722 1488 1488 616 616 724 643 440 440"
    " 359 359 870 870 870 696 732 785 531 679 666 588 719 592 566 661"
    " 736 364 355 696 565 861 694 710 628 710 681 528 654 691 646 974"
    " 619 617 581 550 606 483 612 545 417 540 611 329 317 609 323 905"
    " 618 584 612 606 476 474 380 612 541 808 525 541 494 633 656 597"
    " 692 625 611 630 728 379 382 698 532 851 732 676 620 676 637 529"
    " 595 689 625 950 645 603 592 557 539 460 580 496 550 612 555 317"
    " 359 552 316 838 574 533 558 539 476 462 395 574 544 739 532 560"
    " 483 452 460 405 485 428 421 444 508 264 331 487 370 595 512 473"
    " 440 473 445 378 416 497 443 664 477 424 422 412 392 327 415 350"
    " 387 422 408 229 298 392 231 614 431 384 416 392 346 302 288 431"
    " 346 533 386 419 342 452 460 405 485 428 421 444 508 264 331 487"
    " 370 595 512 473 440 473 445 378 416 497 443 664 477 424 422 412"
    " 392 327 415 350 387 422 408 229 298 392 231 614 431 384 416 392"
    " 346 302 288 431 346 533 386 419 342 633 656 608 613 625 592 728"
    " 680 379 698 625 851 732 603 676 716 620 680 607 595 623 731 645"
    " 762 679 613 600 606 525 538 455 475 524 563 260 541 518 548 509"
    " 482 533 593 564 585 432 458 506 677 581 704 726 556 465 586 577"
    " 683 536 799 452 460 418 427 428 422 508 473 264 487 443 595 512"
    " 447 473 508 440 473 418 416 453 534 477 558 500 421 435 455 393"
    " 389 329 347 404 419 223 391 383 441 381 356 384 434 429 427 319"
    " 331 383 495 447 522 533 405 339 450 432 503 396 574 452 460 418"
    " 427 428 422 508 473 264 487 443 595 512 447 473 508 440 473 418"
    " 416 453 534 477 558 500 421 435 455 393 389 329 347 404 419 223"
    " 391 383 441 381 356 384 434 429 427 319 331 383 495 447 522 533"
    " 405 339 450 432 503 396 574 662 723 627 750 656 639 671 788 436"
    " 421 767 566 913 760 732 687 732 697 567 624 727 649 983 692 635"
    " 632 621 607 505 644 549 563 677 632 371 389 612 370 927 647 577"
    " 621 604 537 502 419 646 600 788 577 598 517 662 723 635 676 656"
    " 632 788 737 436 767 649 913 760 617 732 788 687 737 636 624 670"
    " 824 692 838 733 676 671 677 603 580 515 517 602 624 333 608 563"
    " 636 567 516 577 686 621 630 479 510 571 756 657 777 800 608 497"
    " 656 623 764 588 872 892 679 602 734 567 698 575 933 455 618 870"
    " 590 1104 967 662 728 651 737 574 671 768 681 1052 822 738 688 6"
    "44 740 479 659 571 734 463 479 710 1031 747 713 622 733 538 544 "
    "741 740 1011 600 730 557 941 736 650 802 607 747 640 1006 520 67"
    "2 922 636 1188 1012 726 797 709 825 597 727 829 707 1085 872 803"
    " 714 664 758 507 680 528 610 668 752 478 527 726 561 1051 767 67"
    "3 753 669 747 562 554 757 757 1042 646 760 600 717 773 797 661 6"
    "62 754 647 713 611]2377[770 805 767 814 748 713 679 762 972 633 "
    "722 488 497 366 488 400 306 500 503 271 271 336 270 786 530 480 "
    "514 497 368 418 310 536 483 757 416 511 375 700 732 785 637 612 "
    "730 423 406 765 603 908 764 583 678 703 673 1029 680 659 594 645"
    " 520 652 586 445 569 669 373 361 644 367 960 676 616 654 645 496"
    " 481 419 669 565 863 577 566 537 808 795 726 816 726 684 798 747"
    " 670 677 770 646 1053 818 832 793 850 813 748 744 732 795 1072 6"
    "88 775 707 550 542 403 534 444 339 548 543 325 326 367 324 859 6"
    "14 531 565 544 433 447 333 605 567 813 498 568 411 571 589 517 6"
    "48 500 483 601 653 286 286 575 478 793 681 653 546 653 571 473 5"
    "13 637 560 920 532 539 510 491 551 433 551 501 341 501 529 250 2"
    "50 476 250 803 529 535 551 551 380 410 343 529 464 758 456 464 4"
    "38 644 641 543 695 524 506 651 693 323 323 627 500 850 722 693 5"
    "99 693 629 511 542 680 620 980 590 594 534 543 595 459 595 544 3"
    "79 557 577 291 291 542 291 857 577 577 595 595 418 435 373 577 5"
    "11 810 510 511 459 629 607 575 662 564 549 595 663 295 331 659 4"
    "13 803 691 668 590 668 580 504 512 646 625 987 690 543 593 520 5"
    "58 469 602 515 437 583 529 293 335 544 295 803 529 544 580 556 4"
    "27 410 396 529 510 805 555 555 486 700 669 607 726 593 577 646 7"
    "22 352 375 720 451 878 750 718 652 718 650 542 548 698 684 1044 "
    "744 599 623 576 612 502 657 561 475 638 588 347 376 607 350 869 "
    "588 596 635 611 476 436 421 588 557 854 609 602 518 679 666 566 "
    "647 592 581 736 720 364 696 661 861 694 575 710 731 628 710 578 "
    "654 658 849 619 842 736 647 638 611 565 581 501 451 625 596 354 "
    "609 566 622 526 453 584 656 588 475 621 503 576 766 547 815 804 "
    "586 486 631 573 776 581 868 598 598 598 598 598 598 598 598 598 "
    "598 669 462 556 562 620 561 635 533 647 635 554 554 554 554 554 "
    "554 554 554 554 554 612 612 612 612 612 612 612 612 612 612 600 "
    "600 600 600 600 600 600 600 600 600 550 266 314 302 220 213 220 "
    "213 317 359 264 211 264 211 371 389 463 479 478 527 271 271 325 "
    "326 373 361 250 250 291 291 293 335 347 376 379 325 400 429 503 "
    "401 544 579 579 579 579 579 579 579 579 579 551 728 728 728 425 "
    "425 705 705 699 682 629 735 640 592 685 759 364 354 702 594 899 "
    "755 729 635 729 689 561 649 727 679 1023 644 641 599 552 617 501"
    " 625 552 354 559 622 324 312 588 317 926 628 599 626 617 472 489"
    " 391 623 576 870 547 576 516 699 682 590 670 640 599 759 729 364"
    " 702 682 899 755 636 729 745 635 611 649 675 862 644 873 759 655"
    " 627 589 592 504 456 622 615 345 605 573 623 545 468 599 676 599"
    " 648 490 523 609 789 555 802 825 609 481 572 570 748 530 817 584"
    " 402 402 737 737 924 728 625 625 662 678 606 716 639 626 654 751"
    " 391 482 720 547 880 756 698 646 698 657 553 614 723 652 980 688"
    " 625 619 625 599 505 639 542 600 660 622 351 469 605 353 935 649"
    " 589 629 599 529 484 440 649 565 818 591 633 529 697 714 623 631"
    " 673 651 791 665 411 758 652 926 796 644 734 746 700 681 623 646"
    " 657 774 688 808 721 627 666 686 593 597 504 529 602 633 318 599"
    " 582 646 575 542 589 660 644 651 485 507 576 754 668 792 811 618"
    " 519 672 652 764 601 882 324 312 351 469 595 595 595 595 595 595"
    " 595 595 595 595 747 747 747 433 433 763 763 756 734 680 789 688"
    " 631 739 813 396 390 755 636 960 809 786 685 786 738 610 689 785"
    " 734 1098 697 695 643 602 671 547 678 601 394 608 674 360 349 63"
    "5 354 996 682 650 679 671 516 534 433 675 630 941 596 630 563 75"
    "6 734 629 725 688 643 813 786 396 755 739 960 809 683 786 798 68"
    "5 657 689 725 917 697 939 820 719 681 654 642 549 498 682 667 39"
    "6 665 639 681 593 518 650 730 649 707 534 554 660 845 609 864 87"
    "8 609 481 588 570 763 547 829 634 443 443 737 737 1018 747 677 6"
    "77 742 754 664 795 703 691 728 834 433 542 799 606 977 840 775 7"
    "23 775 730 620 682 814 728 1089 783 696 693 675 643 537 681 574 "
    "635 692 670 375 489 643 380 1007 707 630 683 643 568 495 473 707"
    " 575 875 634 688 562 742 754 687 700 703 693 834 775 433 799 728"
    " 977 840 733 775 834 723 775 687 682 743 876 783 916 820 691 713"
    " 747 644 639 540 569 663 688 366 641 629 724 625 585 630 711 704"
    " 700 524 542 629 813 734 857 874 664 557 738 709 826 650 941 360"
    " 349 375 489 430 567 945 430 567 945 430 567 945 430 567 945 368"
    " 368 625 643 539 539 368 0 0 0 517 721 0 0 0 620 0 0 517 616 794"
    " 747 747 747 714 282 747 514 747 747 736 749 749 749 749 747 784"
    " 574 575 551 804 1060 1315 387 387 396 646 387 387 396 646 356 3"
    "79 399 421 356 379 399 421 449 494 609 733 449 494 609 733 469 4"
    "86 507 531 469 486 507 531 359 359 385 420 359 359 385 420 356 3"
    "79 399 421 356 379 399 421 356 379 421 421 356 379 399 421 350 3"
    "56 379 399 421 350 356 379 399 421 350 350 350 350 350 350 350 3"
    "50 743 540 750 765 768 749 833 847 930 1017 1475 594 807 1034 13"
    "04 951 1204 1533 1308 1602 2031 607 807 1034 951 1204 1533 1306 "
    "1602 2031 636 807 1179 636 807 1034 636 807 1034 1134 1326 2081 "
    "2899 1010 1509 2149 2919 406 567 945 1806 391 555 942 1803 406 5"
    "67 945 1806 406 555 942 1803 391 567 942 1804 391 567 942 1804 3"
    "91 555 945 1807 391 555 945 1807 391 555 945 1807 391 555 945 18"
    "07 617 996 1467 1733 2133 617 996 1467 1733 2133 994 1465 1731 2"
    "131 994 1465 1731 2131 617 994 1465 1731 2131 617 994 1465 1731 "
    "2131 616 994 1465 1731 2131 574 600 626 574 600 626 417 417 461 "
    "497 408 417 461 497 316 323 332 529 550 578 982 982 953 953 1246"
    " 1179 1179 1179 1179 1179 1179 1179 1179 1179 1179 1179 936 936 "
    "866 866 1035 936 866 866 478 478 717 717 882 882 882 882 1043 83"
    "8 468 468 865 865 1043 1043 1043 865 865 865 865 882 882 882 882"
    " 746 746 650 650 650 650 779 739 779 739 698 698 739 739 739 739"
    " 779 779 847 870 870 563 563 408 408 808 808 808 808 655 655 763"
    " 763 838 838 838 838 838 838 921 487 838 838 494 494 838 433 838"
    " 433 862 862 433 433 862 862 433 433 872 872 433 433 872 872 433"
    " 433 838 714 838 714 838 838 838 838 838 838 838 838 714 714 838"
    " 838 838 838 838 838 723 1043 723 765 921 765 556 556 598 598 13"
    "16 2128 1316 2128 1316 2128 1022 1548 1021 1548 732 928 928 974 "
    "1258 974 1258 1231 708 1134 2081 708 1134 2081 1597 1665 2530 58"
    "6 594 1034 586 594 1034 586 594 1034 586 594 1034 586 594 1034 5"
    "86 594 1034 586 594 1034 586 594 1034 586 594 1034 586 594 1034 "
    "823 823 1034 586 594 1034 586 594 1034 586 594 1034 586 594 1034"
    " 586 594 1034 727 727 727 743 590 681 714 714 1020 1020 1149 761"
    " 761 1223 1174 905 905 905 430 539 679 764 764 843 843 635 432 3"
    "71 407 407 444 444 407 407 383 383 350 350 350 350 350 350 372 3"
    "72 749 749 749 749 388 388 284 291 658 714 714 673 668 677 673 6"
    "58 658 658 658 707 707 658 658 658 658 721 721 721 721 995 995 9"
    "95 995 995 1098 995 995 995 995 995 995 995 995 995 995 995 995 "
    "1167 1167 699 699 699 699 699 1011 724 724 724 724 722 818 818 7"
    "87 787 787 787 787 547 547 295 295 442 442 851 851 851 1218 699 "
    "723 747 747 747 747 747 553 724 724 771 539 837 837 636 636 636 "
    "636 636 636 838 514 514 514 804 804 747 747 747 747 740 1037 635"
    " 359 954 433 747 747 747 747 747 779 747 747 747 747 747 995 995"
    " 714 714 714 714 714 995 995 995 995 995 977 977 977 747 747 300"
    " 740 681 681 681 681 681 681 569 569 598 598 896 896 710 710 732"
    " 732 710 590 590 590 590 760 760 641 641 590 590 590 590 590 590"
    " 590 590 747 590 590 741 741 747 747 747 747 735 735 747 747 747"
    " 736 747 747 747 747 1084 1212 1628 747 747 747 747 747 747 747 "
    "747 747 747 747 747 747 747 747 747 747 747 747 747 747 747 747 "
    "747 751 751 747 747 747 747 749 749 749 749 747 747 749 749 747 "
    "747 747 747 749 749 1188 944 1396 747 747 747 747 747 747 747 74"
    "7 747 747 747 747 747 747 747 747 747 747 747 747 747 1172 1172 "
    "747 747 747 747 747 747 747 747 747 747 747 747 747 747 747 747 "
    "1176 1176 747 747 747 747 747 747 747 747 1397 1397 681 686 686 "
    "681 681 466 663 663 663 720 753 720 905 753 663 663 663 663 663 "
    "747 747 375 430 430 430 612 612 635 759 284 980 980 749 749 833 "
    "635 674 464 464 536 608 962 554 554 554 554 357 1307 1307 1307 1"
    "307 747 465 958 958 722 577 483 1096 1096 747 706 623 667 667 66"
    "7 667 959 959 900 900 1220 998 1288 520 477 680 680 865 740 998 "
    "607 998 998 998 998 998 998 998 998 998 998 998 998 998 998 998 "
    "998 998 998 998 998 998 998 998 998 998 998 998 998 998 998 998 "
    "998 607 821 998 607 998 998 998 998 998 998 821 607 607 998 998 "
    "998 712 998 607 821 711 711 607 998 998 890 890 284 532 751 574 "
    "457 284 751 574 669 937 952 939 371 740 998 998 969 785 684 675 "
    "843 637 678 678 678 808 786 747 658 658 658 658 658 743 998 472 "
    "789 789 815 815 503 503 503 503 503 503 881 881 881 881 881 881 "
    "881 881 881 881 881 881 881 881 881 998 777 998 644 641 500 647 "
    "524 534 693 693 323 627 644 850 722 565 693 686 693 599 548 542 "
    "594 774 590 759 736 647 624 611 518 581 479 451 625 596 354 583 "
    "561 577 526 453 577 642 582 475 621 470 596 756 529 777 802 586 "
    "476 607 565 776 581 868 700 669 634 683 593 623 722 718 352 720 "
    "700 878 750 650 718 756 718 652 630 548 599 807 744 789 795 647 "
    "683 670 572 596 524 546 589 617 299 594 597 616 534 543 596 683 "
    "620 521 668 487 602 772 602 777 827 627 534 636 672 792 617 913 "
    "601 601 601 601 601 601 601 601 601 601 601 601 601 601 601 601 "
    "601 601 601 601 601 601 601 601 601 601 601 601 601 601 601 601 "
    "601 601 601 601 601 601 601 601 601 601 601 601 601 601 601 601 "
    "601 601 601 601 541 350 350 1392 1392 2149 2149 1879 656 724 683"
    " 572 601 305 836 920 836 732 732 732 836 920 836 732 732 732 468"
    " 586 356 426 581 690 803 955 1021 1218 919 653 653 679 679 446 4"
    "46 845 845 845 653 586 586 586 1098 643 391 391 402 402 525 402 "
    "402 223 344 344 377 377 906 214 214 294 294 323 346 272 305 221 "
    "351 1010 1509 2149 2919 912 1435 912 1435 928 1392 928 1392 293 "
    "782 782 806 806 585 585 585 585 121 0 0 571 796 415 415 387 387 "
    "350 350 490 490 365 365 424 424 574 574 384 384 498 498 682 682 "
    "408 408 595 595 365 365 365 365 365 365 478 478 321 641 384 384 "
    "384 384 384 384 495 495 329 600 408 408 408 408 408 408 519 519 "
    "340 632 346 663 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 623 "
    "623 623 623 623 623 623 623 623 623 623 623 575 575 575 575 575 "
    "575 575 575 324 324 653 653 653 653 653 653 653 726 726 726 726 "
    "726 726 648 648 754 754 754 754 754 754 570 570 570 488 488 488 "
    "488 488 488 488 488 488 488 488 488 488 488 488 488 488 488 488 "
    "488 278 278 531 531 531 531 531 531 531 599 599 599 599 599 599 "
    "552 552 623 623 623 623 623 623 504 504 504 545 545 545 545 545 "
    "545 545 545 545 545 545 545 511 511 511 511 511 511 511 511 297 "
    "297 564 564 564 564 564 564 564 635 635 635 635 635 635 572 572 "
    "671 671 671 671 671 671 511 511 511 554 593 338 528 601 658 666 "
    "656 572 580 572 278 303 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 "
    "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 "
    "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 702 599 597 563 56"
    "3 665 752 599 580 653 543 537 611 618 315 324 629 917 681 653 81"
    "8 658 583 496 548 593 593 647 647 621 538 533 535 560 1199 1116 "
    "844 803 987 947 623 324 653 648 648 648 648 648 623 623 866 616 "
    "611 629 653 653 533 1199 1116 611 922 1013 626 681 623 623 575 5"
    "75 324 324 653 653 621 621 648 648 543 687 666 583 538 623 575 6"
    "53 653 653 653 570 623 563 537 593 497 611 648 605 575 307 659 6"
    "21 570 623 611 611 611 563 662 662 662 662 662 575 575 575 575 5"
    "75 537 611 687 687 687 687 687 324 324 629 629 629 537 537 537 5"
    "37 815 815 815 681 681 681 681 653 653 653 653 568 568 621 621 6"
    "21 621 496 496 496 496 496 593 593 593 593 648 648 648 648 648 6"
    "04 604 921 921 571 571 570 538 538 538 542 542 568 621 688 667 5"
    "38 563 479 547 547 555 529 555 525 303 303 809 542 524 309 488 5"
    "48 683 544 430 343 338 637 455 457 438 458 1009 538 824 488 278 "
    "531 552 552 552 552 552 491 488 488 752 494 494 524 531 531 457 "
    "266 1009 494 558 488 488 488 488 278 278 531 531 414 414 552 552"
    " 479 552 694 515 455 488 488 531 531 531 531 504 406 701 408 818"
    " 818 456 430 455 383 488 302 312 547 421 504 317 490 420 253 408"
    " 292 269 488 547 547 547 441 555 555 555 555 555 488 488 488 488"
    " 488 303 303 494 552 552 552 552 552 278 278 278 524 524 524 271"
    " 271 271 271 832 832 832 558 558 558 558 531 531 531 531 556 556"
    " 414 414 414 414 430 430 430 430 430 338 338 338 338 552 552 552"
    " 552 552 504 504 774 774 483 483 504 455 455 455 552 338 774 504"
    " 488 279 303 309 488 358 557 541 455 517 461 454 364 712 640 542"
    " 536 491 492 587 667 542 516 566 491 479 538 560 297 297 558 832"
    " 596 564 720 609 533 476 485 528 528 557 568 557 482 487 487 495"
    " 1053 1144 733 815 875 959 545 297 564 572 572 572 572 572 545 5"
    "45 545 538 538 558 564 564 487 1053 1144 538 889 952 561 596 545"
    " 545 511 511 297 297 564 564 552 552 572 572 487 611 580 523 482"
    " 545 511 564 564 564 564 511 545 492 464 528 448 560 572 535 511"
    " 279 576 552 511 258 413 318 255 545 555 555 555 492 583 583 583"
    " 583 583 511 511 511 511 511 479 538 611 611 611 611 611 297 297"
    " 558 558 558 464 464 464 464 716 716 716 596 596 596 596 564 564"
    " 564 564 525 525 552 552 552 552 455 455 455 455 455 528 528 528"
    " 528 572 572 572 572 572 535 535 830 830 513 513 511 482 482 482"
    " 611 528 830 511 545 469 469 525 552 611 582 482 491 479 417 545"
    " 461 555 559 582 496 465 544 582 504 312 312 278 318 826 558 528"
    " 414 521 268 292 338 586 524 519 504 457 278 0 0 0 0 0 0 0 0 0 0"
    " 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 563 563 563"
    " 563 778 607 789 583 532 438 438 438 553 550 564 492 491 492 491"
    " 525 525 525 479 564 411 711 456 453 521 623 739 784 643 623 752"
    " 781 716 623 647 623 623 623 623 739 784 643 623 752 781 716 896"
    " 896 1012 1058 916 896 1026 1054 989 706 837 834 673 851 831 708"
    " 697 817 949 945 848 785 962 942 872 820 808 687 817 949 945 848"
    " 785 962 942 872 960 1090 1222 1219 1121 1058 1236 1215 1146 455"
    " 586 583 485 422 600 580 510 458 446 324 324 747 874 919 764 888"
    " 916 745 782 666 745 922 902 832 780 768 607 607 761 888 933 792"
    " 777 901 930 865 759 796 679 761 888 933 792 777 901 930 865 953"
    " 1034 1161 1207 1065 1051 1175 1203 1138 574 574 574 574 574 574"
    " 574 574 574 574 574 574 574 574 574 574 574 574 574 574 574 574"
    " 574 574 574 444 444 444 444 444 444 444 444 543 543 543 543 543"
    " 543 543 543 543 543 543 543 543 543 543 543 543 543 543 543 543"
    " 543 543 284 284 284 284 284 284 284 284 284 284 284 284 284 284"
    " 284 284 531 531 531 531 531 531 531 531 532 532 541 541 541 541"
    " 541 541 541 541 541 541 541 541 541 541 541 541 751 751 751 751"
    " 751 751 751 751 751 751 751 751 751 751 751 751 751 751 751 751"
    " 751 751 751 545 545 545 545 545 545 545 545 545 545 545 545 545"
    " 545 545 545 545 545 545 545 545 545 836 836 836 836 836 836 836"
    " 836 836 511 511 511 511 511 511 511 511 611 611 611 611 611 611"
    " 611 611 611 611 611 611 611 611 611 611 611 611 611 611 901 901"
    " 901 901 901 901 901 901 901 297 297 297 297 297 297 297 297 297"
    " 297 297 297 297 297 297 297 564 564 564 564 564 564 564 564 525"
    " 525 525 525 525 525 525 525 525 525 525 525 525 525 525 608 608"
    " 608 608 608 608 608 608 608 608 608 608 608 608 608 608 608 608"
    " 608 608 899 899 899 899 899 899 899 899 899 623 623 739 784 643"
    " 623 752 781 716 914 914 1029 1075 933 914 1043 1071 1006 687 81"
    "7 949 945 848 785 962 942 872 977 1107 1239 1236 1138 1075 1253 "
    "1232 1163 679 761 888 933 792 777 901 930 865 970 1051 1178 1224"
    " 1082 1068 1192 1220 1155 285 285 285 285 285 285 285 285 285 28"
    "5 285 285 285 285 285 285 285 285 273 324 291 692 597 568 534 60"
    "3 945 543 673 679 650 783 688 877 950 699 563 593 570 570 610 85"
    "2 626 626 625 721 721 324 923 667 680 678 688 626 817 623 623 86"
    "6 575 653 653 923 543 533 692 692 653 653 653 573 600 600 600 62"
    "6 534 850 534 564 571 597 857 827 542 934 942 609 718 543 669 59"
    "0 519 556 454 517 728 458 545 568 527 647 592 729 793 542 441 51"
    "1 504 504 496 741 533 534 552 581 581 714 558 559 584 589 531 67"
    "3 271 488 488 752 488 496 491 714 458 457 590 590 531 528 528 47"
    "0 504 504 504 531 447 748 454 476 483 555 771 689 464 757 792 50"
    "0 606 465 549 613 536 525 478 524 840 487 579 606 577 687 611 77"
    "4 816 584 492 528 511 511 543 766 558 561 553 652 652 297 839 57"
    "9 586 606 611 558 716 545 545 765 511 566 566 839 487 487 613 61"
    "3 564 564 564 498 513 513 513 558 478 787 478 506 513 536 757 72"
    "8 490 802 829 531 634 491 577 495 554 556 460 488 673 458 642 52"
    "1 312 544 520 556 545 552 544 284 298 262 570 818 825 556 571 76"
    "5 741 722 407 407 400 409 398 402 524 430 292 312 397 279 303 33"
    "8 774 504 456 455 489 457 363 357 377 458 782 523 520 557 589 26"
    "8 517 452 547 582 363 357 849 853 883 659 526 549 719 832 598 57"
    "5 584 513 677 677 512 707 758 535 477 558 565 495 458 285 276 53"
    "6 452 681 569 542 472 571 570 581 820 514 531 531 499 531 513 49"
    "9 543 584 737 589 507 771 449 453 468 553 447 507 567 495 656 55"
    "9 461 602 448 453 481 419 424 451 495 242 238 460 388 585 493 49"
    "3 479 434 417 450 420 479 669 367 367 412 554 409 413 366 366 32"
    "9 322 371 220 387 607 411 396 347 396 396 414 264 412 380 603 38"
    "4 374 416 399 392 516 372 220 314 412 384 416 399 379 516 372 81"
    "5 552 555 317 856 583 561 428 413 459 358 466 494 432 481 871 31"
    "3 332 593 578 583 547 555 303 657 524 271 832 558 556 414 430 37"
    "8 504 483 455 488 554 559 488 465 458 575 278 461 317 552 445 41"
    "2 333 353 398 328 240 225 407 412 239 220 224 244 257 242 215 33"
    "0 603 603 443 452 413 396 526 326 245 264 431 393 401 387 384 34"
    "3 415 364 339 407 411 411 213 314 310 344 379 574 384 237 393 22"
    "1 221 221 236 236 257 257 554 554 554 554 188 270 270 188 287 27"
    "0 270 274 254 265 265 271 271 271 271 177 354 379 215 326 363 26"
    "2 400 400 400 400 400 498 498 498 498 400 498 498 498 498 498 40"
    "0 498 498 498 498 498 400 498 498 498 498 498 400 498 498 498 49"
    "8 498 400 498 498 498 498 498 498 498 498 498 400 498 498 498 49"
    "8 498 400 498 498 498 498 498 400 498 498 498 498 498 400 498 49"
    "8 498 498 498 400 498 498 498 498 498 498 498 498 498 400 498 49"
    "8 498 498 498 400 498 498 498 498 498 400 498 498 498 498 498 40"
    "0 498 498 498 498 498 400 498 498 498 498 498 498 498 498 498 40"
    "0 498 498 498 498 498 400 498 498 498 498 498 400 498 498 498 49"
    "8 498 400 498 498 498 498 498 400 498 498 498 498 498 498 498 49"
    "8 498 364 364 337 407 375 337 303 242 242 285 270 446 446 326 18"
    "7 302 302 302 302 438 438 438 310 329 301 340 532 532 367 366 39"
    "6 363 366 500]6776[333 250 167 0 0 0 0 200 222 371 221 375 362 3"
    "32 264 661 554 554 554 554 832 554 1253 1019 879 554 554 554 116"
    "6 554 554 554 612 554 554 554 1008 967 928 928 928 928 928 928 9"
    "28 928 928 928 928 928 554 653 653 653 653 653 929 607 603 503 5"
    "54 598 568 754 550 521 417 450 504 484 0 0 0 0 0 0 0 0 0 0 0 0 3"
    "98 0 0 886 834 610 919 854 1129 536 783 661 1115 688 1093 1093 8"
    "86 536 701 709 526 791 641 906 442 719 555]6886[567 747 747 701 "
    "432 796 754 556 823 751 1010 475 700 587 1038 599 831 831 796 47"
    "5 1201 486 322 426 427 481 430 477 424 484 476 1201 1201 1201 12"
    "01 1201 1201 1201 1201 1201 1201 1201 1201 1201 1201 1201 1201 1"
    "201 1201 1201 1201 1201 1202 1202 1202 1203 1202 1202 1202 1202 "
    "1202 1202 1202 1202 1202 1202 1202 1202 1202 1202 1202 1202 1202"
    " 0 0 1098 681 681 747 747 747 747 747 747 715 715 995 995 995 75"
    "1 751 490 490 747 747 747 747 747 747 727 566 468 676 570 596]]>"
    ">\nendobj\n";


  setbuf(stdout, NULL);

  // First open the test PDF file...
  testBegin("pdfioFileOpen(\"testfiles/testpdfio.pdf\")");
  if ((inpdf = pdfioFileOpen("testfiles/testpdfio.pdf", /*password_cb*/NULL, /*password_data*/NULL, (pdfio_error_cb_t)error_cb, &error)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  // TODO: Test for known values in this test file.

  // Test dictionary APIs
  testBegin("pdfioDictCreate");
  if ((dict = pdfioDictCreate(inpdf)) != NULL)
  {
    testEnd(true);

    testBegin("pdfioDictSet*");
    if (pdfioDictSetBoolean(dict, "Boolean", true) && pdfioDictSetName(dict, "Name", "Name") && pdfioDictSetNumber(dict, "Number", 42.0) && pdfioDictSetString(dict, "String", "String"))
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      return (1);
    }

    testBegin("pdfioDictIterateKeys");
    pdfioDictIterateKeys(dict, iterate_cb, &count);
    if (count == 4)
    {
      testEnd(true);
    }
    else
    {
      testEndMessage(false, "got %d, expected 4", count);
      return (1);
    }
  }
  else
  {
    testEnd(false);
    return (1);
  }

  // Test the value parsers for edge cases...
  testBegin("_pdfioValueRead(complex_dict)");
  s = complex_dict;
  _pdfioTokenInit(&tb, inpdf, (_pdfio_tconsume_cb_t)token_consume_cb, (_pdfio_tpeek_cb_t)token_peek_cb, (void *)&s);
  if (_pdfioValueRead(inpdf, NULL, &tb, &value, 0))
  {
    // TODO: Check value...
    testEnd(true);
    _pdfioValueDebug(&value, stdout);
  }
  else
    goto fail;

  // Test the value parsers for edge cases...
  testBegin("_pdfioValueRead(cid_dict)");
  s = cid_dict;
  _pdfioTokenInit(&tb, inpdf, (_pdfio_tconsume_cb_t)token_consume_cb, (_pdfio_tpeek_cb_t)token_peek_cb, (void *)&s);
  if (_pdfioValueRead(inpdf, NULL, &tb, &value, 0))
  {
    // TODO: Check value...
    testEnd(true);
    _pdfioValueDebug(&value, stdout);
  }
  else
    goto fail;

  // Do crypto tests...
  if (do_crypto_tests())
    return (1);

  // Create a new PDF file...
  testBegin("pdfioFileCreate(\"testpdfio-out.pdf\", ...)");
  if ((outpdf = pdfioFileCreate("testpdfio-out.pdf", /*version*/"1.7", /*media_box*/NULL, /*crop_box*/NULL, (pdfio_error_cb_t)error_cb, &error)) != NULL)
    testEnd(true);
  else
    goto fail;

  if (write_unit_file(inpdf, "testpdfio-out.pdf", outpdf, &num_pages, &first_image))
    goto fail;

  if (read_unit_file("testpdfio-out.pdf", num_pages, first_image, false))
    goto fail;

  // Stream a new PDF file...
  if ((outfd = open("testpdfio-out2.pdf", O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0666)) < 0)
  {
    perror("Unable to open \"testpdfio-out2.pdf\"");
    goto fail;
  }

  testBegin("pdfioFileCreateOutput(...)");
  if ((outpdf = pdfioFileCreateOutput((pdfio_output_cb_t)output_cb, &outfd, /*version*/"1.7", /*media_box*/NULL, /*crop_box*/NULL, (pdfio_error_cb_t)error_cb, &error)) != NULL)
    testEnd(true);
  else
    goto fail;

  if (write_unit_file(inpdf, "testpdfio-out2.pdf", outpdf, &num_pages, &first_image))
    goto fail;

  close(outfd);

  if (read_unit_file("testpdfio-out2.pdf", num_pages, first_image, true))
    goto fail;

  // Create new encrypted PDF files...
  testBegin("pdfioFileCreate(\"testpdfio-rc4.pdf\", ...)");
  if ((outpdf = pdfioFileCreate("testpdfio-rc4.pdf", /*version*/"2.0", /*media_box*/NULL, /*crop_box*/NULL, (pdfio_error_cb_t)error_cb, &error)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileSetPermissions(all, RC4-128, no passwords)");
  if (pdfioFileSetPermissions(outpdf, PDFIO_PERMISSION_ALL, PDFIO_ENCRYPTION_RC4_128, NULL, NULL))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  if (write_unit_file(inpdf, "testpdfio-rc4.pdf", outpdf, &num_pages, &first_image))
    return (1);

  if (read_unit_file("testpdfio-rc4.pdf", num_pages, first_image, false))
    return (1);

  // Create new encrypted PDF files...
  testBegin("pdfioFileCreate(\"testpdfio-rc4p.pdf\", ...)");
  if ((outpdf = pdfioFileCreate("testpdfio-rc4p.pdf", /*version*/"1.7", /*media_box*/NULL, /*crop_box*/NULL, (pdfio_error_cb_t)error_cb, &error)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileSetPermissions(no-print, RC4-128, passwords='owner' and 'user')");
  if (pdfioFileSetPermissions(outpdf, PDFIO_PERMISSION_ALL ^ PDFIO_PERMISSION_PRINT, PDFIO_ENCRYPTION_RC4_128, "owner", "user"))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  if (write_unit_file(inpdf, "testpdfio-rc4p.pdf", outpdf, &num_pages, &first_image))
    return (1);

  if (read_unit_file("testpdfio-rc4p.pdf", num_pages, first_image, false))
    return (1);

  testBegin("pdfioFileCreate(\"testpdfio-aes.pdf\", ...)");
  if ((outpdf = pdfioFileCreate("testpdfio-aes.pdf", /*version*/"2.0", /*media_box*/NULL, /*crop_box*/NULL, (pdfio_error_cb_t)error_cb, &error)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileSetPermissions(all, AES-128, no passwords)");
  if (pdfioFileSetPermissions(outpdf, PDFIO_PERMISSION_ALL, PDFIO_ENCRYPTION_AES_128, NULL, NULL))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  if (write_unit_file(inpdf, "testpdfio-aes.pdf", outpdf, &num_pages, &first_image))
    return (1);

  if (read_unit_file("testpdfio-aes.pdf", num_pages, first_image, false))
    return (1);

  testBegin("pdfioFileCreate(\"testpdfio-aesp.pdf\", ...)");
  if ((outpdf = pdfioFileCreate("testpdfio-aesp.pdf", /*version*/"2.0", /*media_box*/NULL, /*crop_box*/NULL, (pdfio_error_cb_t)error_cb, &error)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileSetPermissions(no-print, AES-128, passwords='owner' and 'user')");
  if (pdfioFileSetPermissions(outpdf, PDFIO_PERMISSION_ALL ^ PDFIO_PERMISSION_PRINT, PDFIO_ENCRYPTION_AES_128, "owner", "user"))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  if (write_unit_file(inpdf, "testpdfio-aesp.pdf", outpdf, &num_pages, &first_image))
    return (1);

  if (read_unit_file("testpdfio-aesp.pdf", num_pages, first_image, false))
    return (1);

  testBegin("pdfioFileCreateTemporary");
  if ((outpdf = pdfioFileCreateTemporary(temppdf, sizeof(temppdf), /*version*/"2.0", /*media_box*/NULL, /*crop_box*/NULL, (pdfio_error_cb_t)error_cb, &error)) != NULL)
  {
    testEndMessage(true, "%s", temppdf);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  if (write_unit_file(inpdf, "<temporary>", outpdf, &num_pages, &first_image))
    return (1);

  if (read_unit_file(temppdf, num_pages, first_image, false))
    return (1);

  pdfioFileClose(inpdf);

  // Do PDF/A tests...
  if (do_pdfa_tests())
    return (1);

  return (0);

  fail:

  pdfioFileClose(inpdf);

  return (1);
}


//
// 'draw_image()' - Draw an image with a label.
//

static int				// O - 1 on failure, 0 on success
draw_image(pdfio_stream_t *st,
           const char     *name,	// I - Name
           double         x,		// I - X offset
           double         y,		// I - Y offset
           double         w,		// I - Image width
           double         h,		// I - Image height
           const char     *label)	// I - Label
{
  testBegin("pdfioContentDrawImage(name=\"%s\", x=%g, y=%g, w=%g, h=%g)", name, x, y, w, h);
  if (pdfioContentDrawImage(st, name, x, y, w, h))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentTextBegin()");
  if (pdfioContentTextBegin(st))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentSetTextFont(\"F1\", 18.0)");
  if (pdfioContentSetTextFont(st, "F1", 18.0))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentTextMoveTo(%g, %g)", x, y + h + 9);
  if (pdfioContentTextMoveTo(st, x, y + h + 9))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentTextShow(\"%s\")", label);
  if (pdfioContentTextShow(st, false, label))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentTextEnd()");
  if (pdfioContentTextEnd(st))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  return (0);
}


//
// 'error_cb()' - Display an error message during a unit test.
//

static bool				// O  - `true` to stop, `false` to continue
error_cb(pdfio_file_t *pdf,		// I  - PDF file
         const char   *message,		// I  - Error message
	 bool         *error)		// IO - Have we displayed an error?
{
  (void)pdf;

  *error = true;

  testMessage("%s", message);

  // Continue to catch more errors...
  return (true);
}


//
// 'iterate_cb()' - Test pdfioDictIterateKeys function.
//

static bool				// O - `true` to continue, `false` to stop
iterate_cb(pdfio_dict_t *dict,		// I - Dictionary
           const char   *key,		// I - Key
           void         *cb_data)	// I - Callback data
{
  int	*count = (int *)cb_data;	// Pointer to counter


  if (!dict || !key || !cb_data)
    return (false);

  (*count)++;

  return (true);
}


//
// 'output_cb()' - Write output to a file.
//

static ssize_t				// O - Number of bytes written
output_cb(int        *fd,		// I - File descriptor
          const void *buffer,		// I - Output buffer
          size_t     bytes)		// I - Number of bytes to write
{
  return (write(*fd, buffer, bytes));
}


//
// 'password_cb()' - Password callback for PDF file.
//

static const char *			// O - Password string
password_cb(void       *data,		// I - Callback data
            const char *filename)	// I - Filename (not used)
{
  (void)filename;

  return ((const char *)data);
}


//
// 'read_unit_file()' - Read back a unit test file and confirm its contents.
//

static int				// O - Exit status
read_unit_file(const char *filename,	// I - File to read
               size_t     num_pages,	// I - Expected number of pages
	       size_t     first_image,	// I - First image object
	       bool       is_output)	// I - File written with output callback?
{
  pdfio_file_t	*pdf;			// PDF file
  pdfio_dict_t	*catalog;		// Catalog dictionary
  size_t	i;			// Looping var
  const char	*s;			// String
  bool		error = false;		// Error callback data


  // Open the new PDF file to read it...
  testBegin("pdfioFileOpen(\"%s\", ...)", filename);
  if ((pdf = pdfioFileOpen(filename, password_cb, (void *)"user", (pdfio_error_cb_t)error_cb, &error)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  // Get the root object/catalog dictionary
  testBegin("pdfioFileGetCatalog");
  if ((catalog = pdfioFileGetCatalog(pdf)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEndMessage(false, "got NULL, expected dictionary");
    return (1);
  }

  // Verify some catalog values...
  testBegin("pdfioDictGetName(PageLayout)");
  if ((s = pdfioDictGetName(catalog, "PageLayout")) != NULL && !strcmp(s, "SinglePage"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'SinglePage'", s);
    return (1);
  }
  else
  {
    testEndMessage(false, "got NULL, expected 'SinglePage'");
    return (1);
  }

  testBegin("pdfioDictGetName(PageLayout)");
  if ((s = pdfioDictGetName(catalog, "PageLayout")) != NULL && !strcmp(s, "SinglePage"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'SinglePage'", s);
    return (1);
  }
  else
  {
    testEndMessage(false, "got NULL, expected 'SinglePage'");
    return (1);
  }

  testBegin("pdfioDictGetName(PageMode)");
  if ((s = pdfioDictGetName(catalog, "PageMode")) != NULL && !strcmp(s, "UseThumbs"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'UseThumbs'", s);
    return (1);
  }
  else
  {
    testEndMessage(false, "got NULL, expected 'UseThumbs'");
    return (1);
  }

  testBegin("pdfioDictGetString(Lang)");
  if ((s = pdfioDictGetString(catalog, "Lang")) != NULL && !strcmp(s, "en-CA"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'en-CA'", s);
    return (1);
  }
  else
  {
    testEndMessage(false, "got NULL, expected 'en-CA'");
    return (1);
  }

  // Verify metadata...
  testBegin("pdfioFileGetAuthor");
  if ((s = pdfioFileGetAuthor(pdf)) != NULL && !strcmp(s, "Michael R Sweet"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'Michael R Sweet'", s);
    return (1);
  }
  else if (strcmp(pdfioFileGetVersion(pdf), "2.0"))
  {
    testEndMessage(false, "got NULL, expected 'Michael R Sweet'");
    return (1);
  }
  else
  {
    testEnd(true);
  }

  testBegin("pdfioFileGetCreator");
  if ((s = pdfioFileGetCreator(pdf)) != NULL && !strcmp(s, "testpdfio"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'testpdfio'", s);
    return (1);
  }
  else if (strcmp(pdfioFileGetVersion(pdf), "2.0"))
  {
    testEndMessage(false, "got NULL, expected 'testpdfio'");
    return (1);
  }
  else
  {
    testEnd(true);
  }

  testBegin("pdfioFileGetKeywords");
  if ((s = pdfioFileGetKeywords(pdf)) != NULL && !strcmp(s, "one fish,two fish,red fish,blue fish"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'one fish,two fish,red fish,blue fish'", s);
    return (1);
  }
  else if (strcmp(pdfioFileGetVersion(pdf), "2.0"))
  {
    testEndMessage(false, "got NULL, expected 'one fish,two fish,red fish,blue fish'");
    return (1);
  }
  else
  {
    testEnd(true);
  }

  testBegin("pdfioFileGetSubject");
  if ((s = pdfioFileGetSubject(pdf)) != NULL && !strcmp(s, "Unit test document"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'Unit test document'", s);
    return (1);
  }
  else if (strcmp(pdfioFileGetVersion(pdf), "2.0"))
  {
    testEndMessage(false, "got NULL, expected 'Unit test document'");
    return (1);
  }
  else
  {
    testEnd(true);
  }

  testBegin("pdfioFileGetTitle");
  if ((s = pdfioFileGetTitle(pdf)) != NULL && !strcmp(s, "Test Document"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'Test Document'", s);
    return (1);
  }
  else if (strcmp(pdfioFileGetVersion(pdf), "2.0"))
  {
    testEndMessage(false, "got NULL, expected 'Test Document'");
    return (1);
  }
  else
  {
    testEnd(true);
  }

  // Verify the number of pages is the same...
  testBegin("pdfioFileGetNumPages");
  if (num_pages == pdfioFileGetNumPages(pdf))
  {
    testEnd(true);
  }
  else
  {
    testEndMessage(false, "%lu != %lu", (unsigned long)num_pages, (unsigned long)pdfioFileGetNumPages(pdf));
    return (1);
  }

  // Verify the images
  for (i = 0; i < 7; i ++)
  {
    if (is_output)
    {
      if (verify_image(pdf, first_image + (size_t)i * 2))
        return (1);
    }
    else if (verify_image(pdf, first_image + (size_t)i))
      return (1);
  }

  // Close the new PDF file...
  testBegin("pdfioFileClose(\"testpdfio-out.pdf\")");
  if (pdfioFileClose(pdf))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  return (0);
}


//
// 'token_consume_cb()' - Consume bytes from a test string.
//

static ssize_t				// O  - Number of bytes consumed
token_consume_cb(const char **s,	// IO - Test string
                 size_t     bytes)	// I  - Bytes to consume
{
  size_t	len = strlen(*s);	// Number of bytes remaining


  // "Consume" bytes by incrementing the string pointer, limiting to the
  // remaining length...
  if (bytes > len)
    bytes = len;

  *s += bytes;

  return ((ssize_t)bytes);
}


//
// 'token_peek_cb()' - Peek bytes from a test string.
//

static ssize_t				// O  - Number of bytes peeked
token_peek_cb(const char **s,		// IO - Test string
              char       *buffer,	// I  - Buffer
              size_t     bytes)		// I  - Bytes to peek
{
  size_t	len = strlen(*s);	// Number of bytes remaining


  // Copy bytes from the test string as possible...
  if (bytes > len)
    bytes = len;

  if (bytes > 0)
    memcpy(buffer, *s, bytes);

  return ((ssize_t)bytes);
}


//
// 'usage()' - Show program usage.
//

static int				// O - Exit status
usage(FILE *fp)				// I - Output file
{
  fputs("Usage: ./testpdfio [OPTIONS] [FILENAME {OBJECT-NUM,OUT-FILENAME}] ...\n", fp);
  fputs("Options:\n", fp);
  fputs("  --help               Show program help.\n", fp);
  fputs("  --password PASSWORD  Set PDF password.\n", fp);
  fputs("  --verbose            Be verbose.\n", fp);

  return (fp != stdout);
}


//
// 'verify_image()' - Verify an image object.
//

static int				// O - 1 on failure, 0 on success
verify_image(pdfio_file_t *pdf,		// I - PDF file
             size_t       number)	// I - Object number
{
  pdfio_obj_t	*obj;			// Image object
  const char	*type,			// Object type
		*subtype;		// Object subtype
  double	width,			// Width of object
		height;			// Height of object
  pdfio_stream_t *st;			// Stream
  int		x, y;			// Coordinates in image
  unsigned char	buffer[768],		// Expected data
		*bufptr,		// Pointer into buffer
		line[768];		// Line from file
  ssize_t	bytes;			// Bytes read from stream


  testBegin("pdfioFileFindObj(%lu)", (unsigned long)number);
  if ((obj = pdfioFileFindObj(pdf, number)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioObjGetType");
  if ((type = pdfioObjGetType(obj)) != NULL && !strcmp(type, "XObject"))
  {
    testEnd(true);
  }
  else
  {
    testEndMessage(false, "got %s, expected XObject", type);
    return (1);
  }

  testBegin("pdfioObjGetSubtype");
  if ((subtype = pdfioObjGetSubtype(obj)) != NULL && !strcmp(subtype, "Image"))
  {
    testEnd(true);
  }
  else
  {
    testEndMessage(false, "got %s, expected Image", subtype);
    return (1);
  }

  testBegin("pdfioImageGetWidth");
  if ((width = pdfioImageGetWidth(obj)) == 256.0)
  {
    testEnd(true);
  }
  else
  {
    testEndMessage(false, "got %g, expected 256", width);
    return (1);
  }

  testBegin("pdfioImageGetHeight");
  if ((height = pdfioImageGetHeight(obj)) == 256.0)
  {
    testEnd(true);
  }
  else
  {
    testEndMessage(false, "got %g, expected 256", height);
    return (1);
  }

  // Open the image stream, read the image, and verify it matches expectations...
  testBegin("pdfioObjOpenStream");
  if ((st = pdfioObjOpenStream(obj, PDFIO_FILTER_FLATE)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioStreamRead");

  for (y = 0; y < 256; y ++)
  {
    for (x = 0, bufptr = buffer; x < 256; x ++, bufptr += 3)
    {
      bufptr[0] = (unsigned char)y;
      bufptr[1] = (unsigned char)(y + x);
      bufptr[2] = (unsigned char)(y - x);
    }

    if ((bytes = pdfioStreamRead(st, line, sizeof(line))) != (ssize_t)sizeof(line))
    {
      testEndMessage(false, "got %d for line %d, expected 768", y, (int)bytes);
      pdfioStreamClose(st);
      return (1);
    }

    if (memcmp(buffer, line, sizeof(buffer)))
    {
      testEndMessage(false, "line %d doesn't match expectations", y);
      pdfioStreamClose(st);
      return (1);
    }
  }

  pdfioStreamClose(st);

  testEnd(true);

  return (0);
}


//
// 'write_alpha_test()' - Write a series of test images with alpha channels.
//

static int				// O - 1 on failure, 0 on success
write_alpha_test(
    pdfio_file_t *pdf,			// I - PDF file
    int          number,		// I - Page number
    pdfio_obj_t  *font)			// I - Text font
{
  pdfio_dict_t	*dict;			// Page dictionary
  pdfio_stream_t *st;			// Page stream
  pdfio_obj_t	*images[6];		// Images using PNG predictors
  char		iname[32];		// Image name
  int		i,			// Image number
		x, y;			// Coordinates in image
  unsigned char	buffer[1280 * 256],	// Buffer for image
		*bufptr;		// Pointer into buffer


  // Create the images...
  for (i = 0; i < 6; i ++)
  {
    size_t	num_colors = 0;		// Number of colors

    // Generate test image data...
    switch (i)
    {
      case 0 : // Grayscale
      case 3 : // Grayscale + alpha
          num_colors = 1;
	  for (y = 0, bufptr = buffer; y < 256; y ++)
	  {
	    for (x = 0; x < 256; x ++)
	    {
	      unsigned char r = (unsigned char)y;
	      unsigned char g = (unsigned char)(y + x);
	      unsigned char b = (unsigned char)(y - x);

              *bufptr++ = (unsigned char)((r * 30 + g * 59 + b * 11) / 100);

	      if (i > 2)
	      {
	        // Add alpha channel
	        if (x < 112 || x >= 144 || y < 112 || y >= 144)
		  *bufptr++ = (unsigned char)((x - 128) * (y - 128));
		else
		  *bufptr++ = 0;
	      }
	    }
	  }
          break;

      case 1 : // RGB
      case 4 : // RGB + alpha
          num_colors = 3;
	  for (y = 0, bufptr = buffer; y < 256; y ++)
	  {
	    for (x = 0; x < 256; x ++)
	    {
	      *bufptr++ = (unsigned char)y;
	      *bufptr++ = (unsigned char)(y + x);
	      *bufptr++ = (unsigned char)(y - x);

	      if (i > 2)
	      {
	        // Add alpha channel
	        if (x < 112 || x >= 144 || y < 112 || y >= 144)
		  *bufptr++ = (unsigned char)((x - 128) * (y - 128));
		else
		  *bufptr++ = 0;
	      }
	    }
	  }
          break;
      case 2 : // CMYK
      case 5 : // CMYK + alpha
          num_colors = 4;
	  for (y = 0, bufptr = buffer; y < 256; y ++)
	  {
	    for (x = 0; x < 256; x ++)
	    {
	      unsigned char cc = (unsigned char)y;
	      unsigned char mm = (unsigned char)(y + x);
	      unsigned char yy = (unsigned char)(y - x);
	      unsigned char kk = cc < mm ? cc < yy ? cc : yy : mm < yy ? mm : yy;

              *bufptr++ = (unsigned char)(cc - kk);
              *bufptr++ = (unsigned char)(mm - kk);
              *bufptr++ = (unsigned char)(yy - kk);
              *bufptr++ = (unsigned char)(kk);

	      if (i > 2)
	      {
	        // Add alpha channel
	        if (x < 112 || x >= 144 || y < 112 || y >= 144)
		  *bufptr++ = (unsigned char)((x - 128) * (y - 128));
		else
		  *bufptr++ = 0;
	      }
	    }
	  }
          break;
    }

    // Write the image...
    testBegin("pdfioFileCreateImageObjFromData(num_colors=%u, alpha=%s)", (unsigned)num_colors, i > 2 ? "true" : "false");
    if ((images[i] = pdfioFileCreateImageObjFromData(pdf, buffer, 256, 256, num_colors, NULL, i > 2, false)) != NULL)
    {
      testEndMessage(true, "%u", (unsigned)pdfioObjGetNumber(images[i]));
    }
    else
    {
      testEnd(false);
      return (1);
    }
  }

  // Create the page dictionary, object, and stream...
  testBegin("pdfioDictCreate");
  if ((dict = pdfioDictCreate(pdf)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  for (i = 0; i < 6; i ++)
  {
    testBegin("pdfioPageDictAddImage(%d)", i + 1);
    snprintf(iname, sizeof(iname), "IM%d", i + 1);
    if (pdfioPageDictAddImage(dict, pdfioStringCreate(pdf, iname), images[i]))
      testEnd(true);
    else
      return (1);
  }

  testBegin("pdfioPageDictAddFont(F1)");
  if (pdfioPageDictAddFont(dict, "F1", font))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileCreatePage(%d)", number);

  if ((st = pdfioFileCreatePage(pdf, dict)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  if (write_header_footer(st, "Image Writing Test", number))
    goto error;

  // Draw images
  for (i = 0; i < 6; i ++)
  {
    static const char *labels[] = {
      "DeviceGray",
      "DeviceRGB",
      "DeviceCMYK",
      "DevGray + Alpha",
      "DevRGB + Alpha",
      "DevCMYK + Alpha"
    };

    snprintf(iname, sizeof(iname), "IM%d", i + 1);

    if (draw_image(st, iname, 36 + 180 * (i % 3), 306 - 216 * (i / 3), 144, 144, labels[i]))
      goto error;
  }

  // Wrap up...
  testBegin("pdfioStreamClose");
  if (pdfioStreamClose(st))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  return (0);

  error:

  pdfioStreamClose(st);
  return (1);
}


//
// 'write_color_patch()' - Write a color patch...
//

static int				// O - 1 on failure, 0 on success
write_color_patch(pdfio_stream_t *st,	// I - Content stream
                  bool           device)// I - Use device color?
{
  int		col,			// Current column
		row;			// Current row
  double	x, y,			// Relative position
		r,			// Radius
		hue,			// Hue angle
		sat,			// Saturation
		cc,			// Computed color
		red,			// Red value
		green,			// Green value
		blue;			// Blue value


  // Draw an 11x11 patch...
  for (col = 0; col < 21; col ++)
  {
    for (row = 0; row < 21; row ++)
    {
      // Compute color in patch...
      x = 0.1 * (col - 10);
      y = 0.1 * (row - 10);
      r = sqrt(x * x + y * y);

      if (r == 0.0)
      {
        red = green = blue = 1.0;
      }
      else
      {
	sat = pow(r, 1.5);

        x   /= r;
        y   /= r;
	hue = 3.0 * atan2(y, x) / M_PI;
	if (hue < 0.0)
	  hue += 6.0;

	cc = sat * (1.0 - fabs(fmod(hue, 2.0) - 1.0)) + 1.0 - sat;

	if (hue < 1.0)
	{
	  red   = 1.0;
	  green = cc;
	  blue  = 1.0 - sat;
	}
	else if (hue < 2.0)
	{
	  red   = cc;
	  green = 1.0;
	  blue  = 1.0 - sat;
	}
	else if (hue < 3.0)
	{
	  red   = 1.0 - sat;
	  green = 1.0;
	  blue  = cc;
	}
	else if (hue < 4.0)
	{
	  red   = 1.0 - sat;
	  green = cc;
	  blue  = 1.0;
	}
	else if (hue < 5.0)
	{
	  red   = cc;
	  green = 1.0 - sat;
	  blue  = 1.0;
	}
	else
	{
	  red   = 1.0;
	  green = 1.0 - sat;
	  blue  = cc;
	}
      }

      // Draw it...
      if (device)
      {
        // Use device CMYK color instead of a calibrated RGB space...
        double cyan = 1.0 - red;	// Cyan color
        double magenta = 1.0 - green;	// Magenta color
        double yellow = 1.0 - blue;	// Yellow color
        double black = cyan;		// Black color

        if (black > magenta)
          black = magenta;
        if (black > yellow)
          black = yellow;

        cyan    -= black;
        magenta -= black;
        yellow  -= black;

	testBegin("pdfioContentSetFillColorDeviceCMYK(c=%g, m=%g, y=%g, k=%g)", cyan, magenta, yellow, black);
	if (pdfioContentSetFillColorDeviceCMYK(st, cyan, magenta, yellow, black))
	{
	  testEnd(true);
	}
	else
	{
	  testEnd(false);
	  return (1);
	}
      }
      else
      {
        // Use calibrate RGB space...
	testBegin("pdfioContentSetFillColorRGB(r=%g, g=%g, b=%g)", red, green, blue);
	if (pdfioContentSetFillColorRGB(st, red, green, blue))
	{
	  testEnd(true);
	}
	else
	{
	  testEnd(false);
	  return (1);
	}
      }

      testBegin("pdfioContentPathRect(x=%g, y=%g, w=%g, h=%g)", col * 6.0, row * 6.0, 6.0, 6.0);
      if (pdfioContentPathRect(st, col * 6.0, row * 6.0, 6.0, 6.0))
      {
	testEnd(true);
      }
      else
      {
	testEnd(false);
	return (1);
      }

      testBegin("pdfioContentFill(even_odd=false)");
      if (pdfioContentFill(st, false))
      {
	testEnd(true);
      }
      else
      {
	testEnd(false);
	return (1);
      }
    }
  }

  return (0);
}


//
// 'write_color_test()' - Write a color test page...
//

static int				// O - 1 on failure, 0 on success
write_color_test(pdfio_file_t *pdf,	// I - PDF file
                 int          number,	// I - Page number
                 pdfio_obj_t  *font)	// I - Text font
{
  pdfio_dict_t		*dict;		// Page dictionary
  pdfio_stream_t	*st;		// Page contents stream
  pdfio_array_t		*cs;		// Color space array
  pdfio_obj_t		*prophoto;	// ProPhotoRGB ICC profile object


  testBegin("pdfioFileCreateICCObjFromFile(ProPhotoRGB)");
  if ((prophoto = pdfioFileCreateICCObjFromFile(pdf, "testfiles/iso22028-2-romm-rgb.icc", 3)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioDictCreate");
  if ((dict = pdfioDictCreate(pdf)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioArrayCreateColorFromStandard(AdobeRGB)");
  if ((cs = pdfioArrayCreateColorFromStandard(pdf, 3, PDFIO_CS_ADOBE)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddColorSpace(AdobeRGB)");
  if (pdfioPageDictAddColorSpace(dict, "AdobeRGB", cs))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioArrayCreateColorFromStandard(DisplayP3)");
  if ((cs = pdfioArrayCreateColorFromStandard(pdf, 3, PDFIO_CS_P3_D65)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddColorSpace(DisplayP3)");
  if (pdfioPageDictAddColorSpace(dict, "DisplayP3", cs))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioArrayCreateColorFromICCObj(ProPhotoRGB)");
  if ((cs = pdfioArrayCreateColorFromICCObj(pdf, prophoto)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddColorSpace(ProPhotoRGB)");
  if (pdfioPageDictAddColorSpace(dict, "ProPhotoRGB", cs))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioArrayCreateColorFromStandard(sRGB)");
  if ((cs = pdfioArrayCreateColorFromStandard(pdf, 3, PDFIO_CS_SRGB)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddColorSpace(sRGB)");
  if (pdfioPageDictAddColorSpace(dict, "sRGB", cs))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddFont(F1)");
  if (pdfioPageDictAddFont(dict, "F1", font))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileCreatePage(%d)", number);

  if ((st = pdfioFileCreatePage(pdf, dict)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  if (write_header_footer(st, "Color Space Test", number))
    goto error;

  testBegin("pdfioContentTextBegin()");
  if (pdfioContentTextBegin(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentSetTextFont(\"F1\", 18.0)");
  if (pdfioContentSetTextFont(st, "F1", 18.0))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextMoveTo(82, 234)");
  if (pdfioContentTextMoveTo(st, 82, 234))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextShow(\"AdobeRGB\")");
  if (pdfioContentTextShow(st, false, "AdobeRGB"))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextMoveTo(234, 0)");
  if (pdfioContentTextMoveTo(st, 234, 0))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextShow(\"DisplayP3\")");
  if (pdfioContentTextShow(st, false, "DisplayP3"))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextMoveTo(-234, 216)");
  if (pdfioContentTextMoveTo(st, -234, 216))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextShow(\"sRGB\")");
  if (pdfioContentTextShow(st, false, "sRGB"))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextMoveTo(234, 0)");
  if (pdfioContentTextMoveTo(st, 234, 0))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextShow(\"ProPhotoRGB\")");
  if (pdfioContentTextShow(st, false, "ProPhotoRGB"))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextMoveTo(-234, 216)");
  if (pdfioContentTextMoveTo(st, -234, 216))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextShow(\"DeviceCMYK\")");
  if (pdfioContentTextShow(st, false, "DeviceCMYK"))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextEnd()");
  if (pdfioContentTextEnd(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentSave()");
  if (pdfioContentSave(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentSetFillColorSpace(AdobeRGB)");
  if (pdfioContentSetFillColorSpace(st, "AdobeRGB"))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentMatrixTranslate(82, 90)");
  if (pdfioContentMatrixTranslate(st, 82, 90))
    testEnd(true);
  else
    goto error;

  if (write_color_patch(st, false))
    goto error;

  testBegin("pdfioContentRestore()");
  if (pdfioContentRestore(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentSave()");
  if (pdfioContentSave(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentSetFillColorSpace(DisplayP3)");
  if (pdfioContentSetFillColorSpace(st, "DisplayP3"))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentMatrixTranslate(316, 90)");
  if (pdfioContentMatrixTranslate(st, 316, 90))
    testEnd(true);
  else
    goto error;

  if (write_color_patch(st, false))
    goto error;

  testBegin("pdfioContentRestore()");
  if (pdfioContentRestore(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentSave()");
  if (pdfioContentSave(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentSetFillColorSpace(sRGB)");
  if (pdfioContentSetFillColorSpace(st, "sRGB"))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentMatrixTranslate(82, 306)");
  if (pdfioContentMatrixTranslate(st, 82, 306))
    testEnd(true);
  else
    goto error;

  if (write_color_patch(st, false))
    goto error;

  testBegin("pdfioContentRestore()");
  if (pdfioContentRestore(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentSave()");
  if (pdfioContentSave(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentSetFillColorSpace(ProPhotoRGB)");
  if (pdfioContentSetFillColorSpace(st, "ProPhotoRGB"))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentMatrixTranslate(316, 306)");
  if (pdfioContentMatrixTranslate(st, 316, 306))
    testEnd(true);
  else
    goto error;

  if (write_color_patch(st, false))
    goto error;

  testBegin("pdfioContentRestore()");
  if (pdfioContentRestore(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentSave()");
  if (pdfioContentSave(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentMatrixTranslate(82, 522)");
  if (pdfioContentMatrixTranslate(st, 82, 522))
    testEnd(true);
  else
    goto error;

  if (write_color_patch(st, true))
    goto error;

  testBegin("pdfioContentRestore()");
  if (pdfioContentRestore(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioStreamClose");
  if (pdfioStreamClose(st))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  return (0);

  error:

  pdfioStreamClose(st);
  return (1);
}


//
// 'write_font_test()' - Write a font test page.
//

static int				// O - 1 on failure, 0 on success
write_font_test(
    pdfio_file_t *pdf,			// I - PDF file
    int          number,		// I - Page number
    pdfio_obj_t  *font,			// I - Page number font
    const char   *textfontfile,		// I - Text font file
    bool         unicode)		// I - Use Unicode font?
{
  pdfio_dict_t		*dict;		// Page dictionary
  pdfio_stream_t	*st;		// Page contents stream
  pdfio_obj_t		*textfont;	// Text font
  char			title[256],	// Page title
			textname[256],	// Name of text font
			*ptr;		// Pointer into name
  double		width;		// Text width
  int			i;		// Looping var
  static const char * const welcomes[] =// "Welcome" in many languages
  {
    "Welcome",
    "Welkom",
    "ḫaṣānu",
    "Mayad-ayad nga pad-abot",
    "Mir se vjên",
    "Mirë se vjen",
    "Wellkumma",
    "Bienveniu",
    "Ghini vinit!",
    "Bienveníu",
    "Miro peicak",
    "Xoş gəlmişsiniz!",
    "Salamat datang",
    "Сәләм бирем!",
    "Menjuah-juah!",
    "Še das d' kemma bisd",
    "Mwaiseni",
    "Maogmáng Pag-abót",
    "Welkam",
    "Dobrodošli",
    "Degemer mat",
    "Benvingut",
    "Maayong pag-abot",
    "Kopisanangan do kinorikatan",
    "Bienvenida",
    "Bien binidu",
    "Bienbenidu",
    "Hóʔą",
    "Boolkhent!",
    "Kopivosian do kinoikatan",
    "Malipayeng Pag-abot!",
    "Vítej",
    "Velkommen",
    "Salâm",
    "Welkom",
    "Emedi",
    "Welkumin",
    "Tere tulemast",
    "Woé zɔ",
    "Bienveníu",
    "Vælkomin",
    "Bula",
    "Tervetuloa",
    "Bienvenue",
    "Wäljkiimen",
    "Wäilkuumen",
    "Wäilkuumen",
    "Wolkom",
    "Benvignût",
    "Benvido",
    "Willkommen",
    "Ἀσπάζομαι!",
    "Καλώς Ήρθες",
    "Tikilluarit",
    "Byen venu",
    "Sannu da zuwa",
    "Aloha",
    "Wayakurua",
    "Dayón",
    "Zoo siab txais tos!",
    "Üdvözlet",
    "Selamat datai",
    "Velkomin",
    "Nnọọ",
    "Selamat datang",
    "Qaimarutin",
    "Fáilte",
    "Benvenuto",
    "Voschata",
    "Murakaza neza",
    "Mauri",
    "Tu be xér hatî ye!",
    "Taŋyáŋ yahí",
    "Salve",
    "Laipni lūdzam",
    "Wilkóm",
    "Sveiki atvykę",
    "Willkamen",
    "Mu amuhezwi",
    "Tukusanyukidde",
    "Wëllkomm",
    "Swagatam",
    "Tonga soa",
    "Selamat datang",
    "Merħba",
    "B’a’ntulena",
    "Failt ort",
    "Haere mai",
    "mai",
    "Pjila’si",
    "Benvegnüu",
    "Ne y kena",
    "Ximopanōltih",
    "Yá'át'ééh",
    "Siyalemukela",
    "Siyalemukela",
    "Bures boahtin",
    "Re a go amogela",
    "Velkommen",
    "Benvengut!",
    "Bon bini",
    "Witam Cię",
    "Bem-vindo",
    "Haykuykuy!",
    "T'aves baxtalo",
    "Bainvegni",
    "Afio mai",
    "Ennidos",
    "Walcome",
    "Fàilte",
    "Mauya",
    "Bon vinutu",
    "Vitaj",
    "Dobrodošli",
    "Soo dhowow",
    "Witaj",
    "Bienvenido",
    "Wilujeng sumping",
    "Karibu",
    "Wamukelekile",
    "Välkommen",
    "Wilkomme",
    "Maligayang pagdating",
    "Maeva",
    "Räxim itegez",
    "Ksolok Bodik Mai",
    "Ulu tons mai",
    "Welkam",
    "Talitali fiefia",
    "Lek oy li la tale",
    "amogetswe",
    "Tempokani",
    "Hoş geldin",
    "Koş geldiniz",
    "Ulufale mai!",
    "Xush kelibsiz",
    "Benvignùo",
    "Tervhen tuldes",
    "Hoan nghênh",
    "Tere tulõmast",
    "Benvnuwe",
    "Croeso",
    "Merhbe",
    "Wamkelekile",
    "Märr-ŋamathirri",
    "Ẹ ku abọ",
    "Kíimak 'oolal",
    "Ngiyakwemukela",
    "いらっしゃいませ"
  };


  testBegin("pdfioFileCreateFontObjFromFile(%s)", textfontfile);
  if ((textfont = pdfioFileCreateFontObjFromFile(pdf, textfontfile, unicode)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioDictCreate");
  if ((dict = pdfioDictCreate(pdf)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddFont(F1)");
  if (pdfioPageDictAddFont(dict, "F1", font))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddFont(F2)");
  if (pdfioPageDictAddFont(dict, "F2", textfont))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileCreatePage(%d)", number);

  if ((st = pdfioFileCreatePage(pdf, dict)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  if ((ptr = strrchr(textfontfile, '/')) != NULL)
    strncpy(textname, ptr + 1, sizeof(textname) - 1);
  else
    strncpy(textname, textfontfile, sizeof(textname) - 1);

  textname[sizeof(textname) - 1] = '\0';
  if ((ptr = strrchr(textname, '.')) != NULL)
    *ptr = '\0';

  if (unicode)
    snprintf(title, sizeof(title), "Unicode %s Font Test", textname);
  else
    snprintf(title, sizeof(title), "CP1252 %s Font Test", textname);

  if (write_header_footer(st, title, number))
    goto error;

  testBegin("pdfioContentTextBegin()");
  if (pdfioContentTextBegin(st))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentSetTextFont(\"F2\", 10.0)");
  if (pdfioContentSetTextFont(st, "F2", 10.0))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentSetTextLeading(12.0)");
  if (pdfioContentSetTextLeading(st, 12.0))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextMoveTo(198.0, 702.0)");
  if (pdfioContentTextMoveTo(st, 198.0, 702.0))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  for (i = 0; i < (int)(sizeof(welcomes) / sizeof(welcomes[0])); i ++)
  {
    if (i > 0 && (i % 50) == 0)
    {
      testBegin("pdfioContentTextMoveTo(162.0, 600.0)");
      if (pdfioContentTextMoveTo(st, 162.0, 600.0))
      {
	testEnd(true);
      }
      else
      {
	testEnd(false);
	return (1);
      }
    }

    testBegin("pdfioContentTextMeasure(\"%s\")", welcomes[i]);
    if ((width = pdfioContentTextMeasure(textfont, welcomes[i], 10.0)) >= 0.0)
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      return (1);
    }

    testBegin("pdfioContextTextMoveTo(%g, 0.0)", -width);
    if (pdfioContentTextMoveTo(st, -width, 0.0))
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      return (1);
    }

    testBegin("pdfioContentTextShowf(\"%s\")", welcomes[i]);
    if (pdfioContentTextShowf(st, unicode, "%s\n", welcomes[i]))
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      return (1);
    }

    testBegin("pdfioContextTextMoveTo(%g, 0.0)", width);
    if (pdfioContentTextMoveTo(st, width, 0.0))
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      return (1);
    }
  }

  testBegin("pdfioContentTextEnd()");
  if (pdfioContentTextEnd(st))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioStreamClose");
  if (pdfioStreamClose(st))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  return (0);

  error:

  pdfioStreamClose(st);
  return (1);
}


//
// 'write_header_footer()' - Write common header and footer text.
//

static int				// O - 1 on failure, 0 on success
write_header_footer(
    pdfio_stream_t *st,			// I - Page content stream
    const char     *title,		// I - Title
    int            number)		// I - Page number
{
  testBegin("pdfioContentSetFillColorDeviceGray(0.0)");
  if (pdfioContentSetFillColorDeviceGray(st, 0.0))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentTextBegin()");
  if (pdfioContentTextBegin(st))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentSetTextFont(\"F1\", 18.0)");
  if (pdfioContentSetTextFont(st, "F1", 18.0))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentTextMoveTo(36.0, 738.0)");
  if (pdfioContentTextMoveTo(st, 36.0, 738.0))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentTextShow(\"%s\")", title);
  if (pdfioContentTextShow(st, false, title))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentSetTextFont(\"F1\", 12.0)");
  if (pdfioContentSetTextFont(st, "F1", 12.0))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentTextMoveTo(514.0, -702.0)");
  if (pdfioContentTextMoveTo(st, 514.0, -702.0))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentTextShowf(\"%d\")", number);
  if (pdfioContentTextShowf(st, false, "%d", number))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioContentTextEnd()");
  if (pdfioContentTextEnd(st))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  return (0);
}


//
// 'write_image_object()' - Write an image object using the specified predictor.
//

static pdfio_obj_t *			// O - Image object
write_image_object(
    pdfio_file_t       *pdf,		// I - PDF file
    _pdfio_predictor_t predictor)	// I - Predictor to use
{
  pdfio_dict_t	*dict,			// Image dictionary
		*decode;		// Decode dictionary
  pdfio_obj_t	*obj;			// Image object
  pdfio_stream_t *st;			// Image stream
  int		x, y;			// Coordinates in image
  unsigned char	buffer[768],		// Buffer for image
		*bufptr;		// Pointer into buffer


  // Create the image dictionary...
  if ((dict = pdfioDictCreate(pdf)) == NULL)
    return (NULL);

  pdfioDictSetName(dict, "Type", "XObject");
  pdfioDictSetName(dict, "Subtype", "Image");
  pdfioDictSetNumber(dict, "Width", 256);
  pdfioDictSetNumber(dict, "Height", 256);
  pdfioDictSetNumber(dict, "BitsPerComponent", 8);
  pdfioDictSetName(dict, "ColorSpace", "DeviceRGB");
  pdfioDictSetName(dict, "Filter", "FlateDecode");

  // DecodeParms dictionary...
  if ((decode = pdfioDictCreate(pdf)) == NULL)
    return (NULL);

  pdfioDictSetNumber(decode, "BitsPerComponent", 8);
  pdfioDictSetNumber(decode, "Colors", 3);
  pdfioDictSetNumber(decode, "Columns", 256);
  pdfioDictSetNumber(decode, "Predictor", predictor);
  pdfioDictSetDict(dict, "DecodeParms", decode);

  // Create the image object...
  if ((obj = pdfioFileCreateObj(pdf, dict)) == NULL)
    return (NULL);

  // Create the image stream and write the image...
  if ((st = pdfioObjCreateStream(obj, PDFIO_FILTER_FLATE)) == NULL)
    return (NULL);

  // This creates a useful criss-cross image that highlights predictor errors...
  for (y = 0; y < 256; y ++)
  {
    for (x = 0, bufptr = buffer; x < 256; x ++, bufptr += 3)
    {
      bufptr[0] = (unsigned char)y;
      bufptr[1] = (unsigned char)(y + x);
      bufptr[2] = (unsigned char)(y - x);
    }

    if (!pdfioStreamWrite(st, buffer, sizeof(buffer)))
    {
      pdfioStreamClose(st);
      return (NULL);
    }
  }

  // Close the object and stream...
  pdfioStreamClose(st);

  return (obj);
}


//
// 'write_images_test()' - Write a series of test images.
//

static int				// O - 1 on failure, 0 on success
write_images_test(
    pdfio_file_t *pdf,			// I - PDF file
    int          number,		// I - Page number
    pdfio_obj_t  *font)			// I - Text font
{
  pdfio_dict_t	*dict;			// Page dictionary
  pdfio_stream_t *st;			// Page stream
  _pdfio_predictor_t p;			// Current predictor
  pdfio_obj_t	*noimage,		// No predictor
		*pimages[6];		// Images using PNG predictors
  char		pname[32],		// Image name
		plabel[32];		// Image label


  // Create the images...
  testBegin("Create Image (Predictor 1)");
  if ((noimage = write_image_object(pdf, _PDFIO_PREDICTOR_NONE)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  for (p = _PDFIO_PREDICTOR_PNG_NONE; p <= _PDFIO_PREDICTOR_PNG_AUTO; p ++)
  {
    testBegin("Create Image (Predictor %d)", p);
    if ((pimages[p - _PDFIO_PREDICTOR_PNG_NONE] = write_image_object(pdf, p)) != NULL)
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      return (1);
    }
  }

  // Create the page dictionary, object, and stream...
  testBegin("pdfioDictCreate");
  if ((dict = pdfioDictCreate(pdf)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddImage(1)");
  if (pdfioPageDictAddImage(dict, "IM1", noimage))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  for (p = _PDFIO_PREDICTOR_PNG_NONE; p <= _PDFIO_PREDICTOR_PNG_AUTO; p ++)
  {
    testBegin("pdfioPageDictAddImage(%d)", p);
    snprintf(pname, sizeof(pname), "IM%d", p);
    if (pdfioPageDictAddImage(dict, pdfioStringCreate(pdf, pname), pimages[p - _PDFIO_PREDICTOR_PNG_NONE]))
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      return (1);
    }
  }

  testBegin("pdfioPageDictAddFont(F1)");
  if (pdfioPageDictAddFont(dict, "F1", font))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileCreatePage(%d)", number);

  if ((st = pdfioFileCreatePage(pdf, dict)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  if (write_header_footer(st, "Image Predictor Test", number))
    goto error;

  // Draw images
  if (draw_image(st, "IM1", 36, 522, 144, 144, "No Predictor"))
    goto error;

  for (p = _PDFIO_PREDICTOR_PNG_NONE; p <= _PDFIO_PREDICTOR_PNG_AUTO; p ++)
  {
    int i = (int)p - _PDFIO_PREDICTOR_PNG_NONE;

    snprintf(pname, sizeof(pname), "IM%d", p);
    snprintf(plabel, sizeof(plabel), "PNG Predictor %d", p);

    if (draw_image(st, pname, 36 + 180 * (i % 3), 306 - 216 * (i / 3), 144, 144, plabel))
      goto error;
  }

  // Wrap up...
  testBegin("pdfioStreamClose");
  if (pdfioStreamClose(st))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  return (0);

  error:

  pdfioStreamClose(st);
  return (1);
}


//
// 'write_jpeg_test()' - Write a page with a JPEG image to a PDF file.
//

static int				// O - 1 on failure, 0 on success
write_jpeg_test(pdfio_file_t *pdf,	// I - PDF file
                const char   *title,	// I - Page title
		int          number,	// I - Page number
		pdfio_obj_t  *font,	// I - Text font
		pdfio_obj_t  *image)	// I - Image to draw
{
  pdfio_dict_t		*dict;		// Page dictionary
  pdfio_stream_t	*st;		// Page contents stream
  double		width,		// Width of image
			height;		// Height of image
  double		swidth,		// Scaled width
			sheight,	// Scaled height
			tx,		// X offset
			ty;		// Y offset


  // Create the page dictionary, object, and stream...
  testBegin("pdfioDictCreate");
  if ((dict = pdfioDictCreate(pdf)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddImage");
  if (pdfioPageDictAddImage(dict, "IM1", image))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddFont(F1)");
  if (pdfioPageDictAddFont(dict, "F1", font))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileCreatePage(%d)", number);

  if ((st = pdfioFileCreatePage(pdf, dict)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  if (write_header_footer(st, title, number))
    goto error;

  // Calculate the scaled size of the image...
  testBegin("pdfioImageGetWidth()");
  if ((width = pdfioImageGetWidth(image)) > 0.0)
    testEnd(true);
  else
    goto error;

  testBegin("pdfioImageGetHeight()");
  if ((height = pdfioImageGetHeight(image)) > 0.0)
    testEnd(true);
  else
    goto error;

  swidth  = 400.0;
  sheight = swidth * height / width;
  if (sheight > 500.0)
  {
    sheight = 500.0;
    swidth  = sheight * width / height;
  }

  tx = 0.5 * (595.28 - swidth);
  ty = 0.5 * (792 - sheight);

  // Show "raw" content (a bordered box for the image...)
  testBegin("pdfioStreamPrintf(...)");
  if (pdfioStreamPrintf(st,
                       "1 0 0 RG 0 g 5 w\n"
                       "%g %g %g %g re %g %g %g %g re B*\n", tx - 36, ty - 36, swidth + 72, sheight + 72, tx - 1, ty - 1, swidth + 2, sheight + 2))
    testEnd(true);
  else
    goto error;

  // Draw the image inside the border box...
  testBegin("pdfioContentDrawImage(\"IM1\", x=%g, y=%g, w=%g, h=%g)", tx, ty, swidth, sheight);
  if (pdfioContentDrawImage(st, "IM1", tx, ty, swidth, sheight))
    testEnd(true);
  else
    goto error;

  // Close the page stream/object...
  testBegin("pdfioStreamClose");
  if (pdfioStreamClose(st))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  return (0);

  error:

  pdfioStreamClose(st);
  return (1);
}


//
// 'write_pdfa_file()' - Generate a simple PDF/A file.
//

static int				// O - Exit status
write_pdfa_file(
    const char *filename,		// I - Name of the PDF file to create
    const char *pdfa_version)		// I - PDF/A version string (e.g., "PDF/A-1b")
{
  int		status = 1;		// Exit status
  pdfio_file_t	*pdf;			// Output PDF file
  pdfio_obj_t	*font;			// Font object
  pdfio_obj_t	*color_jpg,		// JPEG file
		*pdfio_png;		// PNG file with transparency
  pdfio_dict_t	*page_dict;		// Page dictionary
  pdfio_stream_t *st;			// Page content stream
  bool		error = false;		// Error flag
  double	width,			// Width of image
		height;			// Height of image
  double	swidth,			// Scaled width
		sheight,		// Scaled height
		tx,			// X offset
		ty;			// Y offset


  testBegin("pdfioFileCreate(%s)", filename);

  if ((pdf = pdfioFileCreate(filename, pdfa_version, /*media_box*/NULL, /*crop_box*/NULL, (pdfio_error_cb_t)error_cb, &error)) == NULL)
  {
    testEnd(false);
    return (1);
  }

  testEnd(true);

  // Embed a base font, which are not allowed for PDF/A
  testBegin("pdfioFileCreateFontObjFromBase(Helvetica)");
  if (pdfioFileCreateFontObjFromBase(pdf, "Helvetica") != NULL)
  {
    testEnd(false);
    goto done;
  }

  testEnd(true);

  // Embed a font, which is required for PDF/A
  testBegin("pdfioFileCreateFontObjFromFile(testfiles/OpenSans-Regular.ttf)");
  if ((font = pdfioFileCreateFontObjFromFile(pdf, "testfiles/OpenSans-Regular.ttf", false)) == NULL)
  {
    testEnd(false);
    goto done;
  }

  testEnd(true);

  // Try embedding two images, one with alpha and one without...
  testBegin("pdfioFileCreateImageObjFromFile(testfiles/color.jpg)");
  if ((color_jpg = pdfioFileCreateImageObjFromFile(pdf, "testfiles/color.jpg", true)) == NULL)
  {
    testEnd(false);
    goto done;
  }

  testEnd(true);

  testBegin("pdfioFileCreateImageObjFromFile(testfiles/pdfio-rgba.png)");
  pdfio_png = pdfioFileCreateImageObjFromFile(pdf, "testfiles/pdfio-rgba.png", false);

  if ((pdfio_png != NULL && !strncmp(pdfa_version, "PDF/A-1", 7)) || (pdfio_png == NULL && strncmp(pdfa_version, "PDF/A-1", 7)))
  {
    testEnd(false);
    goto done;
  }

  testEnd(true);

  if (!pdfio_png)
  {
    testBegin("pdfioFileCreateImageObjFromFile(testfiles/pdfio-color.png)");
    if ((pdfio_png = pdfioFileCreateImageObjFromFile(pdf, "testfiles/pdfio-color.png", false)) == NULL)
    {
      testEnd(false);
      goto done;
    }

    testEnd(true);
  }

  // Create a page...
  page_dict = pdfioDictCreate(pdf);
  pdfioPageDictAddFont(page_dict, "F1", font);
  pdfioPageDictAddImage(page_dict, "I1", pdfio_png);
  pdfioPageDictAddImage(page_dict, "I2", color_jpg);

  testBegin("pdfioFileCreatePage()");
  if ((st = pdfioFileCreatePage(pdf, page_dict)) == NULL)
  {
    testEnd(false);
    goto done;
  }

  testEnd(true);

  pdfioContentSetFillColorDeviceRGB(st, 0.0, 0.0, 1.0);
  pdfioContentPathRect(st, 18.0, 702.0, 559.28, 72.0);
  pdfioContentFill(st, true);

  pdfioContentSetFillColorDeviceRGB(st, 1.0, 1.0, 1.0);
  pdfioContentSetStrokeColorDeviceRGB(st, 1.0, 1.0, 1.0);

  pdfioContentSetTextFont(st, "F1", 18.0);
  pdfioContentTextBegin(st);
  pdfioContentTextMoveTo(st, 81.0, 729.0);
  pdfioContentTextShowf(st, false, "This is a %s compliance test page.", pdfa_version);
  pdfioContentTextEnd(st);

  pdfioContentDrawImage(st, "I1", 36.0, 720.0, 36.0, 36.0);

  width  = pdfioImageGetWidth(color_jpg);
  height = pdfioImageGetHeight(color_jpg);

  swidth  = 400.0;
  sheight = swidth * height / width;
  if (sheight > 500.0)
  {
    sheight = 500.0;
    swidth  = sheight * width / height;
  }

  tx = 0.5 * (595.28 - swidth);
  ty = 0.5 * (720.0 - sheight);

  pdfioContentDrawImage(st, "I2", tx, ty, swidth, sheight);
  pdfioContentTextEnd(st);

  pdfioStreamClose(st);

  status = 0;

  done:

  testBegin("pdfioFileClose()");
  if (pdfioFileClose(pdf))
  {
    testEnd(true);
    return (status);
  }
  else
  {
    testEnd(false);
    return (1);
  }
}


//
// 'write_png_tests()' - Write pages of PNG test images.
//

static int				// O - 0 on success, 1 on failure
write_png_tests(pdfio_file_t *pdf,	// I - PDF file
	        int          number,	// I - Page number
	        pdfio_obj_t  *font)	// I - Page number font
{
  pdfio_dict_t		*dict;		// Page dictionary
  pdfio_stream_t	*st;		// Page contents stream
  pdfio_obj_t		*color,		// pdfio-color.png
			*gray,		// pdfio-gray.png
			*indexed;	// pdfio-indexed.png
#ifdef HAVE_LIBPNG
  size_t		i;		// Looping var
  char			imgname[32];	// Image name
  pdfio_obj_t		*pngsuite[80];	// PngSuite test file objects
  static const char * const pngsuite_files[80] =
  {					// PngSuite test filenames
    "testfiles/pngsuite/basi0g01.png", "testfiles/pngsuite/basi0g02.png",
    "testfiles/pngsuite/basi0g04.png", "testfiles/pngsuite/basi0g08.png",
    "testfiles/pngsuite/basi2c08.png", "testfiles/pngsuite/basi3p01.png",
    "testfiles/pngsuite/basi3p02.png", "testfiles/pngsuite/basi3p04.png",
    "testfiles/pngsuite/basi3p08.png", "testfiles/pngsuite/basi4a08.png",
    "testfiles/pngsuite/basi6a08.png", "testfiles/pngsuite/basn0g01.png",
    "testfiles/pngsuite/basn0g02.png", "testfiles/pngsuite/basn0g04.png",
    "testfiles/pngsuite/basn0g08.png", "testfiles/pngsuite/basn2c08.png",
    "testfiles/pngsuite/basn3p01.png", "testfiles/pngsuite/basn3p02.png",
    "testfiles/pngsuite/basn3p04.png", "testfiles/pngsuite/basn3p08.png",
    "testfiles/pngsuite/basn4a08.png", "testfiles/pngsuite/basn6a08.png",
    "testfiles/pngsuite/exif2c08.png", "testfiles/pngsuite/g03n2c08.png",
    "testfiles/pngsuite/g03n3p04.png", "testfiles/pngsuite/g04n2c08.png",
    "testfiles/pngsuite/g04n3p04.png", "testfiles/pngsuite/g05n2c08.png",
    "testfiles/pngsuite/g05n3p04.png", "testfiles/pngsuite/g07n2c08.png",
    "testfiles/pngsuite/g07n3p04.png", "testfiles/pngsuite/g10n2c08.png",
    "testfiles/pngsuite/g10n3p04.png", "testfiles/pngsuite/g25n2c08.png",
    "testfiles/pngsuite/g25n3p04.png", "testfiles/pngsuite/s02i3p01.png",
    "testfiles/pngsuite/s02n3p01.png", "testfiles/pngsuite/s03i3p01.png",
    "testfiles/pngsuite/s03n3p01.png", "testfiles/pngsuite/s04i3p01.png",
    "testfiles/pngsuite/s04n3p01.png", "testfiles/pngsuite/s05i3p02.png",
    "testfiles/pngsuite/s05n3p02.png", "testfiles/pngsuite/s06i3p02.png",
    "testfiles/pngsuite/s06n3p02.png", "testfiles/pngsuite/s07i3p02.png",
    "testfiles/pngsuite/s07n3p02.png", "testfiles/pngsuite/s08i3p02.png",
    "testfiles/pngsuite/s08n3p02.png", "testfiles/pngsuite/s09i3p02.png",
    "testfiles/pngsuite/s09n3p02.png", "testfiles/pngsuite/s32i3p04.png",
    "testfiles/pngsuite/s32n3p04.png", "testfiles/pngsuite/s33i3p04.png",
    "testfiles/pngsuite/s33n3p04.png", "testfiles/pngsuite/s34i3p04.png",
    "testfiles/pngsuite/s34n3p04.png", "testfiles/pngsuite/s35i3p04.png",
    "testfiles/pngsuite/s35n3p04.png", "testfiles/pngsuite/s36i3p04.png",
    "testfiles/pngsuite/s36n3p04.png", "testfiles/pngsuite/s37i3p04.png",
    "testfiles/pngsuite/s37n3p04.png", "testfiles/pngsuite/s38i3p04.png",
    "testfiles/pngsuite/s38n3p04.png", "testfiles/pngsuite/s39i3p04.png",
    "testfiles/pngsuite/s39n3p04.png", "testfiles/pngsuite/s40i3p04.png",
    "testfiles/pngsuite/s40n3p04.png", "testfiles/pngsuite/tbbn0g04.png",
    "testfiles/pngsuite/tbbn3p08.png", "testfiles/pngsuite/tbgn3p08.png",
    "testfiles/pngsuite/tbrn2c08.png", "testfiles/pngsuite/tbwn3p08.png",
    "testfiles/pngsuite/tbyn3p08.png", "testfiles/pngsuite/tm3n3p02.png",
    "testfiles/pngsuite/tp0n0g08.png", "testfiles/pngsuite/tp0n2c08.png",
    "testfiles/pngsuite/tp0n3p08.png", "testfiles/pngsuite/tp1n3p08.png"
  };
  static const char * const pngsuite_labels[80] =
  {					// PngSuite test labels
    "basi0g01", "basi0g02", "basi0g04", "basi0g08", "basi2c08", "basi3p01",
    "basi3p02", "basi3p04", "basi3p08", "basi4a08", "basi6a08", "basn0g01",
    "basn0g02", "basn0g04", "basn0g08", "basn2c08", "basn3p01", "basn3p02",
    "basn3p04", "basn3p08", "basn4a08", "basn6a08", "exif2c08", "g03n2c08",
    "g03n3p04", "g04n2c08", "g04n3p04", "g05n2c08", "g05n3p04", "g07n2c08",
    "g07n3p04", "g10n2c08", "g10n3p04", "g25n2c08", "g25n3p04", "s02i3p01",
    "s02n3p01", "s03i3p01", "s03n3p01", "s04i3p01", "s04n3p01", "s05i3p02",
    "s05n3p02", "s06i3p02", "s06n3p02", "s07i3p02", "s07n3p02", "s08i3p02",
    "s08n3p02", "s09i3p02", "s09n3p02", "s32i3p04", "s32n3p04", "s33i3p04",
    "s33n3p04", "s34i3p04", "s34n3p04", "s35i3p04", "s35n3p04", "s36i3p04",
    "s36n3p04", "s37i3p04", "s37n3p04", "s38i3p04", "s38n3p04", "s39i3p04",
    "s39n3p04", "s40i3p04", "s40n3p04", "tbbn0g04", "tbbn3p08", "tbgn3p08",
    "tbrn2c08", "tbwn3p08", "tbyn3p08", "tm3n3p02", "tp0n0g08", "tp0n2c08",
    "tp0n3p08", "tp1n3p08"
  };
#endif // HAVE_LIBPNG


  // Import the PNG test images
  testBegin("pdfioFileCreateImageObjFromFile(\"testfiles/pdfio-color.png\")");
  if ((color = pdfioFileCreateImageObjFromFile(pdf, "testfiles/pdfio-color.png", false)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileCreateImageObjFromFile(\"testfiles/pdfio-gray.png\")");
  if ((gray = pdfioFileCreateImageObjFromFile(pdf, "testfiles/pdfio-gray.png", false)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileCreateImageObjFromFile(\"testfiles/pdfio-indexed.png\")");
  if ((indexed = pdfioFileCreateImageObjFromFile(pdf, "testfiles/pdfio-indexed.png", false)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  ////// PDFio PNG image test page...
  // Create the page dictionary, object, and stream...
  testBegin("pdfioDictCreate");
  if ((dict = pdfioDictCreate(pdf)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddImage(color)");
  if (pdfioPageDictAddImage(dict, "IM1", color))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddImage(gray)");
  if (pdfioPageDictAddImage(dict, "IM2", gray))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddImage(indexed)");
  if (pdfioPageDictAddImage(dict, "IM3", indexed))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddFont(F1)");
  if (pdfioPageDictAddFont(dict, "F1", font))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileCreatePage(%d)", number);

  if ((st = pdfioFileCreatePage(pdf, dict)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  if (write_header_footer(st, "PNG Image Test Page", number))
    goto error;

  // Show content...
  testBegin("pdfioContentTextBegin()");
  if (pdfioContentTextBegin(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentSetTextFont(\"F1\", 18.0)");
  if (pdfioContentSetTextFont(st, "F1", 18.0))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextMoveTo(36.0, 342.0)");
  if (pdfioContentTextMoveTo(st, 36.0, 342.0))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextShow(\"PNG RGB Color\")");
  if (pdfioContentTextShow(st, false, "PNG RGB Color"))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextMoveTo(288.0, 0.0)");
  if (pdfioContentTextMoveTo(st, 288.0, 0.0))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextShow(\"PNG Gray\")");
  if (pdfioContentTextShow(st, false, "PNG Gray"))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextMoveTo(-288.0, 288.0)");
  if (pdfioContentTextMoveTo(st, -288.0, 288.0))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextShow(\"PNG Indexed\")");
  if (pdfioContentTextShow(st, false, "PNG Indexed"))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentTextEnd()");
  if (pdfioContentTextEnd(st))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentDrawImage(\"IM1\")");
  if (pdfioContentDrawImage(st, "IM1", 36, 108, 216, 216))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentDrawImage(\"IM2\")");
  if (pdfioContentDrawImage(st, "IM2", 324, 108, 216, 216))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentDrawImage(\"IM3\")");
  if (pdfioContentDrawImage(st, "IM3", 36, 396, 216, 216))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentSetFillColorDeviceRGB(0, 1, 1)");
  if (pdfioContentSetFillColorDeviceRGB(st, 0.0, 1.0, 1.0))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentPathRect(315, 387, 234, 234)");
  if (pdfioContentPathRect(st, 315, 387, 234, 234))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentFill(false)");
  if (pdfioContentFill(st, false))
    testEnd(true);
  else
    goto error;

  testBegin("pdfioContentDrawImage(\"IM3\")");
  if (pdfioContentDrawImage(st, "IM3", 324, 396, 216, 216))
    testEnd(true);
  else
    goto error;

  // Close the object and stream...
  testBegin("pdfioStreamClose");
  if (pdfioStreamClose(st))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

#ifdef HAVE_LIBPNG
  ////// PngSuite page
  // Create the image objects...
  for (i = 0; i < (sizeof(pngsuite_files) / sizeof(pngsuite_files[0])); i ++)
  {
    testBegin("pdfioFileCreateImageObjFromFile(\"%s\")", pngsuite_files[i]);
    if ((pngsuite[i] = pdfioFileCreateImageObjFromFile(pdf, pngsuite_files[i], false)) != NULL)
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      return (1);
    }
  }

  // Create the page dictionary, object, and stream...
  testBegin("pdfioDictCreate");
  if ((dict = pdfioDictCreate(pdf)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  for (i = 0; i < (sizeof(pngsuite_files) / sizeof(pngsuite_files[0])); i ++)
  {
    testBegin("pdfioPageDictAddImage(\"%s\")", pngsuite_labels[i]);
    snprintf(imgname, sizeof(imgname), "IM%u", (unsigned)(i + 1));
    if (pdfioPageDictAddImage(dict, pdfioStringCreate(pdf, imgname), pngsuite[i]))
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      return (1);
    }
  }

  testBegin("pdfioPageDictAddFont(F1)");
  if (pdfioPageDictAddFont(dict, "F1", font))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileCreatePage(%d)", number + 1);

  if ((st = pdfioFileCreatePage(pdf, dict)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  if (write_header_footer(st, "PngSuite Test Page", number + 1))
    goto error;

  // Show content...
  testBegin("pdfioContentSetTextFont(\"F1\", 9.0)");
  if (pdfioContentSetTextFont(st, "F1", 8.0))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    goto error;
  }

  for (i = 0; i < (sizeof(pngsuite_files) / sizeof(pngsuite_files[0])); i ++)
  {
    double x = (i % 8) * 69.0 + 36;	// X position
    double y = 671 - (i / 8) * 64.0;	// Y position

    testBegin("pdfioContentTextBegin()");
    if (pdfioContentTextBegin(st))
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      goto error;
    }

    testBegin("pdfioContentTextMoveTo(%g, %g)", x, y);
    if (pdfioContentTextMoveTo(st, x, y))
      testEnd(true);
    else
      goto error;

    testBegin("pdfioContentTextShow(\"%s\")", pngsuite_labels[i]);
    if (pdfioContentTextShow(st, false, pngsuite_labels[i]))
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      goto error;
    }

    testBegin("pdfioContentTextEnd()");
    if (pdfioContentTextEnd(st))
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      goto error;
    }
  }

  for (i = 0; i < (sizeof(pngsuite_files) / sizeof(pngsuite_files[0])); i ++)
  {
    double x = (i % 8) * 69.0 + 36;	// X position
    double y = 671 - (i / 8) * 64.0;	// Y position

    snprintf(imgname, sizeof(imgname), "IM%u", (unsigned)(i + 1));
    testBegin("pdfioContentDrawImage(\"%s\")", imgname);
    if (pdfioContentDrawImage(st, imgname, x, y + 9, 32, 32))
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      goto error;
    }
  }

  // Close the object and stream...
  testBegin("pdfioStreamClose");
  if (pdfioStreamClose(st))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }
#endif // HAVE_LIBPNG

  return (0);

  error:

  pdfioStreamClose(st);
  return (1);
}


//
// 'write_text_test()' - Print a plain text file.
//

static int				// O - 0 on success, 1 on failure
write_text_test(pdfio_file_t *pdf,		// I - PDF file
           int          first_page,	// I - First page number
           pdfio_obj_t  *font,		// I - Page number font
           const char   *filename)	// I - File to print
{
  pdfio_obj_t		*courier;	// Courier font
  pdfio_dict_t		*dict;		// Page dictionary
  FILE			*fp;		// Print file
  char			line[1024];	// Line from file
  int			page,		// Current page number
			plinenum,	// Current line number on page
			flinenum;	// Current line number in file
  pdfio_stream_t	*st = NULL;	// Page contents stream


  // Create text font...
  testBegin("pdfioFileCreateFontObjFromBase(\"Courier\")");
  if ((courier = pdfioFileCreateFontObjFromBase(pdf, "Courier")) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  // Create the page dictionary...
  testBegin("pdfioDictCreate");
  if ((dict = pdfioDictCreate(pdf)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddFont(F1)");
  if (pdfioPageDictAddFont(dict, "F1", font))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageDictAddFont(F2)");
  if (pdfioPageDictAddFont(dict, "F2", courier))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  // Open the print file...
  if ((fp = fopen(filename, "r")) == NULL)
  {
    printf("Unable to open \"%s\": %s\n", filename, strerror(errno));
    return (1);
  }

  page     = first_page;
  plinenum = 0;
  flinenum = 0;

  while (fgets(line, sizeof(line), fp))
  {
    flinenum ++;

    if (plinenum == 0)
    {
      testBegin("pdfioFileCreatePage(%d)", page);

      if ((st = pdfioFileCreatePage(pdf, dict)) != NULL)
	testEnd(true);
      else
        goto error;

      if (write_header_footer(st, "README.md", page))
	goto error;

      page ++;
      plinenum ++;

      testBegin("pdfioContentTextBegin()");
      if (pdfioContentTextBegin(st))
	testEnd(true);
      else
	goto error;

      testBegin("pdfioContentSetTextFont(\"F2\", 10.0)");
      if (pdfioContentSetTextFont(st, "F2", 10.0))
	testEnd(true);
      else
	goto error;

      testBegin("pdfioContentSetTextLeading(12.0)");
      if (pdfioContentSetTextLeading(st, 12.0))
	testEnd(true);
      else
	goto error;

      testBegin("pdfioContentTextMoveTo(36.0, 708.0)");
      if (pdfioContentTextMoveTo(st, 36.0, 708.0))
	testEnd(true);
      else
	goto error;
    }

    if (!pdfioContentSetFillColorDeviceGray(st, 0.75))
      goto error;
    if (!pdfioContentTextShowf(st, false, "%3d  ", flinenum))
      goto error;
    if (!pdfioContentSetFillColorDeviceGray(st, 0.0))
      goto error;
    if (strlen(line) > 81)
    {
      char	temp[82];	// Temporary string

      memcpy(temp, line, 80);
      temp[80] = '\n';
      temp[81] = '\0';

      if (!pdfioContentTextShow(st, false, temp))
        goto error;

      if (!pdfioContentTextShowf(st, false, "     %s", line + 80))
        goto error;

      plinenum ++;
    }
    else if (!pdfioContentTextShow(st, false, line))
      goto error;

    plinenum ++;
    if (plinenum >= 56)
    {
      testBegin("pdfioContentTextEnd()");
      if (pdfioContentTextEnd(st))
	testEnd(true);
      else
	goto error;

      testBegin("pdfioStreamClose");
      if (pdfioStreamClose(st))
	testEnd(true);
      else
	goto error;

      st       = NULL;
      plinenum = 0;
    }
  }

  if (plinenum > 0)
  {
    testBegin("pdfioContentTextEnd()");
    if (pdfioContentTextEnd(st))
      testEnd(true);
    else
      goto error;

    testBegin("pdfioStreamClose");
    if (pdfioStreamClose(st))
    {
      testEnd(true);
    }
    else
    {
      testEnd(false);
      fclose(fp);
      return (1);
    }
  }

  fclose(fp);

  return (0);

  error:

  testEnd(false);
  fclose(fp);
  pdfioStreamClose(st);
  return (1);
}


//
// 'write_unit_file()' - Write a unit test file.
//

static int				// O - Exit status
write_unit_file(
    pdfio_file_t *inpdf,		// I - Input PDF file
    const char   *outname,		// I - Output PDF file name
    pdfio_file_t *outpdf,		// I - Output PDF file
    size_t       *num_pages,		// O - Number of pages
    size_t       *first_image)		// O - First image object
{
  const char		*s;		// String buffer
  pdfio_obj_t		*color_jpg,	// color.jpg image
			*gray_jpg,	// gray.jpg image
			*helvetica,	// Helvetica font
			*page;		// Page from test PDF file
  int			pagenum = 1;	// Current page number
  pdfio_dict_t		*catalog;	// Catalog dictionary


  // Get the root object/catalog dictionary
  testBegin("pdfioFileGetCatalog");
  if ((catalog = pdfioFileGetCatalog(outpdf)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEndMessage(false, "got NULL, expected dictionary");
    return (1);
  }

  // Set some catalog values...
  pdfioDictSetName(catalog, "PageLayout", "SinglePage");
  pdfioDictSetName(catalog, "PageMode", "UseThumbs");

  // Set info values...
  testBegin("pdfioFileGet/SetAuthor");
  pdfioFileSetAuthor(outpdf, "Michael R Sweet");
  if ((s = pdfioFileGetAuthor(outpdf)) != NULL && !strcmp(s, "Michael R Sweet"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'Michael R Sweet'", s);
    return (1);
  }
  else
  {
    testEndMessage(false, "got NULL, expected 'Michael R Sweet'");
    return (1);
  }

  testBegin("pdfioFileGet/SetCreator");
  pdfioFileSetCreator(outpdf, "testpdfio");
  if ((s = pdfioFileGetCreator(outpdf)) != NULL && !strcmp(s, "testpdfio"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'testpdfio'", s);
    return (1);
  }
  else
  {
    testEndMessage(false, "got NULL, expected 'testpdfio'");
    return (1);
  }

  testBegin("pdfioFileGet/SetKeywords");
  pdfioFileSetKeywords(outpdf, "one fish,two fish,red fish,blue fish");
  if ((s = pdfioFileGetKeywords(outpdf)) != NULL && !strcmp(s, "one fish,two fish,red fish,blue fish"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'one fish,two fish,red fish,blue fish'", s);
    return (1);
  }
  else
  {
    testEndMessage(false, "got NULL, expected 'one fish,two fish,red fish,blue fish'");
    return (1);
  }

  testBegin("pdfioFileGet/SetLanguage");
  pdfioFileSetLanguage(outpdf, "en-CA");
  if ((s = pdfioFileGetLanguage(outpdf)) != NULL && !strcmp(s, "en-CA"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'en-CA'", s);
    return (1);
  }
  else
  {
    testEndMessage(false, "got NULL, expected 'en-CA'");
    return (1);
  }

  testBegin("pdfioFileGet/SetSubject");
  pdfioFileSetSubject(outpdf, "Unit test document");
  if ((s = pdfioFileGetSubject(outpdf)) != NULL && !strcmp(s, "Unit test document"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'Unit test document'", s);
    return (1);
  }
  else
  {
    testEndMessage(false, "got NULL, expected 'Unit test document'");
    return (1);
  }

  testBegin("pdfioFileGet/SetTitle");
  pdfioFileSetTitle(outpdf, "Test Document");
  if ((s = pdfioFileGetTitle(outpdf)) != NULL && !strcmp(s, "Test Document"))
  {
    testEnd(true);
  }
  else if (s)
  {
    testEndMessage(false, "got '%s', expected 'Test Document'", s);
    return (1);
  }
  else
  {
    testEndMessage(false, "got NULL, expected 'Test Document'");
    return (1);
  }

  // Create some image objects...
  testBegin("pdfioFileCreateImageObjFromFile(\"testfiles/color.jpg\")");
  if ((color_jpg = pdfioFileCreateImageObjFromFile(outpdf, "testfiles/color.jpg", true)) != NULL)
  {
    testEndMessage(true, "%u", (unsigned)pdfioObjGetNumber(color_jpg));
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioFileCreateImageObjFromFile(\"testfiles/gray.jpg\")");
  if ((gray_jpg = pdfioFileCreateImageObjFromFile(outpdf, "testfiles/gray.jpg", true)) != NULL)
  {
    testEndMessage(true, "%u", (unsigned)pdfioObjGetNumber(gray_jpg));
  }
  else
  {
    testEnd(false);
    return (1);
  }

  // Create fonts...
  testBegin("pdfioFileCreateFontObjFromBase(\"Helvetica\")");
  if ((helvetica = pdfioFileCreateFontObjFromBase(outpdf, "Helvetica")) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  // Copy the first page from the test PDF file...
  testBegin("pdfioFileGetPage(0)");
  if ((page = pdfioFileGetPage(inpdf, 0)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageCopy(first page)");
  if (pdfioPageCopy(outpdf, page))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  pagenum ++;

  // Write a page with a color image...
  if (write_jpeg_test(outpdf, "Color JPEG Test", pagenum, helvetica, color_jpg))
    return (1);

  pagenum ++;

  // Copy the third page from the test PDF file...
  testBegin("pdfioFileGetPage(2)");
  if ((page = pdfioFileGetPage(inpdf, 2)) != NULL)
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  testBegin("pdfioPageCopy(third page)");
  if (pdfioPageCopy(outpdf, page))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  pagenum ++;

  // Write a page with a grayscale image...
  if (write_jpeg_test(outpdf, "Grayscale JPEG Test", pagenum, helvetica, gray_jpg))
    return (1);
  pagenum ++;

  // Write a page with PNG images...
  if (write_png_tests(outpdf, pagenum, helvetica))
    return (1);

#ifdef HAVE_LIBPNG
  pagenum += 2;
#else
  pagenum ++;
#endif // HAVE_LIBPNG

  // Write a page that tests multiple color spaces...
  if (write_color_test(outpdf, pagenum, helvetica))
    return (1);

  pagenum ++;

  // Write a page with test images...
  *first_image = pdfioFileGetNumObjs(outpdf) + 1;
  if (write_images_test(outpdf, pagenum, helvetica))
    return (1);

  pagenum ++;

  // Write a page width alpha (soft masks)...
  if (write_alpha_test(outpdf, pagenum, helvetica))
    return (1);

  pagenum ++;

  // Test TrueType fonts...
  if (write_font_test(outpdf, pagenum, helvetica, "testfiles/OpenSans-Regular.ttf", false))
    return (1);

  pagenum ++;

  if (write_font_test(outpdf, pagenum, helvetica, "testfiles/OpenSans-Regular.ttf", true))
    return (1);

  pagenum ++;

  if (write_font_test(outpdf, pagenum, helvetica, "testfiles/NotoSansJP-Regular.otf", true))
    return (1);

  pagenum ++;

  // Print this text file...
  if (write_text_test(outpdf, pagenum, helvetica, "README.md"))
    return (1);

  testBegin("pdfioFileGetNumPages");
  if ((*num_pages = pdfioFileGetNumPages(outpdf)) > 0)
  {
    testEndMessage(true, "%lu", (unsigned long)*num_pages);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  // Close the new PDF file...
  testBegin("pdfioFileClose(\"%s\")", outname);
  if (pdfioFileClose(outpdf))
  {
    testEnd(true);
  }
  else
  {
    testEnd(false);
    return (1);
  }

  return (0);
}
