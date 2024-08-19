//
// Unit test program for TTF library
//
//     https://github.com/michaelrsweet/ttf
//
// Copyright © 2018-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Usage:
//
//   ./testttf [FILENAME]
//

#include <stdio.h>
#include "ttf.h"


//
// Local functions...
//

static void	error_cb(void *data, const char *message);
static int	test_font(const char *filename);


//
// 'main()' - Main entry for unit tests.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  int		errors = 0;		// Number of errors


  if (argc > 1)
  {
    for (i = 1; i < argc; i ++)
      errors += test_font(argv[i]);
  }
  else
  {
    // Test with the bundled TrueType files...
    errors += test_font("testfiles/OpenSans-Bold.ttf");
    errors += test_font("testfiles/OpenSans-Regular.ttf");
    errors += test_font("testfiles/NotoSansJP-Regular.otf");
  }

  if (!errors)
    puts("\nALL TESTS PASSED");
  else
    printf("\n%d TEST(S) FAILED\n", errors);

  return (errors);
}


//
// 'error_cb()' - Error callback.
//

static void
error_cb(void       *data,		// I - User data (not used)
         const char *message)		// I - Message string
{
  fprintf(stderr, "FAIL (%s)\n", message);
}


//
// 'test_font()' - Test a font file.
//

