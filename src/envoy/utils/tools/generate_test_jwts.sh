#!/usr/bin/env bash
set -e
RSA_KEY_FILE1=/tmp/istio/proxy1.rsa.pem
RSA_KEY_FILE2=/tmp/istio/proxy2.rsa.pem
EC_KEY_FILE1=/tmp/istio/proxy1.ec.pem
mkdir -p /tmp/istio
cat > ${RSA_KEY_FILE1} <<EOF
-----BEGIN RSA PRIVATE KEY-----
MIIEowIBAAKCAQEAtw7MNxUTxmzWROCD5BqJxmzT7xqc9KsnAjbXCoqEEHDx4WBl
fcwkXHt9e/2+Uwi3Arz3FOMNKwGGlbr7clBY3utsjUs8BTF0kO/poAmSTdSuGeh2
mSbcVHvmQ7X/kichWwx5Qj0Xj4REU3Gixu1gQIr3GATPAIULo5lj/ebOGAa+l0wI
G80Nzz1pBtTIUx68xs5ZGe7cIJ7E8n4pMX10eeuh36h+aossePeuHulYmjr4N0/1
jG7a+hHYL6nqwOR3ej0VqCTLS0OloC0LuCpLV7CnSpwbp2Qg/c+MDzQ0TH8g8drI
zR5hFe9a3NlNRMXgUU5RqbLnR9zfXr7b9oEszQIDAQABAoIBAQCgQQ8cRZJrSkqG
P7qWzXjBwfIDR1wSgWcD9DhrXPniXs4RzM7swvMuF1myW1/r1xxIBF+V5HNZq9tD
Z07LM3WpqZX9V9iyfyoZ3D29QcPX6RGFUtHIn5GRUGoz6rdTHnh/+bqJ92uR02vx
VPD4j0SNHFrWpxcE0HRxA07bLtxLgNbzXRNmzAB1eKMcrTu/W9Q1zI1opbsQbHbA
CjbPEdt8INi9ij7d+XRO6xsnM20KgeuKx1lFebYN9TKGEEx8BCGINOEyWx1lLhsm
V6S0XGVwWYdo2ulMWO9M0lNYPzX3AnluDVb3e1Yq2aZ1r7t/GrnGDILA1N2KrAEb
AAKHmYNNAoGBAPAv9qJqf4CP3tVDdto9273DA4Mp4Kjd6lio5CaF8jd/4552T3UK
N0Q7N6xaWbRYi6xsCZymC4/6DhmLG/vzZOOhHkTsvLshP81IYpWwjm4rF6BfCSl7
ip+1z8qonrElxes68+vc1mNhor6GGsxyGe0C18+KzpQ0fEB5J4p0OHGnAoGBAMMb
/fpr6FxXcjUgZzRlxHx1HriN6r8Jkzc+wAcQXWyPUOD8OFLcRuvikQ16sa+SlN4E
HfhbFn17ABsikUAIVh0pPkHqMsrGFxDn9JrORXUpNhLdBHa6ZH+we8yUe4G0X4Mc
R7c8OT26p2zMg5uqz7bQ1nJ/YWlP4nLqIytehnRrAoGAT6Rn0JUlsBiEmAylxVoL
mhGnAYAKWZQ0F6/w7wEtPs/uRuYOFM4NY1eLb2AKLK3LqqGsUkAQx23v7PJelh2v
z3bmVY52SkqNIGGnJuGDaO5rCCdbH2EypyCfRSDCdhUDWquSpBv3Dr8aOri2/CG9
jQSLUOtC8ouww6Qow1UkPjMCgYB8kTicU5ysqCAAj0mVCIxkMZqFlgYUJhbZpLSR
Tf93uiCXJDEJph2ZqLOXeYhMYjetb896qx02y/sLWAyIZ0ojoBthlhcLo2FCp/Vh
iOSLot4lOPsKmoJji9fei8Y2z2RTnxCiik65fJw8OG6mSm4HeFoSDAWzaQ9Y8ue1
XspVNQKBgAiHh4QfiFbgyFOlKdfcq7Scq98MA3mlmFeTx4Epe0A9xxhjbLrn362+
ZSCUhkdYkVkly4QVYHJ6Idzk47uUfEC6WlLEAnjKf9LD8vMmZ14yWR2CingYTIY1
LL2jMkSYEJx102t2088meCuJzEsF3BzEWOP8RfbFlciT7FFVeiM4
-----END RSA PRIVATE KEY-----
EOF

