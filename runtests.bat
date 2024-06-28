:: Script to run unit test program
::
:: Usage:
::
::   .\runtests.bat x64\{Debug|Release}
::

:: Copy dependent DLLs to the named build directory
echo Copying DLLs
copy packages\zlib_native.redist.1.2.11\build\native\bin\x64\Debug\*.dll %1
copy packages\zlib_native.redist.1.2.11\build\native\bin\x64\Release\*.dll %1

:: Run unit test program
echo Running %1\testpdfio.exe
cd %1
testpdfio.exe
