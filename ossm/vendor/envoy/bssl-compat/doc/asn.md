# Implementation of ASN Functions

## asn1_string_st
The structure asn1_string_st is not opaque but ossl_asn1_string_st is identical in terms of fields and types. It is assumed that the semantics of these fields are identical between BoringSSL and OpenSSL, then we get away with the simple typedef mapping.

