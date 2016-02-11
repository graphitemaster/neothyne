# Building Neothyne

Neothyne only depends on SDL2. Make sure you have it installed correctly for your
tool chain and Neothyne should build out of the box.

## Linux
Linux users can build Neothyne from source by invoking make.
```
$ make
```

## BSD
BSD users can build Neothyne from source by invoking gmake.
```
$ gmake
```

## Windows
Windows users have a couple methods for building Neothyne.

### Visual Studio
The preferred method for binaries is to use the included Visual Studio
solution with Visual Studio 2015 or higher.

Visual Studio 2015 project files are setup to produce binaries
using a relatively new C run time. If you need binaries on older versions
of Windows, change the runtime library or use the alternative method.

### MingW
The alternative method is to use a MingW toolchain with a version of gcc >= 4.9.2
Optionally you can use a version of clang >= 3.6.0 in place of gcc
```
$ make -f Makefile.mingw
```

The compiler can be set with `CC=clang` if you prefer to use clang.
```
$ make -f Makefile.mingw CC=clang
```
You can optionally build Windows binaries on Linux using a cross compiler
using this Makefile
