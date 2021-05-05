//
// Test program for pdfio.
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


//
// 'main()' - Main entry for test program.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  if (argc > 1)
  {
    int			i;		// Looping var
    pdfio_file_t	*pdf;		// PDF file
    size_t		n,		// Object/page index
			num_objs,	// Number of objects
			num_pages;	// Number of pages
    pdfio_obj_t		*obj;		// Object
    pdfio_dict_t	*dict;		// Object dictionary
    const char		*type;		// Object type

    for (i = 1; i < argc; i ++)
    {
      if ((pdf = pdfioFileOpen(argv[i], NULL, NULL)) != NULL)
      {
        num_objs  = pdfioFileGetNumObjects(pdf);
        num_pages = pdfioFileGetNumPages(pdf);

	printf("%s: PDF %s, %d objects, %d pages.\n", argv[i], pdfioFileGetVersion(pdf), (int)num_objs, (int)num_pages);

        for (n = 0; n < num_objs; n ++)
        {
          if ((obj = pdfioFileGetObject(pdf, n)) == NULL)
          {
            printf("%s: Unable to get object #%d.\n", argv[i], (int)n);
          }
          else
          {
            size_t 		np;	// Number of pairs
            _pdfio_pair_t	*pair;	// Current pair

            dict = pdfioObjGetDict(obj);

            printf("%s: %u %u obj dict=%p(%lu)\n", argv[i], (unsigned)pdfioObjGetNumber(obj), (unsigned)pdfioObjGetGeneration(obj), dict, dict ? (unsigned long)dict->num_pairs : 0UL);
            if (dict)
            {
              for (np = dict->num_pairs, pair = dict->pairs; np > 0; np --, pair ++)
              {
		switch (pair->value.type)
		{
		  case PDFIO_VALTYPE_INDIRECT :
		      printf("    /%s %u %u R\n", pair->key, (unsigned)pair->value.value.indirect.number, pair->value.value.indirect.generation);
		      break;
		  case PDFIO_VALTYPE_NAME :
		      printf("    /%s /%s\n", pair->key, pair->value.value.name);
		      break;
		  case PDFIO_VALTYPE_STRING :
		      printf("    /%s (%s)\n", pair->key, pair->value.value.string);
		      break;
		  case PDFIO_VALTYPE_BINARY :
		      {
		        size_t bn;
		        unsigned char *bptr;

		        printf("    /%s <", pair->key);
		        for (bn = pair->value.value.binary.datalen, bptr = pair->value.value.binary.data; bn > 0; bn --, bptr ++)
		          printf("%02X", *bptr);
		        puts(">");
		      }
		      break;
		  case PDFIO_VALTYPE_NUMBER :
		      printf("    /%s %g\n", pair->key, pair->value.value.number);
		      break;
		  case PDFIO_VALTYPE_BOOLEAN :
		      printf("    /%s %s\n", pair->key, pair->value.value.boolean ? "true" : "false");
		      break;
		  case PDFIO_VALTYPE_NULL :
		      printf("    /%s null\n", pair->key);
		      break;
		  case PDFIO_VALTYPE_ARRAY :
		      printf("    /%s [...]\n", pair->key);
		      break;
		  case PDFIO_VALTYPE_DICT :
		      printf("    /%s <<...>>\n", pair->key);
		      break;
		  default :
		      printf("    /%s ...\n", pair->key);
		      break;
		}
              }
            }
          }
        }

	pdfioFileClose(pdf);
      }
    }
  }

  return (0);
}
