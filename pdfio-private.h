//
// Private header file for pdfio.
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef PDFIO_PRIVATE_H
#  define PDFIO_PRIVATE_H

//
// Include necessary headers...
//


//
// Visibility and other annotations...
//

#  if defined(__has_extension) || defined(__GNUC__)
#    define PDFIO_INTERNAL	__attribute__ ((visibility("hidden")))
#    define PDFIO_PRIVATE	__attribute__ ((visibility("default")))
#    define PDFIO_NONNULL(...)	__attribute__ ((nonnull(__VA_ARGS__)))
#    define PDFIO_NORETURN	__attribute__ ((noreturn))
#  else
#    define PDFIO_INTERNAL
#    define PDFIO_PRIVATE
#    define PDFIO_NONNULL(...)
#    define PDFIO_NORETURN
#  endif // __has_extension || __GNUC__


//
// Debug macro...
//

#  ifdef DEBUG
#    define PDFIO_DEBUG(...)	fprintf(stderr, __VA_ARGS__)
#  else
#    define PDFIO_DEBUG(...)
#  endif // DEBUG


//
// Types and constants...
//


//
// Functions...
//


#endif // !PDFIO_PRIVATE_H
