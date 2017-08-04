##How to recieve JWT

In HTTP Authorization header
- `Authorization: Bearer <JWT>` 	

##How to pass payload
Add some HTTP header `<hoge>: <content>`
- `<hoge> = Istio-Auth-UserInfo`
- `<content>` = 
    - `<payload_json_string>` or
    - `<payload_json_string_base64urlEncoded>` or
    - `<raw JWT without signature>`

##How to receive config of issuer / pubkey(s)
For each issuer, one of them is required:
- discovery document
  - via uri or raw string 
- issuer's name & public key
  - public key: via uri or raw string
  - public key's format: JWK (, pem, ...)


###How

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
     “discovery_document”: {
       “uri”: <uri of document>, 
       “value”: <raw string of document>,
     },
     “name”: <string of issuer name>,
     “pubkey”: {
        “type”: <type>, 
        “uri”: <uri>, 
        “value”: <raw string of public key(s)>,
     }, 
   },
   ...
 ]
}
```
Where
- `discovery_document` or (`name` and `pubkey`) is required
- If `discovery_document` is used, `uri` or `value` is required
- If `pubkey` is used,
  - `type` is required
    - `<type>` is in `{"jwks" (, "pem", ...)}`
  - `uri` or `value` is required
