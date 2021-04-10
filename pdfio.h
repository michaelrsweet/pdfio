//
// Public header file for pdfio.
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef PDFIO_H
#  define PDFIO_H

//
// Include necessary headers...
//


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Visibility and other annotations...
//

#  if defined(__has_extension) || defined(__GNUC__)
#    define PDFIO_PUBLIC	__attribute__ ((visibility("default")))
#    define PDFIO_FORMAT(a,b)	__attribute__ ((__format__(__printf__, a,b)))
#  else
#    define PDFIO_PUBLIC
#    define PDFIO_FORMAT(a,b)
#  endif // __has_extension || __GNUC__


//
// Types and constants...
//


//
// Functions...
//


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !PDFIO_H