cat > ${RSA_KEY_FILE2} <<EOF
-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCoOLtPHgOE289C
yXWh/HFzZ49AVyz4vSZdijpMZLrgJj/ZaY629iVws1mOG511lVXZfzybQx/BpIDX
rAT5GIoz2GqjkRjwE9ePnsIyJgDKIe5A+nXJrKMyCgTU/aO+nh6oX4FOKWUYm3lb
lG5e2L26p8y0JB1qAHwQLcw1G5T8p14uAHLeVLeijgs5h37viREFVluTbCeaZvsi
E/06gtzX7v72pTW6GkPGYTonAFq7SYNLAydgNLgb8wvXt0L5kO0t3WLbhJNTDf0o
fSlxJ18VsvY20Rl015qbUMN2TSJS0lI9mWJQckEj+mPwz7Yyf+gDyMG4jxgrAGpi
RkI3Uj3lAgMBAAECggEAOuaaVyp4KvXYDVeC07QTeUgCdZHQkkuQemIi5YrDkCZ0
Zsi6CsAG/f4eVk6/BGPEioItk2OeY+wYnOuDVkDMazjUpe7xH2ajLIt3DZ4W2q+k
v6WyxmmnPqcZaAZjZiPxMh02pkqCNmqBxJolRxp23DtSxqR6lBoVVojinpnIwem6
xyUl65u0mvlluMLCbKeGW/K9bGxT+qd3qWtYFLo5C3qQscXH4L0m96AjGgHUYW6M
Ffs94ETNfHjqICbyvXOklabSVYenXVRL24TOKIHWkywhi1wW+Q6zHDADSdDVYw5l
DaXz7nMzJ2X7cuRP9zrPpxByCYUZeJDqej0Pi7h7ZQKBgQDdI7Yb3xFXpbuPd1VS
tNMltMKzEp5uQ7FXyDNI6C8+9TrjNMduTQ3REGqEcfdWA79FTJq95IM7RjXX9Aae
p6cLekyH8MDH/SI744vCedkD2bjpA6MNQrzNkaubzGJgzNiZhjIAqnDAD3ljHI61
NbADc32SQMejb6zlEh8hssSsXwKBgQDCvXhTIO/EuE/y5Kyb/4RGMtVaQ2cpPCoB
GPASbEAHcsRk+4E7RtaoDQC1cBRy+zmiHUA9iI9XZyqD2xwwM89fzqMj5Yhgukvo
XMxvMh8NrTneK9q3/M3mV1AVg71FJQ2oBr8KOXSEbnF25V6/ara2+EpH2C2GDMAo
pgEnZ0/8OwKBgFB58IoQEdWdwLYjLW/d0oGEWN6mRfXGuMFDYDaGGLuGrxmEWZdw
fzi4CquMdgBdeLwVdrLoeEGX+XxPmCEgzg/FQBiwqtec7VpyIqhxg2J9V2elJS9s
PB1rh9I4/QxRP/oO9h9753BdsUU6XUzg7t8ypl4VKRH3UCpFAANZdW1tAoGAK4ad
tjbOYHGxrOBflB5wOiByf1JBZH4GBWjFf9iiFwgXzVpJcC5NHBKL7gG3EFwGba2M
BjTXlPmCDyaSDlQGLavJ2uQar0P0Y2MabmANgMkO/hFfOXBPtQQe6jAfxayaeMvJ
N0fQOylUQvbRTodTf2HPeG9g/W0sJem0qFH3FrECgYEAnwixjpd1Zm/diJuP0+Lb
YUzDP+Afy78IP3mXlbaQ/RVd7fJzMx6HOc8s4rQo1m0Y84Ztot0vwm9+S54mxVSo
6tvh9q0D7VLDgf+2NpnrDW7eMB3n0SrLJ83Mjc5rZ+wv7m033EPaWSr/TFtc/MaF
aOI20MEe3be96HHuWD3lTK0=
-----END PRIVATE KEY-----
EOF

