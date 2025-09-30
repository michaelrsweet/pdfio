#include <pdfio.h>
#include <pdfio-content.h>
#include <string.h>

int main(void)
{
    // 1. Basic PDF setup
    pdfio_file_t *pdf;
    pdfio_rect_t media_box = {0.0, 0.0, 612.0, 792.0}; // US Letter size

    // Create the PDF file
    if ((pdf = pdfioFileCreate("tagged_document.pdf", "2.0", &media_box, &media_box, NULL, NULL)) == NULL)
    {
        puts("Error: Could not create PDF file.");
        return (1);
    }

    // 2. Build the Structure Tree Root (StructTreeRoot)
    // This is the master "table of contents" for all tags.
    pdfio_dict_t  *struct_tree_root_dict = pdfioDictCreate(pdf);
    pdfioDictSetName(struct_tree_root_dict, "Type", "StructTreeRoot");

    // Create the top-level document element tag: /Document
    pdfio_dict_t  *doc_elem_dict = pdfioDictCreate(pdf);
    pdfioDictSetName(doc_elem_dict, "Type", "StructElem");
    pdfioDictSetName(doc_elem_dict, "S", "Document"); // 'S' is the structure type

    // Create the paragraph element tag: /P
    pdfio_dict_t  *p_elem_dict = pdfioDictCreate(pdf);
    pdfioDictSetName(p_elem_dict, "Type", "StructElem");
    pdfioDictSetName(p_elem_dict, "S", "P"); // 'S' is the structure type (Paragraph)
    pdfioDictSetNumber(p_elem_dict, "K", 0); // 'K' is the content, pointing to MCID 0 on the page

    // Link the paragraph as a child of the document element
    pdfio_array_t *doc_kids = pdfioArrayCreate(pdf);
    pdfioArrayAppendDict(doc_kids, p_elem_dict);
    pdfioDictSetArray(doc_elem_dict, "K", doc_kids);

    // Link the document element as a child of the StructTreeRoot
    pdfio_array_t *root_kids = pdfioArrayCreate(pdf);
    pdfioArrayAppendDict(root_kids, doc_elem_dict);
    pdfioDictSetArray(struct_tree_root_dict, "K", root_kids);

    // Create a PDF object for the StructTreeRoot and link it to the main catalog
    pdfio_obj_t *struct_tree_root_obj = pdfioFileCreateObj(pdf, struct_tree_root_dict);
    pdfioDictSetObj(pdfioFileGetCatalog(pdf), "StructTreeRoot", struct_tree_root_obj);

    // 3. Create a page and its content
    pdfio_dict_t  *page_dict = pdfioDictCreate(pdf);
    pdfio_obj_t   *helvetica = pdfioFileCreateFontObjFromBase(pdf, "Helvetica");
    pdfioPageDictAddFont(page_dict, "F1", helvetica);

    pdfio_stream_t *st = pdfioFileCreatePage(pdf, page_dict);

    // 4. Write the tagged content to the page stream
    pdfioContentTextBegin(st);
    pdfioContentSetTextFont(st, "F1", 24.0);
    pdfioContentTextMoveTo(st, 72.0, 700.0);

    // Create a dictionary for the marked content, specifying the ID
    pdfio_dict_t *p_mcid_dict = pdfioDictCreate(pdf);
    pdfioDictSetNumber(p_mcid_dict, "MCID", 0); // This ID must match the 'K' value in the StructElem

    // Use the functions from pdfio-content.c to wrap the text
    pdfioContentBeginMarked(st, "P", p_mcid_dict); // Start tag for Paragraph
    pdfioContentTextShow(st, false, "This is a tagged paragraph.");
    pdfioContentEndMarked(st);                     // End tag

    pdfioContentTextEnd(st);

    // 5. Finalize and close
    pdfioStreamClose(st);
    pdfioFileClose(pdf);

    puts("Successfully created tagged_document.pdf");
    return (0);
}
