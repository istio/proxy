# Copyright 2020 Istio Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# Using istio.io/istio/security/tools/jwt/samples/get-jwt.py
# python ./gen-jwt.py key.pem --expire=3153600000 --claims=group:canary --iss testing@secure.istio.io
export TOKEN_CANARY="eyJhbGciOiJSUzI1NiIsImtpZCI6IkRIRmJwb0lVcXJZOHQyenBBMnFYZkNtcjVWTzVaRXI0UnpIVV8tZW52dlEiLCJ0eXAiOiJKV1QifQ.eyJleHAiOjQ3MzU3NDYwMTgsImdyb3VwIjoiY2FuYXJ5IiwiaWF0IjoxNTgyMTQ2MDE4LCJpc3MiOiJ0ZXN0aW5nQHNlY3VyZS5pc3Rpby5pbyIsInN1YiI6InRlc3RpbmdAc2VjdXJlLmlzdGlvLmlvIn0.Rz1Vq2-55nJUcwozuV-FozoWEh2W0QvtkCbDCXBv6Tr9qxTtJ07uM6OWTnYQo-21sJqH5uv8q8G9j0SkCVX_QfYktR0c76JogXDPw9uZxcuYauQfIGn_E2L3DSShPY7AbTZdCxEMyIaYIBR8B6odRTfaqjuOjMdfSwmZAVtn3buB3Ay4_DY5hspsye8rF614dGPDT0ijr-pSbH_JeBGY6Mucm_86XIrKJ84YsTMJmu0lBRs7G5J54-MA9lwKUoPXmmHeo5OlIpB-LWb9tx5ssRBDwukCkMVO-Wa9cHcDyxRbsk0X2Cv3ExYmyeF-1A4P-jazEsGSxV41p1FsrMFDXg"

# python ./gen-jwt.py key.pem --expire=3153600000 --claims=group:prod --iss testing@secure.istio.io
export TOKEN_PROD="eyJhbGciOiJSUzI1NiIsImtpZCI6IkRIRmJwb0lVcXJZOHQyenBBMnFYZkNtcjVWTzVaRXI0UnpIVV8tZW52dlEiLCJ0eXAiOiJKV1QifQ.eyJleHAiOjQ3MzU3NDYwMjcsImdyb3VwIjoicHJvZCIsImlhdCI6MTU4MjE0NjAyNywiaXNzIjoidGVzdGluZ0BzZWN1cmUuaXN0aW8uaW8iLCJzdWIiOiJ0ZXN0aW5nQHNlY3VyZS5pc3Rpby5pbyJ9.pA_Z-PzJVAaNiS__FOQWZpjPHdVzgJxzSm0DNIp85-k4bx93p-1HDU4NayM9kp_3JosB4li8ImusrRc1aWPiMXqBQMnVfKc8w35FKSF8SZno-aZsk_F4cpN-BlOVyOFzMIPtOIuHAh15QmdUz3UTBIO0b5VzQp8j8XRbaxxbprr4Woknr4PZD5ktNYH6XI1MyGa3f3jr9cKzeYH-bdXT6ZZucxsvRTXBlzp5HUyQsqbuRa5vgt19xH0iH54nk-KCm1JvE-f_a7nipI31d6WvgHPTRuOY-WC5MQ9EDAbH9ZrRtyndr9nXweZtN-Fqpr-Au2_xbOXPH-sg_TFpPJTBGA"

# python ./gen-jwt.py key.pem --expire=3153600000 --claims=group:dev --iss testing@secure.istio.io
export TOKEN_DEV="eyJhbGciOiJSUzI1NiIsImtpZCI6IkRIRmJwb0lVcXJZOHQyenBBMnFYZkNtcjVWTzVaRXI0UnpIVV8tZW52dlEiLCJ0eXAiOiJKV1QifQ.eyJleHAiOjQ3MzU3NTUxMjgsImdyb3VwIjoiZGV2IiwiaWF0IjoxNTgyMTU1MTI4LCJpc3MiOiJ0ZXN0aW5nQHNlY3VyZS5pc3Rpby5pbyIsInN1YiI6InRlc3RpbmdAc2VjdXJlLmlzdGlvLmlvIn0.jgHb0qrRB74Bxz9Egy2IWVRIfQh5RksYzg_Y6Rp7KE4ttna5UVAPBrwk1eMbL78qFskKB2W_YCAJE1V6gzQqftEnTxJcibluKdhhnhyWNaP-s9eWAWaT1dQRLKEHEGImk-dUYVBwLGeMlz4ywbGNwUaaoYQLgcPeR9dbayjGdBb0OXZwkfKp_nFf6DSeUhSeihjbLLsFHthUItTUeyHxT10s_POGOq9vNSzrQ3yz2aN15DWz1Kwuu2DcccvoEqBlcr_MrEPXMg8mKIdJv4ayQD0loqKwQVHc6wRqcCTwHpTZcZhgH798w5izRbx9XQi0G1nqAc2bRcXpjeackVcPsA"

# python ./gen-jwt.py key.pem --expire=3153600000 --claims=group:prod --iss new@secure.istio.io
export TOKEN_PROD_ISS2="eyJhbGciOiJSUzI1NiIsImtpZCI6IkRIRmJwb0lVcXJZOHQyenBBMnFYZkNtcjVWTzVaRXI0UnpIVV8tZW52dlEiLCJ0eXAiOiJKV1QifQ.eyJleHAiOjQ3MzU3NDg0NDUsImdyb3VwIjoicHJvZCIsImlhdCI6MTU4MjE0ODQ0NSwiaXNzIjoibmV3QHNlY3VyZS5pc3Rpby5pbyIsInN1YiI6Im5ld0BzZWN1cmUuaXN0aW8uaW8ifQ.wlp5-UiMIbg_UHqQ_ZXp4aTby6SlRjzTkxXS1sW96jLNsrVg9alfwChRLhOiaGYxcjaHSHqyw5_yaHFSvsWEv3CQBQrAp0DMEHVGzbqSG_g5VHYv6YVNVwLE75lxKFbdy4joRBS54xLQuF1bAAT7e1Qle278O60h0VnT-90UtEo1RKUlklICkU5QUeKyebZ9CHt1pVRjB8zEdxzr0uHeUSFoiJXZxRU5GoAN6TpRi01Bvteo0INOvrM3F6L_E4EX9wi0q774mJpLr6PItMLhSsaG10mWmDzV8kgcpKtr5G9ilVrvn515hP890ZnBVi3EgDIi1fG8vGuGzYkEmbesQw"

echo "Attempting canary token"
curl http://localhost:8081/201 -H "Authorization: Bearer $TOKEN_CANARY" -H "jwt-issuer: newissuer"

echo "Attempting prod token"
curl http://localhost:8081/201 -H "Authorization: Bearer $TOKEN_PROD" -H "jwt-issuer: newissuer"

echo "Attempting dev token"
curl -sD - http://localhost:8081/201 -H "Authorization: Bearer $TOKEN_DEV" -H "jwt-issuer: newissuer"

echo "Attempting prod token with unknown issuer"
curl http://localhost:8081/201 -H "Authorization: Bearer $TOKEN_PROD_ISS2" -H "jwt-issuer: newissuer"

echo "Using bad token"
curl http://localhost:8081/201 -H "Authorization: Bearer BAD$TOKEN"
