# libcircllhist

[![Build Status](https://travis-ci.org/openhistogram/libcircllhist.svg?branch=master)](https://travis-ci.org/openhistogram/libcircllhist)

An implementation of OpenHistogram log-linear histograms.

## Requirements

* C compiler (gcc, clang, etc.)
* GNU Autoconf
* GNU Make

Additionally, to run tests and/or use the bindings:
* [LuaJIT](http://luajit.org/luajit.html)
* Python 2.7+ or 3.x (including `setuptools` and `cffi`)

## Installing

    autoconf
    ./configure
    make
    make install

To install to a directory prefix other than the default `/usr/local`, use the
`--prefix=<path>` option with configure.

## Tests

    make tests

## Documentation

The circllhist API is documented in the header file [circllhist.h](src/circllhist.h).
A html version can be generated using:

    make docs

This requires doxygen to be available on your system.

## Python bindings

This repository contains [Python bindings](/src/python) for libcircllhist.
Build and install the C library as described above. Then use the following
commands to install the python library.

    cd src/python
    python setup.py install

## Lua bindings

This repository contains [Lua bindings](/src/lua) for libcircllhist, which are
built by default and will be installed by `make install` to the directory
`(PREFIX)/share/lua/5.1/`.

## FAQ

### After the installation, libcircllhist.so can not be found by my program / the python bindings. How can I fix this?

One possible source for this problem is that libcircllhist is not installed
into a path where the linker can find it. The default prefix is `/usr/local`,
which is in the default `ld.so` search path for UNIX and most Linux
distributions.  To specify an alternate library directory, use
`--libdir=<path>` with configure. For example:

    ./configure --libdir=/usr/local/lib64
