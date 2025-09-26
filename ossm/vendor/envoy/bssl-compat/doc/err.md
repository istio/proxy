
## ERR_GET_LIB()

The `ERR_GET_LIB()` function returns one of the `ERR_LIB_*` values, to indicate where an error came from.

In BoringSSL, the `ERR_LIB_*` values are defined as enumerators, whereas in OpenSSL they are defined as macros, but more importantly, their values do not all match, so some translation is required.

To implement `ERR_GET_LIB()` we patch `<openssl/err.h>` to change it from being a macro definition, to being a function declaration. Then, the `generate_ERR_GET_LIB().py` script is used to generate the implementation.

## ERR_GET_REASON()

The implementation of `ERR_GET_REASON()` follows the same pattern as `ERR_GET_LIB()` above.