cat > ${EC_KEY_FILE1} <<EOF
-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIOyf96eKdFeSFYeHiM09vGAylz+/auaXKEr+fBZssFsJoAoGCCqGSM49
AwEHoUQDQgAEEB54wykhS7YJFD6RYJNnwbWEz3cI7CF5bCDTXlrwI5n3ZsIFO8wV
DyUptLYxuCNPdh+Zijoec8QTa2wCpZQnDw==
-----END EC PRIVATE KEY-----
EOF

echo 'Generate a JWT using an RSA key with a very long expiry ...'
./src/envoy/utils/tools/jwt_generator.py -x 9223372036854775807 ${RSA_KEY_FILE1} RS256 https://example.com test@example.com aud1 > /tmp/istio/good.jwt
echo 'Generate a JWT using an RSA key that is not valid for a very long time ...'
./src/envoy/utils/tools/jwt_generator.py -n 9223372036854775806 -x 9223372036854775807 ${RSA_KEY_FILE1} RS256 https://example.com test@example.com aud1 > /tmp/istio/nbf.jwt
echo 'Generate a JWT using an RSA key that has expired ...'
./src/envoy/utils/tools/jwt_generator.py -x 1 ${RSA_KEY_FILE1} RS256 https://example.com test@example.com aud1 > /tmp/istio/exp.jwt
echo 'Generate a JWT using an RSA key with multiple audiences ...'
./src/envoy/utils/tools/jwt_generator.py -x 9223372036854775807 ${RSA_KEY_FILE1} RS256 https://example.com test@example.com aud1 aud2 > /tmp/istio/auds.jwt
echo 'Generate a JWT using an RSA key and no KeyID ...'
./src/envoy/utils/tools/jwt_generator.py -x 9223372036854775807 ${RSA_KEY_FILE2} RS256 https://example.com test@example.com aud1 > /tmp/istio/nokid.jwt
echo 'Generate a JWT using an RSA key and KeyID ...'
./src/envoy/utils/tools/jwt_generator.py -x 9223372036854775807 -k b3319a147514df7ee5e4bcdee51350cc890cc89e ${RSA_KEY_FILE2} RS256 https://example.com test@example.com aud1 > /tmp/istio/kid.jwt
echo 'Generate a JWT using an RSA key and the wrong KeyID ...'
./src/envoy/utils/tools/jwt_generator.py -x 9223372036854775807 -k 62a93512c9ee4c7f8067b5a216dade2763d32a47 ${RSA_KEY_FILE2} RS256 https://example.com test@example.com aud1 > /tmp/istio/wrongkid.jwt
echo 'Generate a JWT using an RSA key and non-existant KeyID ...'
./src/envoy/utils/tools/jwt_generator.py -x 9223372036854775807 -k blahblahblah ${RSA_KEY_FILE2} RS256 https://example.com test@example.com aud1 > /tmp/istio/noexistkid.jwt
echo 'Generate a JWT using an RSA key and badly formatted KeyID ...'
./src/envoy/utils/tools/jwt_generator.py -x 9223372036854775807 -k 1 ${RSA_KEY_FILE2} RS256 https://example.com test@example.com aud1 > /tmp/istio/badkidformat.jwt

echo 'Generate a JWT using an EC key with a very long expiry ...'
./src/envoy/utils/tools/jwt_generator.py -x 9223372036854775807 -k abc ${EC_KEY_FILE1} ES256 https://example.com test@example.com aud1 > /tmp/istio/goodec.jwt
echo 'Generate a JWT using an EC key and non-existent KeyID ...'
./src/envoy/utils/tools/jwt_generator.py -x 9223372036854775807 -k abcdef ${EC_KEY_FILE1} ES256 https://example.com test@example.com aud1 > /tmp/istio/noexistkidec.jwt
echo 'Generate a JWT using an EC key with no kid ...'
./src/envoy/utils/tools/jwt_generator.py -x 9223372036854775807 ${EC_KEY_FILE1} ES256 https://example.com test@example.com aud1 > /tmp/istio/nokidec.jwt
echo 'Done!'
