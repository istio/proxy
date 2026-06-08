# Python bindings for libcircllhist

This package requires the [OpenHistogram](https://openhistogram.io) C library,
libcircllhist, to be installed on your system:

https://github.com/openhistogram/libcircllhist/

The bindings themselves can be installed via pip:

    pip install circllhist

Or manually via

    python setup.py install

from this folder.

## Usage Example

```
import circllhist

h = circllhist.Circllhist()
h.insert(123,3)        # Insert value 123, three times
h.insert_intscale(1,1) # Insert 1x10^1
print(h.count())       # Print the count of used bins
print(h.sum())         # Print the sum of all values
```
