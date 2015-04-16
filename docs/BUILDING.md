# Building Neothyne

Neothyne only depends on SDL2. Make sure you have it installed correctly for your
tool chain and Neothyne should build out of the box.

## Linux
Linux users can built Neothyne from source by invoking make.
```
$ make
```

## Windows
Windows users need a working mingw toolchain with a version of gcc >= 4.9.2 or
a version of clang >= 3.6.0. Clang alone won't work as Neothyne also requires the
resource compiler from mingw to embed the icon. Compiling is done by invoking
make on the mingw makefile.
```
$ make -f Makefile.mingw
```

The compiler can be set with `CC=clang` if you prefer to use clang.
```
$ make -f Makefile.mingw CC=clang
```
You can optionally build Windows binaries on Linux using a cross compiler.
