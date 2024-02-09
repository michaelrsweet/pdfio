#include <stdio.h>
#include <stdlib.h>
#include "pdfio.h"

/*
*	Usage: ./test_mod [file.pdf]
*
* Compiled as:
* gcc test_mod.c -L. -lpdfio -lm -lz -o test_mod
*
*/


int main (int argc, char **argv)
{
	pdfio_file_t *pdf = pdfioFileOpen(argv[1], NULL, NULL, NULL, NULL);
	pdfio_obj_t *obj_page = pdfioFileGetPage(pdf, 0);
	pdfio_dict_t *dict_page = pdfioObjGetDict(obj_page);

	size_t num_keys = pdfioDictGetNumKeys(dict_page);
	printf("Number of keys in this page: %d\n", num_keys);

	for (unsigned int i = 0; i < num_keys; ++i)
	{
		pdfio_dictKey_t dict_key;
		pdfioDictGetKeyByIndex(dict_page, i, &dict_key);
		printf("\t%s (%d)\n", dict_key.key, dict_key.type);
	}

	pdfioFileClose(pdf);

	return 0;
}
