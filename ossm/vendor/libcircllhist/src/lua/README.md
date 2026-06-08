# Lua bindings for libcircllhist

The lua bindings are installed automatically as part of the base [libcircllhist](/) install.

After a successful `make install` the lua bindings will be placed in `/usr/local/share/lua/5.1/`.

## Usage Example

```
local h = Circllhist:new()
h.insert(1)
h.insert(2)
local h2 = Circllhist.from_data({1,2})
assert(h:isequal(h2))
```
