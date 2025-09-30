//
// Test program from pdf-a
//
// This file is specifically designed to test the PDF/A creation feature
//

#include "pdfio.h"
#include "pdfio-content.h"
#include <stdio.h>
#include <string.h>

// Local Function

static bool create_pdfa_test_file(const char *filename, const char *pdfa_version);
static int test_pdfa(void);

// 'create_pdfa_test_file()' -> A helper function to generate a simple
// PDF/A file


static bool                      // 0 - true on success, false on error
create_pdfa_test_file(
    const char *filename,        // I - Name of the pdf file to create
    const char *pdfa_version)    // I - PDF/A version string (e.g., "PDF/A-1b")
{
  pdfio_file_t *pdf;            // Ouput PDF File
  pdfio_rect_t media-box = { 0.0, 0.0, 612.0, 792.0 }; // Media box for US Letter
  pdfio_obj_t *font;            // Font Object
  pdfio_dict_t *page_dict;      // Page Dictionary
  pdfio_stream_t *st;           // Page content stream
  char  text[256];              // Text to write to page

  // Let the user know what we're doing
  snprintf(text, sizeof(text), "This is a compliance test for %s.", pdfa_version);
  printf("  Creating '%s' for %s compilance check ... \n", filename, pdfa_version);

  // Create the PDF/A file using the specified version string
  if ((pdf = pdfioFileCreate(filename, pdfa_version, &media_box, NULL, NULL, NULL)) == NULL)
  {
    fprintf(stderr, "    ERROR: Unable to create '%s'.\n",filename);
    return (false);
  }

  // Add some basic content to make it a valid, non-empty PDF
  font = pdfioFileCreateFontObjFromBase(pdf,"Helvetica");
  page_dict = pdfioDictCreate(pdf);
  pdfioPageDictAddFont(page_dict, "F1", font);
  st = pdfioFileCreatePage(pdf, page_dict);

  pdfioContentSetTextFont(st, "F1", 12.0);
  pdfioContentTextBegin(st);
  pdfioContentTextMoveTo(st, 72.0, 720.0); 
  pdfioContentTextShow(st, false, text);
  pdfioContentTextEnd(st);

  // Close the stream and the file to finalize and save it
  pdfioStreamClose(st);
  pdfioFileClose(pdf);

  printf("     Successfully created '%s'.\n", filename);
  return (true);
}

// 'test_pdfa()' - The main test runner for the PDF/A feature.'

static int                      // 0 - 0 on success, 1 on error
test_pdfa(void)
{
  int status = 0;               // Overall test status
  pdfio_file_t *fail_pdf;       // PDF for failure test
  pdfio_rect_t media_box = {0.0, 0.0, 612.0, 792.0 };


  puts (" ----- Running PDF/A Generation Tests --- \n");

  // --- Positive Test Cases: Generate one file for each conformance level ---
  if (!create_pdfa_test_file("test-pdfa-1b.pdf", "PDF/A-1b")) status = 1;
  if (!create_pdfa_test_file("test-pdfa-2b.pdf", "PDF/A-2b")) status = 1;
  if (!create_pdfa_test_file("test-pdfa-2u.pdf", "PDF/A-2u")) status = 1;
  if (!create_pdfa_test_file("test-pdfa-3b.pdf", "PDF/A-3b")) status = 1;
  if (!create_pdfa_test_file("test-pdfa-3u.pdf", "PDF/A-3u")) status = 1;
  if (!create_pdfa_test_file("test-pdfa-4.pdf", "PDF/A-4")) status = 1;

  // --- Navigate test case: Ensure encryption is blocked --
  puts("\n--- Running PDF/A Encryption Block Test ---\n");

  printf(" Creating PDF/A file to test encryption failure .. \n");
  if ((fail_pdf = pdfioFileCreate("test-pdfa-fail.pdf", "PDF/A-1b", &media_box, NULL,NULL,NULL)) == NULL)
  {
    fputs(" ERROR: Unable to create temporary file for encryption test.\n", stderr);
    return (1);
  }

  // This call MUST fail because encryption is not allowed in PDF/A
  if (pdfioFileSetPermissions(fail_pdf, PDFIO_PERMISSION_ALL, PDFIO_ENCRYPTION_RC4_128, "owner", "user"))
  {
    fputs("   ERROR: pdfioFileSetPermission succeeded but should have FAILED!\n",stderr);
    status = 1; // Mark the test suite as failed
  }
  else
  {
    puts(" SUCCESS: COrrectly blocked encryption for PDF/A file as expected.");
  }
  pdfioFileClose(fail_pdf);

  //--- Final Summary----
  puts("\n-------------------------");
  if (status == 0)
    puts(" All PDF/A test passed.");
  else
    puts(" One or more PDF/A tests FAILED.");
  puts("\n--------------------------------\n");

  return (status);
}


// 'main()' 

int           // 0 - Exit status
main(void)
{
  return (test_pdfa());
}



