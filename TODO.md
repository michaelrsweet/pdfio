To-Do List
==========

- All of the object write code
- All of the stream write code
- Page create method
- Write out trailer
- How to handle object references between documents, i.e., if I copy a page from
  one PDF to another, there are a bunch of resources that also need to be
  copied. A dictionary with an object reference can't be copied directly as the
  object number in the new PDF will likely be different than the old one.
    - Add _pdfio_map_t with original pdfio_file_t * and object numbers
    - Add _pdfioObjCopy function
    - Add _pdfioFileGetMappedObject function to get the new object number
- Security handlers (RC4 + AES, MD5 + SHA-256) for reading encrypted documents.
- Signature generation/validation code
- Documentation
- VS project
- Run through code analysis tools
