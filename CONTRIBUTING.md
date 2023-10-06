Contributing to PDFio
=====================

PDFio is developed and distributed as open source software under the Apache
License, Version 2.0.  Contributions should be submitted as pull requests on
the Github site:

    http://github.com/michaelrsweet/pdfio/pulls


Contents
--------

- [Build System](#build-system)
- [Version Numbering](#version-numbering)
- [Coding Guidelines](#coding-guidelines)
  - [Source Files](#source-files)
  - [Header Files](#header-files)
  - [Comments](#comments)
  - [Indentation](#indentation)
  - [Spacing](#spacing)
  - [Return Values](#return-values)
  - [Functions](#functions)
  - [Variables](#variables)
  - [Types](#types)
  - [Structures](#structures)
  - [Constants](#constants)
- [Shell Script Guidelines](#shell-script-guidelines)
- [Makefile Guidelines](#makefile-guidelines)
  - [General Organization](#general-organization)
  - [Makefile Documentation](#makefile-documentation)
  - [Portable Makefile Construction](#portable-makefile-construction)
  - [Standard Variables](#standard-variables)
  - [Standard Targets](#standard-targets)
  - [Object Files](#object-files)
  - [Programs](#programs)
  - [Static Libraries](#static-libraries)
  - [Shared Libraries](#shared-libraries)
  - [Dependencies](#dependencies)
  - [Install/Uninstall Support](#installuninstall-support)


Build System
------------

The build system uses a simple POSIX makefile to build a static or shared
library.  To improve portability, makefiles *must not* make use of features
unique to GNU make.  See the [Makefile Guidelines](#makefile-guidelines) section
for a description of the allowed make features and makefile guidelines.

An Xcode project is provided for macOS/iOS developers, and a Visual Studio
solution and projects for Windows developers.


Version Numbering
-----------------

PDFio uses a three-part version number separated by periods to represent the
major, minor, and patch release numbers.  Major release numbers indicate large
design changes or backwards-incompatible changes to the library.  Minor release
numbers indicate new features and other smaller changes which are backwards-
compatible with previous releases.  Patch numbers indicate bug fixes to the
previous feature or patch release.

Production releases use the plain version numbers:

    MAJOR.MINOR.PATCH
    1.0.0
    1.0.1
    1.0.2
    ...
    1.1.0
    ...
    2.0.0

The first production release in a MAJOR.MINOR series (MAJOR.MINOR.0) is called
a feature release.  Feature releases are the only releases that may contain new
features.  Subsequent production releases in a MAJOR.MINOR series may only
contain bug fixes.

Beta-test releases are identified by appending the letter B to the major and
minor version numbers followed by the beta release number:

    MAJOR.MINORbNUMBER
    1.0b1

Release candidates are identified by appending the letters RC to the major and
minor version numbers followed by the release candidate number:

    MAJOR.MINORrcNUMBER
    1.0rc1

> Note: While the beta/release candidate syntax is *not* strictly compatible
> with [Semantic Versioning](https://semver.org), it is better supported by the
> various traditional package formats. When packaging a pre-release version of
> PDFio in a format that requires the use of semantic version numbers, the
> version number should simply be converted to the form "MAJOR.MINOR.0-suffix".


Coding Guidelines
-----------------

Contributed source code must follow the guidelines below.  While the examples
are for C source files, source code for other languages should conform to the
same guidelines as allowed by the language.


### Source Files

All source files names must be 16 characters or less in length to ensure
compatibility with older UNIX filesystems.  Source files containing functions
have an extension of ".c" for C source files.  All "include" files have an
extension of ".h".  Tabs are set to 8 characters or columns.

The top of each source file contains a header giving the purpose or nature of
the source file and the copyright and licensing notice:

    //
    // Description of file contents.
    //
    // Copyright © YYYY by AUTHOR.
    //
    // Licensed under Apache License v2.0.  See the file "LICENSE" for more
    // information.
    //


### Header Files

Private API header files must be named with the suffix "-private", for example
the "pdfio.h" header file defines all of the public APIs while the
"pdfio-private.h" header file defines all of the private APIs.  Typically a
private API header file will include the corresponding public API header file.


### Comments

All source code utilizes block comments within functions to describe the
operations being performed by a group of statements; avoid putting a comment
per line unless absolutely necessary, and then consider refactoring the code
so that it is not necessary.  C source files use the C99 comment format
("// comment"):

    // Clear the state array before we begin...
    for (i = 0; i < (sizeof(array) / sizeof(sizeof(array[0])); i ++)
      array[i] = PDFIO_STATE_IDLE;

    // Wait for state changes on another thread...
    do
    {
      for (i = 0; i < (sizeof(array) / sizeof(sizeof(array[0])); i ++)
        if (array[i] != PDFIO_STATE_IDLE)
          break;

      if (i == (sizeof(array) / sizeof(array[0])))
        sleep(1);
    } while (i == (sizeof(array) / sizeof(array[0])));


### Indentation

All code blocks enclosed by brackets begin with the opening brace on a new
line.  The code then follows starting on a new line after the brace and is
indented 2 spaces.  The closing brace is then placed on a new line following
the code at the original indentation:

    {
      int i; // Looping var

      // Process foobar values from 0 to 999...
      for (i = 0; i < 1000; i ++)
      {
        do_this(i);
        do_that(i);
      }
    }

Single-line statements following "do", "else", "for", "if", and "while" are
indented 2 spaces as well.  Blocks of code in a "switch" block are indented 4
spaces after each "case" and "default" case:

    switch (array[i])
    {
      case PDFIO_STATE_IDLE :
          do_this(i);
          do_that(i);
          break;

      default :
          do_nothing(i);
          break;
    }


### Spacing

A space follows each reserved word such as `if`, `while`, etc.  Spaces are not
inserted between a function name and the arguments in parenthesis.


### Return Values

Parenthesis surround values returned from a function:

    return (PDFIO_STATE_IDLE);


### Functions

Functions with a global scope have a lowercase prefix followed by capitalized
words, e.g., `pdfioDoThis`, `pdfioDoThat`, `pdfioDoSomethingElse`, etc.  Private
global functions begin with a leading underscore, e.g., `_pdfioDoThis`,
`_pdfioDoThat`, etc.

Functions with a local scope are declared static with lowercase names and
underscores between words, e.g., `do_this`, `do_that`, `do_something_else`, etc.

Function names follow the following pattern:

- "pdfioFooCreate" to create a Foo object,
- "pdfioFooDelete" to destroy (free) a Foo object,
- "pdfioFooGetBar" to get data element Bar from object Foo,
- "pdfioFooIsBar" to test condition Bar for object Foo, and
- "pdfioFooSetBar" to set data element Bar in object Foo.
- "pdfioFooVerb" to take an action with object Foo.

Each function begins with a comment header describing what the function does,
the possible input limits (if any), the possible output values (if any), and
any special information needed:

    //
    // 'pdfioDoThis()' - Short description of function.
    //
    // Longer documentation for function with examples using a subset of
    // markdown.  This is a bulleted list:
    //
    // - One fish
    // - Two fish
    // - Red fish
    // - Blue fish
    //
    // > *Note:* Special notes for developer should be markdown block quotes.
    //

    float                  // O - Inverse power value, 0.0 <= y <= 1.1
    pdfioDoThis(float x)   // I - Power value (0.0 <= x <= 1.1)
    {
      ...
      return (y);
    }

Return/output values are indicated using an "O" prefix, input values are
indicated using the "I" prefix, and values that are both input and output use
the "IO" prefix for the corresponding in-line comment.

The [`codedoc` documentation generator][1] also understands the following
special text in the function description comment:

    @deprecated@         - Marks the function as deprecated (not recommended
                           for new development and scheduled for removal)
    @since version@      - Marks the function as new in the specified version.
    @private@            - Marks the function as private (same as starting the
                           function name with an underscore)

[1]: https://www.msweet.org/codedoc


### Variables

Variables with a global scope are capitalized, e.g., `ThisVariable`,
`ThatVariable`, `ThisStateVariable`, etc.  Globals *must not* be used in the
PDFio library.

Variables with a local scope are lowercase with underscores between words,
e.g., `this_variable`, `that_variable`, etc.  Any "local global" variables
shared by functions within a source file are declared static.

Each variable is declared on a separate line and is immediately followed by a
comment block describing the variable:

    int         ThisVariable;    // The current state of this
    static int  that_variable;   // The current state of that


### Types

All type names are lowercase with underscores between words and `_t` appended
to the end of the name, e.g., `pdfio_this_type_t`, `pdfio_that_type_t`, etc.
Type names start with the "pdfio\_" prefix to avoid conflicts with system types.
Private type names start with an underscore, e.g., `_pdfio_this_t`,
`_pdfio_that_t`, etc.

Each type has a comment block immediately after the typedef:

    typedef int pdfio_this_type_t;  // This type is for foobar options.


### Structures

All structure names are lowercase with underscores between words and `_s`
appended to the end of the name, e.g., `pdfio_this_s`, `pdfio_that_s`, etc.
Structure names start with the "pdfio\_" prefix to avoid conflicts with system
types.  Private structure names start with an underscore, e.g., `_pdfio_this_s`,
`_pdfio_that_s`, etc.

Each structure has a comment block immediately after the struct and each member
is documented similar to the variable naming policy above:

    struct pdfio_this_struct_s // This structure is for foobar options.
    {
      int this_member;         // Current state for this
      int that_member;         // Current state for that
    };

One common design pattern is to define a private structure with a public
typedef, for example:

    // In public header
    typedef struct _pdfio_foo_s pdfio_foo_t // Foo object

    // In private header
    struct _pdfio_foo_s        // Foo object
    {
      int this_member;         // Current state for this
      int that_member;         // Current state for that
    };


### Constants

All constant names are uppercase with underscores between words, e.g.,
`PDFIO_THIS_CONSTANT`, `PDFIO_THAT_CONSTANT`, etc.  Constants begin with the
"PDFIO\_" prefix to avoid conflicts with system constants.  Private constants
start with an underscore, e.g., `_PDFIO_THIS_CONSTANT`,
`_PDFIO_THAT_CONSTANT`, etc.

Typed enumerations should be used whenever possible to allow for type checking
by the compiler.  The constants for typed enumerations must match the type name
in uppercase, for example a `pdfio_foo_e` enumeration has constant names
starting with `PDFIO_FOO_`.

Comment blocks immediately follow each constant:

    typedef enum pdfio_style_e  // Style enumerations
    {
      PDFIO_STYLE_THIS,         // This style
      PDFIO_STYLE_THAT          // That style
    } pdfio_style_t;


Shell Script Guidelines
-----------------------

All shell scripts in PDFio must conform to the [POSIX shell][POSIX-SHELL]
command language and should restrict their dependence on non-POSIX utility
commands.

[POSIX-SHELL]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18


Makefile Guidelines
-------------------

PDFio uses a single [POSIX makefile][POSIX-MAKE] to build it.  GNU make
extensions MUST NOT be used.

[POSIX-MAKE]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/make.html

The following variables are defined in the makefile:

- `AR`; the static library archiver command,
- `ARFLAGS`; options for the static library archiver,
- `CC`; the C compiler command,
- `CFLAGS`; options for the C compiler,
- `CODESIGN_IDENTITY`: the code signing identity,
- `COMMONFLAGS`; common compiler optimization options,
- `CPPFLAGS`; options for the C preprocessor,
- `DESTDIR`/`DSTROOT`: the destination root directory when installing.
- `DSO`; the shared library building command,
- `DSOFLAGS`; options for the shared library building command,
- `DSONAME`: the root name of the shared library
- `LDFLAGS`; options for the linker,
- `LIBS`; libraries for all programs,
- `prefix`; the installation prefix directory,
- `RANLIB`; the static library indexing command,
- `SHELL`; the sh (POSIX shell) command,
- `VERSION`: the library version number.

The following standard targets are defined in the makefile:

- `all`; creates the static library and unit test program.
- `all-shared`; creates a shared library appropriate for the local system.
- `clean`; removes all target programs libraries, documentation files, and
  object files,
- `debug`: creates a clean build of the static library and unit test program
  with debug printfs and the clang address sanitizer enabled.
- `install`; installs all distribution files in their corresponding locations.
- `install-shared`; same as `install` but also installs the shared library.
- `macos`; same as `all` but creates a Universal Binary (X64 + ARM64).
- `test`; runs the unit test program, building it as needed.
