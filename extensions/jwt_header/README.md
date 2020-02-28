# WebAssembly

This plugin can be compiled and run via the Envoy WebAssembly support.

## Creating build Docker image.

Follow the instructions in the github.com/istio/envoy/api/wasm/cpp/README.md to build the WebAssembly Docker build image.

## Build via the Docker image.

```bash
./build_wasm.sh
```

# jwt_header filter

Jwt header filter reads a jwt token and populates header from claims based on configuration.
The headers that are populated according to configuration can be used in header based routing.
This filter is compiled as webassembly and loaded into Envoy.


`testdata/istio/jwt_filter.yaml` is the EnvoyFilter specification to add filters in a predefined way.
`testdata/istio/bookinfo/*` contain Istio configuration that exposes reviews and uses header based routing at ingress.


The Jwt header filter is desiged to specifically work with the jwt_authn filter. Jwt_authn filter validates jwt token and if valid
populates dynamic metadata. Jwt header filter reads the validated token from this place.

## How to deploy
* Update ingress-gateway deployment image with a build of proxy 1.5 that bundles in the wasm module.
  `gcr.io/mixologist-142215/proxyv2:1.5-jwtHeader`

* Deploy the filter at ingress using canned configuration.

`kubectl apply -f testdata/istio/jwt_filter.yaml`

The above configuration will deploy jwt validation filter using unsafe keys from testing@secure.istio.io issuer.
The configuration specifies that a header named `jwt-group` should be populated from the `group` claim in the validated jwt token.

`testdata/*_token.txt` files contain pre-generated tokens with group claim of canary, dev, and prod.

If you have deployed the `bookinfo` application you may try jwt based routing by running
`kubectl apply -f testdata/istio/bookinfo/` 


The virtual service maps the following jwt group claims to reviews versions.
Different review versions have slightly different json output.

```yaml
{prod: v1, canary: v2, dev: v3}
```
The following is an example testing session. 

```bash
$ export GW_IP=35.184.6.129
$ cd extensions/jwt_header/testdata (jwt_header_sample)
$ curl http://${GW_IP}/reviews/0  -H "Host: reviews.local" -H "Authorization: Bearer $(cat canary_token.txt)"
{"id": "0","reviews": [{  "reviewer": "Reviewer1",  "text": "An extremely entertaining play by Shakespeare. The slapstick humour is refreshing!", "rating": {"stars": 5, "color": "black"}},{  "reviewer": "Reviewer2",  "text": "Absolutely fun and entertaining. The play lacks thematic depth when compared to other plays by Shakespeare.", "rating": {"stars": 4, "color": "black"}}]}

$ curl http://${GW_IP}/reviews/0  -H "Host: reviews.local" -H "Authorization: Bearer $(cat dev_token.txt)"
{"id": "0","reviews": [{  "reviewer": "Reviewer1",  "text": "An extremely entertaining play by Shakespeare. The slapstick humour is refreshing!", "rating": {"stars": 5, "color": "red"}},{  "reviewer": "Reviewer2",  "text": "Absolutely fun and entertaining. The play lacks thematic depth when compared to other plays by Shakespeare.", "rating": {"stars": 4, "color": "red"}}]}

$ curl http://${GW_IP}/reviews/0  -H "Host: reviews.local" -H "Authorization: Bearer $(cat prod_token.txt)"
{"id": "0","reviews": [{  "reviewer": "Reviewer1",  "text": "An extremely entertaining play by Shakespeare. The slapstick humour is refreshing!"},{  "reviewer": "Reviewer2",  "text": "Absolutely fun and entertaining. The play lacks thematic depth when compared to other plays by Shakespeare."}]}
```


## Limitations
* Only supports accessing top level claims
* Does not support multi valued claims, like lists.
