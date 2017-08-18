## How to recieve JWT

In HTTP Authorization header
- `Authorization: Bearer <JWT>` 	

## How to pass payload
Add an HTTP header `Istio-Auth-UserInfo: <content>`
- `<content>` = 
    - `<payload_json_string>` or
    - `<payload_json_string_base64urlEncoded>` or
    - `<raw JWT without signature>`

## How to receive config of issuer / pubkey(s)
For each issuer, one of them is required:
- discovery document
  - via uri or raw string 
- issuer's name & public key
  - public key: via uri or raw string
  - public key's format: JWK (, pem, ...)


### Config format

In Envoy config,
```
{
 "type": "decoder",
 "name": "jwt-auth",
 "config": {<config>}
},
```

Format of `<config>`:
```
{
 “issuers”:[
   {
     “name”: <string of issuer name>,
     “pubkey”: {
        “type”: <type>, 
        “uri”: <uri>, 
        "cluster": <name of cluster>,
        “value”: <raw string of public key(s)>,
     }, 
   },
   ...
 ]
}
```
Here
- `name` and `pubkey` are required
- For `pubkey`,
  - `type` is required
    - `<type>` is in `{"jwks" (, "pem", ...)}`
  - (`uri` and `cluster`) or `value` is required
