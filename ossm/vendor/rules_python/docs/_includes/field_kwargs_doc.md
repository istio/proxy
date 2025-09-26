:::{field} kwargs
:type: dict[str, Any]

Additional kwargs to use when building. This is to allow manipulations that
aren't directly supported by the builder's API. The state of this dict
may or may not reflect prior API calls, and subsequent API calls may
modify this dict. The general contract is that modifications to this will
be respected when `build()` is called, assuming there were no API calls
in between.
:::

