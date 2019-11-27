# Authentication V2 filter

Istio Authentication V2 policy, TODO: link, is changed to only authenticate the request.

Authentication filter, thus also changed correspondingly to extract peer identity from X509 SAN
and `request.auth.xxx` attributes, put under `istio.authnv2` filter metadata.

- This filter never rejects the request.
- When multiple JWTs produced by envoy JWT filter, authentication filter selects by dictionary order
by Issuer to populate `request.authn.xxx` attributes.
