Building, Testing, and Installing PDFio
=======================================

This file describes how to compile, test, and install the PDFio library from
source code.  For more information on PDFio see the file called `README.md`.


Getting the Code
----------------

> Note: Do not use the ZIP file available via the Github "Code" button on the
> main project page, as that archive is missing the TTF submodule and will not
> compile.

The source code is available in release tarballs or via the Github repository.
For a release tarball, run the following commands:

    tar xvzf pdfio-VERSION.tar.gz
    cd pdfio-VERSION

Similarly, the release ZIP file can be extracted with the following commands:

    unzip pdfio-VERSION.zip
    cd pdfio-VERSION

From the Github sources, clone the repository with the `--recurse-submodules`
option *or* use the `git submodule` command:

    git clone --recurse-submodules git@github.com:michaelrsweet/pdfio.git
    cd pdfio

    git clone git@github.com:michaelrsweet/pdfio.git
    cd pdfio
    git submodule update --init --recursive

To update an already-cloned repository:

    git pull
    git submodule update --init --recursive


Requirements
------------

PDFio requires the following to build the software:

- A C99 compiler such as Clang, GCC, or MS Visual C
- A POSIX-compliant `make` program
- A POSIX-compliant `sh` program
- libpng (<https://www.libpng.org/>) 1.6 or later for full PNG image support
  (optional)
- libwebp (<https://developers.google.com/speed/webp>) 1.0 or later for WebP
  image support (optional)
- ZLIB (<https://www.zlib.net/>) 1.1 or later for compression support

IDE files for Xcode (macOS/iOS) and Visual Studio (Windows) are also provided.

On a stock Ubuntu install, the following command will install the various
prerequisites:

    sudo apt-get install build-essential libpng-dev libwebp-dev zlib1g-dev


Building and Installing on Linux/Unix
-------------------------------------

PDFio uses a configure script on Unix systems to generate a makefile:

    ./configure

If you want a shared library, run:

    ./configure --enable-shared

The default installation location is "/usr/local".  Pass the `--prefix` option
to make to install it to another location:

    ./configure --prefix=/some/other/directory

Other configure options can be discovered by using the `--help` option:

    ./configure --help

Once configured, run the following command to build the library:

    make all

Finally, run the following command to install the library, documentation, and
examples on the local system:

    sudo make install

Use the `BUILDROOT` variable to install to an alternate root directory:

    sudo make BUILDROOT=/some/other/root/directory install

> Note: PDFio also supports the GNU `DSTROOT`, Apple/Darwin `DESTDIR`, and
> Red Hat `RPM_BUILD_ROOT` variables to specify an alternate root directory.


### Running the Unit Tests

PDFio includes a unit test program that thoroughly tests the library.  Type the
following to run the unit tests:

    make test

Detailed test results are saved to the "test.log" file.


Building on Windows with Visual Studio
--------------------------------------

The Visual Studio solution (`pdfio.sln`) is provided for Windows developers and
generates the PDFIO1 DLL.  You can also use NuGet to install the `pdfio_native`
package.

You can build and run the `testpdfio` unit tests target from within Visual
Studio to verify the functioning of the library.


Building on macOS with Xcode
----------------------------

The Xcode project (`pdfio.xcodeproj`) is provided for macOS developer which
generates a static library that will be installed under "/usr/local".  Run the
following command to build and install PDFio on macOS:

    sudo xcodebuild install

Alternately you can add the Xcode project to a workspace and reference the PDFio
library target as a dependency in your own project.

You can build and run the `testpdfio` unit tests target from within Xcode to
verify the functioning of the library.
