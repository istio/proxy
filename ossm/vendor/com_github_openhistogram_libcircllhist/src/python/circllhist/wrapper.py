"""
The circllhist python module provides a wrapper around the libcircllhist data structures
"""

import sys
import json
import math
import circllhist.ffi as ffi

if sys.version_info[0] == 3:
    str_ascii = lambda b: str(b, "ASCII")
else:
    str_ascii = str

class Circllbin(object):
    "Wraps a histogram bin"

    __slots__ = ("_b",)

    def __init__(self, b):
        self._b = b

    @classmethod
    def from_number(cls, n):
        return cls(ffi.C.double_to_hist_bucket(n))

    def __str__(self):
        buf = ffi.ffi.new("char[]", 9)
        ffi.C.hist_bucket_to_string(self._b, buf)
        return str_ascii(ffi.ffi.string(buf))

    @property
    def width(self):
        return ffi.C.hist_bucket_to_double_bin_width(self._b)

    @property
    def midpoint(self):
        return ffi.C.hist_bucket_midpoint(self._b)

    @property
    def edge(self):
        "Returns the edge of the histogram bucket that is closer to zero"
        return ffi.C.hist_bucket_to_double(self._b)

    @property
    def exp(self):
        return self._b.exp

    @property
    def val(self):
        return self._b.val

class Circllhist(object):
    "Wraps a log-linear histogram"

    __slots__ = ("_h",)

    def __init__(self, h=None, gc=False):
        if h:
            if gc:
                self._h = ffi.ffi.gc(h, ffi.C.hist_free)
            else:
                self._h = h
        else:
            self._h = ffi.ffi.gc(ffi.C.hist_alloc(), ffi.C.hist_free)

    def count(self):
        "Returns the number of samples stored in the histogram"
        return ffi.C.hist_sample_count(self._h)

    def bin_count(self):
        "Returns the number of bins used by the histogram"
        return ffi.C.hist_bucket_count(self._h)

    def insert(self, value, count=1):
        "Insert a value into the histogram"
        ffi.C.hist_insert(self._h, value, count)

    def insert_intscale(self, val, scale, count=1):
        "Insert a value of val * 10^scale into this histogram. Use this if you can."
        ffi.C.hist_insert_intscale(self._h, val, scale, count)

    def merge(self, hist):
        "Merge the given histogram into self."
        p = ffi.ffi.new("histogram_t **")
        p[0] = hist._h
        ffi.C.hist_accumulate(self._h, p, 1)

    def clear(self):
        "Clear data. fast. Keep allocations."
        ffi.C.hist_clear(self._h)

    def __iter__(self):
        count = self.bin_count()
        for i in range(count):
            b = ffi.ffi.new("hist_bucket_t*")
            c = ffi.ffi.new("uint64_t*")
            ffi.C.hist_bucket_idx_bucket(self._h, i, b, c)
            yield (Circllbin(b[0]), c[0])

    def mean(self):
        "Retuns an approximation of the mean value"
        return ffi.C.hist_approx_mean(self._h)

    def sum(self):
        "Returns an approximation of the sum"
        return ffi.C.hist_approx_sum(self._h)

    def stddev(self):
        "Returns an approximation of the standard deviation"
        return ffi.C.hist_approx_stddev(self._h)

    def moment(self, k):
        "Returns an approximation of the k-th moment"
        return ffi.C.hist_approx_moment(self._h, k)

    def count_below(self, threshold):
        "Returns the number of values in buckets that are entirely lower than or equal to threshold"
        return ffi.C.hist_approx_count_below(self._h, threshold)

    def count_above(self, threshold):
        "Returns the number of values in buckets that are entirely larger than or equal to threshold"
        return ffi.C.hist_approx_count_above(self._h, threshold)

    def count_nearby(self, value):
        "Returns the number of samples in the histogram that are in the same bucket as the provided value"
        return ffi.C.hist_approx_count_nearby(self._h, value)

    def quantile(self, q):
        "Returns an approximation of the q-quantile"
        q_in = ffi.ffi.new("double*", q)
        q_out = ffi.ffi.new("double*")
        ffi.C.hist_approx_quantile(self._h, q_in, 1, q_out)
        return q_out[0]

    def to_dict(self):
        "Use this to generate JSON output"
        d = {}
        for b, c in self:
            d[str(b)] = c
        return d

    @classmethod
    def from_dict(cls, d):
        "Create a histogram from a dict of the form bin => count"
        h = cls()
        for k, v in d.items():
            h.insert(float(k), v)
        return h

    def to_b64(self):
        "Returns a base64 encoded binary representation of the histogram"
        sz = ffi.C.hist_serialize_b64_estimate(self._h)
        buf = ffi.ffi.new("char[]", sz)
        ffi.C.hist_serialize_b64(self._h, buf, sz)
        return str_ascii(ffi.ffi.string(buf))

    @classmethod
    def from_b64(cls, b64):
        "Create from a binary base64 encoded string"
        h = cls()
        buf = b64.encode("ASCII")
        sz = len(buf)
        ffi.C.hist_deserialize_b64(h._h, buf, sz)
        return h

    def __str__(self):
        return json.dumps(self.to_dict())

    def compress_mbe(self, mbe):
        """
        Compress histogram by squshing together adjacent buckets

        This compression is lossy. mean/quantiles will be affected by compression.
        Intended use cases is visualization.
        - mbe the Minimum Bucket Exponent
        - return the compressed histogram as new value
        """
        return Circllhist(ffi.C.hist_compress_mbe(self._h, int(mbe)), gc=True)

    def plot(self, **kwargs):
        """
        Plot histogram using matplotlib.
        Depends on matplotlib being available.
        Returns an axes object.

        Hint: Use H.plot().set_xlim(x_min, x_max) to adjust value range.

        mbe-compression is applied before plotting to squash bins which are deemed too small.
        Use the mbe parameter to tune the minimal bin exponent.
        """
        # Other functions will work fine without having matplotlib installed
        # so we keep the import local to this function.
        from matplotlib import pyplot as plt
        ax = kwargs.pop('ax', None)
        mbe = kwargs.pop('mbe', None)
        d_min = self.quantile(.1)
        d_max = self.quantile(.9)
        d_range = d_max - d_min
        if not mbe:
            mbe = math.floor(math.log10(d_range/100))
        mb = 10 ** mbe
        H = self.compress_mbe(mbe)
        if not ax:
            x_min = d_min - 0.10*d_range - mb
            x_max = d_max + 0.10*d_range + mb
            ax = plt.subplot(1,1,1)
            ax.set_xlim(x_min, x_max)
        x=[] # midpoints
        h=[] # height
        w=[] # widths
        for b, c in H:
            c = float(c)
            if b.exp == 0:
                wdt = 2*mb # 0 bucket, spans -mb ... +mb
                x.append(0)
                w.append(wdt)
                h.append(c/wdt)
            elif b.exp == mbe:
                # mbe buckets are scaled by a factor of 10
                sgn = 1 if b.val > 0 else -1
                wdt = b.width * 10
                mid = b.edge + sgn * wdt/2
                x.append(mid)
                w.append(wdt)
                h.append(c/wdt)
            else:
                x.append(b.midpoint)
                w.append(b.width)
                h.append(c/b.width)
        ax.bar(x, h, w, **kwargs)
        return ax
