# Clippy Tests

This directory tests integration of the Clippy static analyzer aspect.

It is split into a couple different directories:

* [src](src) contains a simple binary, library and test which should compile
successfully when built normally, and pass without error when analyzed
with clippy.
* [bad\_src](bad_src) also contains a binary, library, and test which compile
normally, but which intentionally contain "junk code" which will trigger
Clippy lints.