static int				// O - Number of errors
test_font(const char *filename)		// I - Font filename
{
  int		i,			// Looping var
		errors = 0;		// Number of errors
  ttf_t		*font;			// Font
  const char	*value;			// Font (string) value
  int		intvalue;		// Font (integer) value
  float		realvalue;		// Font (real) value
  ttf_rect_t	bounds;			// Bounds
  ttf_rect_t	extents;		// Extents
  size_t	num_fonts;		// Number of fonts
  ttf_style_t	style;			// Font style
  ttf_weight_t	weight;			// Font weight
  static const char * const stretches[] =
  {					// Font stretch strings
    "TTF_STRETCH_NORMAL",		// normal
    "TTF_STRETCH_ULTRA_CONDENSED",	// ultra-condensed
    "TTF_STRETCH_EXTRA_CONDENSED",	// extra-condensed
    "TTF_STRETCH_CONDENSED",		// condensed
    "TTF_STRETCH_SEMI_CONDENSED",	// semi-condensed
    "TTF_STRETCH_SEMI_EXPANDED",	// semi-expanded
    "TTF_STRETCH_EXPANDED",		// expanded
    "TTF_STRETCH_EXTRA_EXPANDED",	// extra-expanded
    "TTF_STRETCH_ULTRA_EXPANDED"	// ultra-expanded
  };
  static const char * const strings[] =	// Test strings
  {
    "Hello, World!",			// English
    "مرحبا بالعالم!",			// Arabic
    "Bonjour le monde!",		// French
    "Γειά σου Κόσμε!",			// Greek
    "שלום עולם!",			// Hebrew
    "Привет мир!",			// Russian
    "こんにちは世界！"			// Japanese
  };
  static const char * const styles[] =	// Font style names
  {
    "TTF_STYLE_NORMAL",
    "TTF_STYLE_ITALIC",
    "TTF_STYLE_OBLIQUE"
  };


  printf("ttfCreate(\"%s\"): ", filename);
  fflush(stdout);
  if ((font = ttfCreate(filename, 0, error_cb, NULL)) != NULL)
    puts("PASS");
  else
    errors ++;

  fputs("ttfGetAscent: ", stdout);
  if ((intvalue = ttfGetAscent(font)) > 0)
  {
    printf("PASS (%d)\n", intvalue);
  }
  else
  {
    printf("FAIL (%d)\n", intvalue);
    errors ++;
  }

  fputs("ttfGetBounds: ", stdout);
  if (ttfGetBounds(font, &bounds))
  {
    printf("PASS (%g %g %g %g)\n", bounds.left, bounds.bottom, bounds.right, bounds.top);
  }
  else
  {
    puts("FAIL");
    errors ++;
  }

  fputs("ttfGetCapHeight: ", stdout);
  if ((intvalue = ttfGetCapHeight(font)) > 0)
  {
    printf("PASS (%d)\n", intvalue);
  }
  else
  {
    printf("FAIL (%d)\n", intvalue);
    errors ++;
  }

  fputs("ttfGetCopyright: ", stdout);
  if ((value = ttfGetCopyright(font)) != NULL)
  {
    printf("PASS (%s)\n", value);
  }
  else
  {
    puts("WARNING (no copyright found)");
  }

  for (i = 0; i < (int)(sizeof(strings) / sizeof(strings[0])); i ++)
  {
    printf("ttfGetExtents(\"%s\"): ", strings[i]);
    if (ttfGetExtents(font, 12.0f, strings[i], &extents))
    {
      printf("PASS (%.1f %.1f %.1f %.1f)\n", extents.left, extents.bottom, extents.right, extents.top);
    }
    else
    {
      puts("FAIL");
      errors ++;
    }
  }

  fputs("ttfGetFamily: ", stdout);
  if ((value = ttfGetFamily(font)) != NULL)
  {
    printf("PASS (%s)\n", value);
  }
  else
  {
    puts("FAIL");
    errors ++;
  }

  fputs("ttfGetItalicAngle: ", stdout);
  if ((realvalue = ttfGetItalicAngle(font)) >= -180.0 && realvalue <= 180.0)
  {
    printf("PASS (%g)\n", realvalue);
  }
  else
  {
    printf("FAIL (%g)\n", realvalue);
    errors ++;
  }

  fputs("ttfGetNumFonts: ", stdout);
  if ((num_fonts = ttfGetNumFonts(font)) > 0)
  {
    printf("PASS (%u)\n", (unsigned)num_fonts);
  }
  else
  {
    puts("FAIL");
    errors ++;
  }

  fputs("ttfGetPostScriptName: ", stdout);
  if ((value = ttfGetPostScriptName(font)) != NULL)
  {
    printf("PASS (%s)\n", value);
  }
  else
  {
    puts("FAIL");
    errors ++;
  }

  fputs("ttfGetStretch: ", stdout);
  if ((intvalue = (int)ttfGetStretch(font)) >= TTF_STRETCH_NORMAL && intvalue <= TTF_STRETCH_ULTRA_EXPANDED)
  {
    printf("PASS (%s)\n", stretches[intvalue]);
  }
  else
  {
    printf("FAIL (%d)\n", intvalue);
    errors ++;
  }

  fputs("ttfGetStyle: ", stdout);
  if ((style = ttfGetStyle(font)) >= TTF_STYLE_NORMAL && style <= TTF_STYLE_ITALIC)
  {
    printf("PASS (%s)\n", styles[style]);
  }
  else
  {
    puts("FAIL");
    errors ++;
  }

  fputs("ttfGetVersion: ", stdout);
  if ((value = ttfGetVersion(font)) != NULL)
  {
    printf("PASS (%s)\n", value);
  }
  else
  {
    puts("FAIL");
    errors ++;
  }

  fputs("ttfGetWeight: ", stdout);
  if ((weight = ttfGetWeight(font)) >= 0)
  {
    printf("PASS (%u)\n", (unsigned)weight);
  }
  else
  {
    puts("FAIL");
    errors ++;
  }

  fputs("ttfGetWidth(' '): ", stdout);
  if ((intvalue = ttfGetWidth(font, ' ')) > 0)
  {
    printf("PASS (%d)\n", intvalue);
  }
  else
  {
    printf("FAIL (%d)\n", intvalue);
    errors ++;
  }

  fputs("ttfGetXHeight: ", stdout);
  if ((intvalue = ttfGetXHeight(font)) > 0)
  {
    printf("PASS (%d)\n", intvalue);
  }
  else
  {
    printf("FAIL (%d)\n", intvalue);
    errors ++;
  }

  fputs("ttfIsFixedPitch: ", stdout);
  if (ttfIsFixedPitch(font))
    puts("PASS (true)");
  else
    puts("PASS (false)");

  ttfDelete(font);

  return (errors);
}
