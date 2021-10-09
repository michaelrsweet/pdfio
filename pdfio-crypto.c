//
// Cryptographic support functions for PDFio.
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
// '_pdfioCryptoMakeReader()' - Setup a cryptographic context and callback for reading.
//

_pdfio_crypto_cb_t			// O  - Decryption callback or `NULL` for none
 _pdfioCryptoMakeReader(
     pdfio_file_t        *pdf,		// I  - PDF file
     pdfio_obj_t         *obj,		// I  - PDF object
     _pdfio_crypto_ctx_t *ctx,		// I  - Pointer to crypto context
     uint8_t             *iv,		// I  - Buffer for initialization vector
     size_t              *ivlen)	// IO - Size of initialization vector
{
  (void)pdf;
  (void)obj;
  (void)ctx;
  (void)iv;


  // No decryption...
  *ivlen = 0;

  return (NULL);
}


//
// '_pdfioCryptoMakeWriter()' - Setup a cryptographic context and callback for writing.
//

_pdfio_crypto_cb_t			// O  - Encryption callback or `NULL` for none
 _pdfioCryptoMakeWriter(
     pdfio_file_t        *pdf,		// I  - PDF file
     pdfio_obj_t         *obj,		// I  - PDF object
     _pdfio_crypto_ctx_t *ctx,		// I  - Pointer to crypto context
     uint8_t             *iv,		// I  - Buffer for initialization vector
     size_t              *ivlen)	// IO - Size of initialization vector
{
  (void)pdf;
  (void)obj;
  (void)ctx;
  (void)iv;


  // No encryption...
  *ivlen = 0;

  return (NULL);
}
