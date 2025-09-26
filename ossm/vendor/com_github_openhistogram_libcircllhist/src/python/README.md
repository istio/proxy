# Python bindings for libcircllhist

This package requires the libcircllhist C library to be installed on your system:

https://github.com/circonus-labs/libcircllhist/

The bindings itself can be installed via pip:

    pip install circllhist

Or manually via

    python setup.py install

from this folder.

## Usage Example

```
h = Circllhist()
h.insert(123,3)        # Insert value 123, three times
h.insert_intscale(1,1) # Insert 1x10^1
print(h.count())
print(h.sum())
```
