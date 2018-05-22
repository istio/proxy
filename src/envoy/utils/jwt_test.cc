/* Copyright 2017 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "jwt.h"

#include "common/common/utility.h"
#include "common/json/json_loader.h"
#include "test/test_common/utility.h"

#include <tuple>

namespace Envoy {
namespace Utils {
namespace Jwt {

class DatasetPem {
 public:
  // JWT with
  // Header:  {"alg":"RS256","typ":"JWT"}
  // Payload:
  // {"iss":"https://example.com","sub":"test@example.com","exp":9223372036854775807, "aud":"aud1"}
  // jwt_generator.py -x 9223372036854775807 ${RSA_KEY_FILE1} RS256 https://example.com test@example.com aud1
  const std::string kJwt =
  "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9."
  "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIsImV4cCI6OTIyMzM3MjAzNjg1NDc3NTgwNywiYXVkIjoiYXVkMSJ9."
  "akByQsBj4ZT5W9ie7X13LPIgvYZhFI3vcrnX5-sKfhariYGFkNXa3OQpWstjmmRCOAyVV2AwMp8cXru6n2R9IXo0EXfFY1McPO_uvtJ5xLCnd13aEIryZfdCT8JSyek0RwBEET9A72A0T2UVbDti-l4fcE7gIWTpbhzm341K8ltEEduLyjXikHQ7ZoKVMd9mktc2Suo65m9pNW6JiSl0QRndUW8zg9bUA_OoFID0SGw_eN2cGaR7huVGAazzGbQJZNl-azMLmGZASXWOkkLWLhE72C2QriomFXSNQBMLxo051Vj-CF5HoSx4nqDxNBcP4DZ0EMTI9zBixQ09n-Y9cA";

  // JWT with
  // Header:  {"alg":"RS256","typ":"JWT"}
  // Payload:
  // {"iss":"https://example.com","sub":"test@example.com","nbf":9223372036854775806, "exp": 9223372036854775807, "aud":"aud1"}
  // jwt_generator.py -n 9223372036854775806 -x 9223372036854775807 ${RSA_KEY_FILE1} RS256 https://example.com test@example.com aud1
  const std::string kJwtNotValidYet =
  "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9."
  "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwibmJmIjo5MjIzMzcyMDM2ODU0Nzc1ODA2LCJzdWIiOiJ0ZXN0QGV4YW1wbGUuY29tIiwiZXhwIjo5MjIzMzcyMDM2ODU0Nzc1ODA3LCJhdWQiOiJhdWQxIn0."
  "WzNv8gAHqCMOjylc4lDZiBVjnnH3EuMJdf1q3WleUfwkF_7F-qhUEaYMWEUi1Ano2OjGRNvAtAASHsqu24oG3l4YZS3fiCsaNv9kmNMtAqVb5HtlwG1g8Spphq7XCx4498tdBYlL7a0EoJWmvo1Wj-BkurzBrOdUiUmtnf8REulVCgRH8UwdMuRspOu3nXdnTnm7FGdLbrQj5jTQBs9bs0oDlaaV2khGk0_z4cgAo0Qti91RXSEfym-mTMqtDZGj3KZrlwLYlZIVgLV3pTIWAr1KqFGBKpMh6C2yUBIf03Fzaqy3yvhZwhVrfODuST-dxQ1XKHTdUc7DOhreErWnQA";

  // JWT with
  // Header:  {"alg":"RS256","typ":"JWT"}
  // Payload:
  // {"iss":"https://example.com","sub":"test@example.com","exp":1, "aud":"aud1"}
  // jwt_generator.py -x 1 ${RSA_KEY_FILE1} RS256 https://example.com test@example.com aud1
  const std::string kJwtExpired =
  "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9."
  "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIsImV4cCI6MSwiYXVkIjoiYXVkMSJ9."
  "j27cScWQXijuCu5pu3mw-iRylYgqkThNwvdTMDHubWyIqRNCUr3YcpqzED_MUsdacDUlFC14_QZVJOPkoZiDIB5eNyIpi8xxiE88GbaGJMLE0m7rQa4MpTETyLaI2TsoQUcp9iMxzqW6V7OzWoBgrE9-DAf6X9TenEt1TQ9-EH3zasA2MrZMkUVkedeJZ_VhkOu6Dug8dHioLelcbqitbRaUnVqRWcOo3J9a0XuRzPqMmp97iirP6c-Rjrf2ojquSk0eA2L3Ha4i6tNZTX-FgrQy8Pi1fRHRfGWDaDnsqzJdAROvu9zK03MEwXc7iF_A280MQLAzuR2qB72gOaivzQ";

  // {"iss":"https://example.com","sub":"test@example.com","exp":9223372036854775807, aud: ["aud1", "aud2"] }
  // jwt_generator.py -x 9223372036854775807 ${RSA_KEY_FILE1} RS256 https://example.com test@example.com aud1 aud2
  const std::string kJwtMultiSub =
    "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9."
    "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIsImV4cCI6OTIyMzM3MjAzNjg1NDc3NTgwNywiYXVkIjpbImF1ZDEiLCJhdWQyIl19."
    "ZRimnPn5DbAhBeGS18E1UUuvvp0QkBTV45NuaSEvf8U1jreZqoc3I2vCfr_7rndlb4N0hshIqX9Hus8InWvvCw2TOaNgBt7h7tOF5Gw7dztMZf5n8vVoDJjQacHbZMfb5IL8ddF0sGUHJ-cNPgNzQ_YuShK30Oc_5_k0wjDFVCIG3fXkKhGmvqAe-gXc2oyvQHprcxYfoKmt6y6DVo7WHU8H_H0wBuTRtN5U0VLllgP01UiJxriAks6lujdFyr4zFosCL3ByEN29z_BxQxFTJSv0nIVYCQ9WlcM86duBPFydInsLAddtlZOkJVoBl9TqKoaH_rRiZP7ITJhpC9Enig";

  const std::string kJwtSub = "test@example.com";
  const std::string kJwtHeaderEncoded = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9";
  const std::string kJwtPayloadEncoded =
      "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIsImV4cCI6OTIyMzM3MjAzNjg1NDc3NTgwNywiYXVkIjpbImF1ZDEiXX0";
  const std::string kJwtSignatureEncoded =
      "ftAY5xUjS41dM0hpfRjPiL5qJjuw8qFJ0SYxsat5DEL7IE7T-YnWKcDn4V3rr4VTdlcYPVi57cPMEMlIloT2vCmMLbfmvQnfcl40Xq-mnRHhbLjI8XdwuOXVlX2WRFhhxshkVcNGlgFBtOR9k_hxozkh70QfClnQ9zuoq7pVacrdHeStAbsFaQwaEeh9EX8MzFrPRo1FlUwGHLjoCFZTpAPYIAgvxSSW03oneRwN42Da6XHaNDjyYAnSEkkbMDZVw_E5XibkXrhbxlRfiyZTWLryHMeO5zypN05G8IJEQE6jTuJBNBJkb8Knrr89kTkhLRJI4DA_hNd7dJkIRhA4hA";
  const std::string kJwtPayload =
      R"EOF({"iss":"https://example.com","sub":"test@example.com","exp":9223372036854775807,"aud":"aud1"})EOF";

  const std::string kPublicKey =
      "MIIBCgKCAQEAtw7MNxUTxmzWROCD5BqJxmzT7xqc9KsnAjbXCoqEEHDx4WBlfcwk"
      "XHt9e/2+Uwi3Arz3FOMNKwGGlbr7clBY3utsjUs8BTF0kO/poAmSTdSuGeh2mSbc"
      "VHvmQ7X/kichWwx5Qj0Xj4REU3Gixu1gQIr3GATPAIULo5lj/ebOGAa+l0wIG80N"
      "zz1pBtTIUx68xs5ZGe7cIJ7E8n4pMX10eeuh36h+aossePeuHulYmjr4N0/1jG7a"
      "+hHYL6nqwOR3ej0VqCTLS0OloC0LuCpLV7CnSpwbp2Qg/c+MDzQ0TH8g8drIzR5h"
      "Fe9a3NlNRMXgUU5RqbLnR9zfXr7b9oEszQIDAQAB";

  //  private key:
  //      "-----BEGIN RSA PRIVATE KEY-----"
  //      "MIIEowIBAAKCAQEAtw7MNxUTxmzWROCD5BqJxmzT7xqc9KsnAjbXCoqEEHDx4WBl"
  //      "fcwkXHt9e/2+Uwi3Arz3FOMNKwGGlbr7clBY3utsjUs8BTF0kO/poAmSTdSuGeh2"
  //      "mSbcVHvmQ7X/kichWwx5Qj0Xj4REU3Gixu1gQIr3GATPAIULo5lj/ebOGAa+l0wI"
  //      "G80Nzz1pBtTIUx68xs5ZGe7cIJ7E8n4pMX10eeuh36h+aossePeuHulYmjr4N0/1"
  //      "jG7a+hHYL6nqwOR3ej0VqCTLS0OloC0LuCpLV7CnSpwbp2Qg/c+MDzQ0TH8g8drI"
  //      "zR5hFe9a3NlNRMXgUU5RqbLnR9zfXr7b9oEszQIDAQABAoIBAQCgQQ8cRZJrSkqG"
  //      "P7qWzXjBwfIDR1wSgWcD9DhrXPniXs4RzM7swvMuF1myW1/r1xxIBF+V5HNZq9tD"
  //      "Z07LM3WpqZX9V9iyfyoZ3D29QcPX6RGFUtHIn5GRUGoz6rdTHnh/+bqJ92uR02vx"
  //      "VPD4j0SNHFrWpxcE0HRxA07bLtxLgNbzXRNmzAB1eKMcrTu/W9Q1zI1opbsQbHbA"
  //      "CjbPEdt8INi9ij7d+XRO6xsnM20KgeuKx1lFebYN9TKGEEx8BCGINOEyWx1lLhsm"
  //      "V6S0XGVwWYdo2ulMWO9M0lNYPzX3AnluDVb3e1Yq2aZ1r7t/GrnGDILA1N2KrAEb"
  //      "AAKHmYNNAoGBAPAv9qJqf4CP3tVDdto9273DA4Mp4Kjd6lio5CaF8jd/4552T3UK"
  //      "N0Q7N6xaWbRYi6xsCZymC4/6DhmLG/vzZOOhHkTsvLshP81IYpWwjm4rF6BfCSl7"
  //      "ip+1z8qonrElxes68+vc1mNhor6GGsxyGe0C18+KzpQ0fEB5J4p0OHGnAoGBAMMb"
  //      "/fpr6FxXcjUgZzRlxHx1HriN6r8Jkzc+wAcQXWyPUOD8OFLcRuvikQ16sa+SlN4E"
  //      "HfhbFn17ABsikUAIVh0pPkHqMsrGFxDn9JrORXUpNhLdBHa6ZH+we8yUe4G0X4Mc"
  //      "R7c8OT26p2zMg5uqz7bQ1nJ/YWlP4nLqIytehnRrAoGAT6Rn0JUlsBiEmAylxVoL"
  //      "mhGnAYAKWZQ0F6/w7wEtPs/uRuYOFM4NY1eLb2AKLK3LqqGsUkAQx23v7PJelh2v"
  //      "z3bmVY52SkqNIGGnJuGDaO5rCCdbH2EypyCfRSDCdhUDWquSpBv3Dr8aOri2/CG9"
  //      "jQSLUOtC8ouww6Qow1UkPjMCgYB8kTicU5ysqCAAj0mVCIxkMZqFlgYUJhbZpLSR"
  //      "Tf93uiCXJDEJph2ZqLOXeYhMYjetb896qx02y/sLWAyIZ0ojoBthlhcLo2FCp/Vh"
  //      "iOSLot4lOPsKmoJji9fei8Y2z2RTnxCiik65fJw8OG6mSm4HeFoSDAWzaQ9Y8ue1"
  //      "XspVNQKBgAiHh4QfiFbgyFOlKdfcq7Scq98MA3mlmFeTx4Epe0A9xxhjbLrn362+"
  //      "ZSCUhkdYkVkly4QVYHJ6Idzk47uUfEC6WlLEAnjKf9LD8vMmZ14yWR2CingYTIY1"
  //      "LL2jMkSYEJx102t2088meCuJzEsF3BzEWOP8RfbFlciT7FFVeiM4"
  //      "-----END RSA PRIVATE KEY-----";

  /*
   * jwt with header replaced by
   * "{"alg":"RS256","typ":"JWT", this is a invalid json}"
   */
  const std::string kJwtWithBadJsonHeader =
      "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsIHRoaXMgaXMgYSBpbnZhbGlkIGpzb259."
      "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIs"
      "ImV4cCI6MTUwMTI4MTA1OH0."
      "ERgdOJdVCrUAaAIMaAG6rgAR7M6ZJUjvKxIMgb9jrfsEVzsetb4UlPsrO-FBA4LUT_"
      "xIshL4Bzd0_3w63v7xol2-iAQgW_7PQeeEEqqMcyfkuXEhHu_lXawAlpqKhCmFuyIeYBiSs-"
      "RRIqHCutIJSBfcIGLMRcVzpMODfwMMlzjw6SlfMuy68h54TpBt1glvwEg71lVVO7IE3Fxwgl"
      "EDR_2MrVwjmyes9TmDgsj_zBHHn_d09kVqV_adYXtVec9fyo7meODQXB_"
      "eWm065WsSRFksQn8fidWtrAfdcSzYe2wN0doP-QYzJeWKll15XVRKS67NeENz40Wd_Tr_"
      "tyHBHw";

  /*
   * jwt with payload replaced by
   * "this is not a json"
   */
  const std::string kJwtWithBadJsonPayload =
      "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.dGhpcyBpcyBub3QgYSBqc29u."
      "NhQjffMuBkYA9MXq3Fi3h2RRR6vNsYHOjF22GRHRcAEsTHJGYpWsU0MpkWnSJ04Ktx6PFp8f"
      "jRUI0bLtLC2R2Nv3VQNfvcZy0cJmlEWGZbRjEA2AwOaMpiKX-6z5BtMic9hG5Aw1IDxf_"
      "ZvqiE5nRxPBnMXxsINgJ1veTd0zBhOsr0Y3Onl2O3UJSqrQn4kSqjpTENODjSJcJcfiy15sU"
      "MX7wCiP_FSjLAW-"
      "mcaa8RdV49LegwB185eK9UmTJ98yTqEN7w9wcKkZFe8vpojkJX8an0EjGOTJ_5IsU1A_"
      "Xv1Z1ZQYGTOEsMH8j9zWslYTRq15bDIyALHRD46UHqjDSQ";

  /*
   * jwt with header replaced by
   * "{"typ":"JWT"}"
   */
  const std::string kJwtWithAlgAbsent =
      "eyJ0eXAiOiJKV1QifQ."
      "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIs"
      "ImV4cCI6MTUwMTI4MTA1OH0"
      "."
      "MGJmMDU2YjViZmJhMzE5MGI3MTRiMmE4NDhiMmIzNzI2Mzk3MGUwOGVmZTAwMzc0YzY4MWFj"
      "NzgzMDZjZWRlYgoyZWY3Mzk2NWE2MjYxZWI2M2FhMGFjM2E1NDQ1MjNmMjZmNjU2Y2MxYWIz"
      "YTczNGFlYTg4ZDIyN2YyZmM4YTI5CjM5OTQwNjI2ZjI3ZDlmZTM4M2JjY2NhZjIxMmJlY2U5"
      "Y2Q3NGY5YmY2YWY2ZDI2ZTEyNDllMjU4NGVhZTllMGQKMzg0YzVlZmUzY2ZkMWE1NzM4YTIw"
      "MzBmYTQ0OWY0NDQ1MTNhOTQ4NTRkMzgxMzdkMTljMWQ3ZmYxYjNlMzJkMQoxMGMyN2VjZDQ5"
      "MTMzNjZiZmE4Zjg3ZTEyNWQzMGEwYjhhYjUyYWE5YzZmZTcwM2FmZDliMjkzODY3OWYxNWQy"
      "CjZiNWIzZjgzYTk0Zjg1MjFkMDhiNmYyNzY1MTM1N2MyYWI0MzBkM2FlYjg5MTFmNjM5ZGNj"
      "MGM2NTcxNThmOWUKOWQ1ZDM2NWFkNGVjOTgwYmNkY2RiMDUzM2MzYjY2MjJmYWJiMDViNjNk"
      "NjIxMDJiZDkyZDE3ZjZkZDhiMTBkOQo1YjBlMDRiZWU2MDBjNjRhNzM0ZGE1ZGY2YjljMGY5"
      "ZDM1Mzk3MjcyNDcyN2RjMTViYjk1MjMwZjdmYmU5MzYx";

  /*
   * jwt with header replaced by
   * "{"alg":256,"typ":"JWT"}"
   */
  const std::string kJwtWithAlgIsNotString =
      "eyJhbGciOjI1NiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIs"
      "ImV4cCI6MTUwMTI4MTA1OH0."
      "ODYxMDhhZjY5MjEyMGQ4ZjE5YzMzYmQzZDY3MmE1NjFjNDM1NzdhYmNhNDM0Njg2MWMwNGI4"
      "ZDNhZDExNjUxZgphZTU0ZjMzZWVmMWMzYmQyOTEwNGIxNTA3ZDllZTI0ZmY0OWFkZTYwNGUz"
      "MGUzMWIxN2MwMTIzNTY0NDYzNjBlCjEyZDk3ZGRiMDAwZDgwMDFmYjcwOTIzZDYzY2VhMzE1"
      "MjcyNzdlY2RhYzZkMWU5MThmOThjOTFkNTZiM2NhYWIKNjA0ZThiNWI4N2MwNWM4M2RkODE4"
      "NWYwNDBiYjY4Yjk3MmY5MDc2YmYzYTk3ZjM0OWVhYjA1ZTdjYzdhOGEzZApjMGU4Y2Y0MzJl"
      "NzY2MDAwYTQ0ZDg1ZmE5MjgzM2ExNjNjOGM3OTllMTEyNTM5OWMzYzY3MThiMzY2ZjU5YTVl"
      "CjVjYTdjZTBmNDdlMjZhYjU3M2Y2NDI4ZmRmYzQ2N2NjZjQ4OWFjNTA1OTRhM2NlYTlhNTE1"
      "ODJhMDE1ODA2YzkKZmRhNTFmODliNTk3NjA4Njg2NzNiMDUwMzdiY2IzOTQzMzViYzU2YmFk"
      "ODUyOWIwZWJmMjc1OTkxMTkzZjdjMwo0MDFjYWRlZDI4NjA2MmNlZTFhOGU3YWFiMDJiNjcy"
      "NGVhYmVmMjA3MGQyYzFlMmY3NDRiM2IyNjU0MGQzZmUx";

  /*
   * jwt with header replaced by
   * "{"alg":"InvalidAlg","typ":"JWT"}"
   */
  const std::string kJwtWithInvalidAlg =
      "eyJhbGciOiJJbnZhbGlkQWxnIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIs"
      "ImV4cCI6MTUwMTI4MTA1OH0."
      "MjQ3ZThmMTQ1YWYwZDIyZjVlZDlhZTJhOWIzYWI2OGY5ZTcyZWU1ZmJlNzA1ODE2NjkxZDU3"
      "OGY0MmU0OTlhNgpiMmY0NmM2OTI3Yjc1Mjk3NDdhYTQyZTY3Mjk2NGY0MzkzMzgwMjhlNjE2"
      "ZDk2YWM4NDQwZTQ1MGRiYTM5NjJmCjNlODU0YjljOTNjOTg4YTZmNjVkNDhiYmViNTBkZTg5"
      "NWZjOGNmM2NmY2I0MGY1MmU0YjQwMWFjYWZlMjU0M2EKMzc3MjU2YzgyNmZlODIxYTgxNDZm"
      "ZDZkODhkZjg3Yzg1MjJkYTM4MWI4MmZiNTMwOGYxODAzMGZjZGNjMjAxYgpmYmM2NzRiZGQ5"
      "YWMyNzYwZDliYzBlMTMwMDA2OTE3MTBmM2U5YmZlN2Y4OGYwM2JjMWFhNTAwZTY2ZmVhMDk5"
      "CjlhYjVlOTFiZGVkNGMxZTBmMzBiNTdiOGM0MDY0MGViNjMyNTE2Zjc5YTczNzM0YTViM2M2"
      "YjAxMGQ4MjYyYmUKM2U1MjMyMTE4MzUxY2U5M2VkNmY1NWJhYTFmNmU5M2NmMzVlZjJiNjRi"
      "MDYxNzU4YWJmYzdkNzUzYzAxMWVhNgo3NTg1N2MwMGY3YTE3Y2E3YWI2NGJlMWIyYjdkNzZl"
      "NWJlMThhZWFmZWY5NDU5MjAxY2RkY2NkZGZiZjczMjQ2";
};

class DatasetJwk {
 public:
  // The following public key jwk and token are taken from
  // https://github.com/cloudendpoints/esp/blob/master/src/api_manager/auth/lib/auth_jwt_validator_test.cc
  const std::string kPublicKeyRSA =
      "{\"keys\": [{\"kty\": \"RSA\",\"alg\": \"RS256\",\"use\": "
      "\"sig\",\"kid\": \"62a93512c9ee4c7f8067b5a216dade2763d32a47\",\"n\": "
      "\"0YWnm_eplO9BFtXszMRQNL5UtZ8HJdTH2jK7vjs4XdLkPW7YBkkm_"
      "2xNgcaVpkW0VT2l4mU3KftR-6s3Oa5Rnz5BrWEUkCTVVolR7VYksfqIB2I_"
      "x5yZHdOiomMTcm3DheUUCgbJRv5OKRnNqszA4xHn3tA3Ry8VO3X7BgKZYAUh9fyZTFLlkeAh"
      "0-"
      "bLK5zvqCmKW5QgDIXSxUTJxPjZCgfx1vmAfGqaJb-"
      "nvmrORXQ6L284c73DUL7mnt6wj3H6tVqPKA27j56N0TB1Hfx4ja6Slr8S4EB3F1luYhATa1P"
      "KU"
      "SH8mYDW11HolzZmTQpRoLV8ZoHbHEaTfqX_aYahIw\",\"e\": \"AQAB\"},{\"kty\": "
      "\"RSA\",\"alg\": \"RS256\",\"use\": \"sig\",\"kid\": "
      "\"b3319a147514df7ee5e4bcdee51350cc890cc89e\",\"n\": "
      "\"qDi7Tx4DhNvPQsl1ofxxc2ePQFcs-L0mXYo6TGS64CY_"
      "2WmOtvYlcLNZjhuddZVV2X88m0MfwaSA16wE-"
      "RiKM9hqo5EY8BPXj57CMiYAyiHuQPp1yayjMgoE1P2jvp4eqF-"
      "BTillGJt5W5RuXti9uqfMtCQdagB8EC3MNRuU_KdeLgBy3lS3oo4LOYd-"
      "74kRBVZbk2wnmmb7IhP9OoLc1-7-9qU1uhpDxmE6JwBau0mDSwMnYDS4G_ML17dC-"
      "ZDtLd1i24STUw39KH0pcSdfFbL2NtEZdNeam1DDdk0iUtJSPZliUHJBI_pj8M-2Mn_"
      "oA8jBuI8YKwBqYkZCN1I95Q\",\"e\": \"AQAB\"}]}";

  //  private key:
  //      "-----BEGIN PRIVATE KEY-----\n"
  //      "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCoOLtPHgOE289C\n"
  //      "yXWh/HFzZ49AVyz4vSZdijpMZLrgJj/ZaY629iVws1mOG511lVXZfzybQx/BpIDX\n"
  //      "rAT5GIoz2GqjkRjwE9ePnsIyJgDKIe5A+nXJrKMyCgTU/aO+nh6oX4FOKWUYm3lb\n"
  //      "lG5e2L26p8y0JB1qAHwQLcw1G5T8p14uAHLeVLeijgs5h37viREFVluTbCeaZvsi\n"
  //      "E/06gtzX7v72pTW6GkPGYTonAFq7SYNLAydgNLgb8wvXt0L5kO0t3WLbhJNTDf0o\n"
  //      "fSlxJ18VsvY20Rl015qbUMN2TSJS0lI9mWJQckEj+mPwz7Yyf+gDyMG4jxgrAGpi\n"
  //      "RkI3Uj3lAgMBAAECggEAOuaaVyp4KvXYDVeC07QTeUgCdZHQkkuQemIi5YrDkCZ0\n"
  //      "Zsi6CsAG/f4eVk6/BGPEioItk2OeY+wYnOuDVkDMazjUpe7xH2ajLIt3DZ4W2q+k\n"
  //      "v6WyxmmnPqcZaAZjZiPxMh02pkqCNmqBxJolRxp23DtSxqR6lBoVVojinpnIwem6\n"
  //      "xyUl65u0mvlluMLCbKeGW/K9bGxT+qd3qWtYFLo5C3qQscXH4L0m96AjGgHUYW6M\n"
  //      "Ffs94ETNfHjqICbyvXOklabSVYenXVRL24TOKIHWkywhi1wW+Q6zHDADSdDVYw5l\n"
  //      "DaXz7nMzJ2X7cuRP9zrPpxByCYUZeJDqej0Pi7h7ZQKBgQDdI7Yb3xFXpbuPd1VS\n"
  //      "tNMltMKzEp5uQ7FXyDNI6C8+9TrjNMduTQ3REGqEcfdWA79FTJq95IM7RjXX9Aae\n"
  //      "p6cLekyH8MDH/SI744vCedkD2bjpA6MNQrzNkaubzGJgzNiZhjIAqnDAD3ljHI61\n"
  //      "NbADc32SQMejb6zlEh8hssSsXwKBgQDCvXhTIO/EuE/y5Kyb/4RGMtVaQ2cpPCoB\n"
  //      "GPASbEAHcsRk+4E7RtaoDQC1cBRy+zmiHUA9iI9XZyqD2xwwM89fzqMj5Yhgukvo\n"
  //      "XMxvMh8NrTneK9q3/M3mV1AVg71FJQ2oBr8KOXSEbnF25V6/ara2+EpH2C2GDMAo\n"
  //      "pgEnZ0/8OwKBgFB58IoQEdWdwLYjLW/d0oGEWN6mRfXGuMFDYDaGGLuGrxmEWZdw\n"
  //      "fzi4CquMdgBdeLwVdrLoeEGX+XxPmCEgzg/FQBiwqtec7VpyIqhxg2J9V2elJS9s\n"
  //      "PB1rh9I4/QxRP/oO9h9753BdsUU6XUzg7t8ypl4VKRH3UCpFAANZdW1tAoGAK4ad\n"
  //      "tjbOYHGxrOBflB5wOiByf1JBZH4GBWjFf9iiFwgXzVpJcC5NHBKL7gG3EFwGba2M\n"
  //      "BjTXlPmCDyaSDlQGLavJ2uQar0P0Y2MabmANgMkO/hFfOXBPtQQe6jAfxayaeMvJ\n"
  //      "N0fQOylUQvbRTodTf2HPeG9g/W0sJem0qFH3FrECgYEAnwixjpd1Zm/diJuP0+Lb\n"
  //      "YUzDP+Afy78IP3mXlbaQ/RVd7fJzMx6HOc8s4rQo1m0Y84Ztot0vwm9+S54mxVSo\n"
  //      "6tvh9q0D7VLDgf+2NpnrDW7eMB3n0SrLJ83Mjc5rZ+wv7m033EPaWSr/TFtc/MaF\n"
  //      "aOI20MEe3be96HHuWD3lTK0=\n"
  //      "-----END PRIVATE KEY-----";

  // JWT payload JSON
  const std::string kJwtPayload =
      R"EOF({"iss":"https://example.com","sub":"test@example.com","exp":9223372036854775807,"aud":"aud1"})EOF";

  // JWT without kid
  // Header:  {"alg":"RS256","typ":"JWT"}
  // Payload:
  // {"iss":"https://example.com","sub":"test@example.com","exp":9223372036854775807, "aud": "aud1"}
  // jwt_generator.py -x 9223372036854775807 ${RSA_KEY_FILE2} RS256 https://example.com test@example.com aud1
  const std::string kJwtNoKid =
    "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9."
    "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIsImV4cCI6OTIyMzM3MjAzNjg1NDc3NTgwNywiYXVkIjoiYXVkMSJ9."
    "pAy8_eK3sbQgtV7MGyGyhevguZWM-5Ry-Hf_shXgb4mSE31B5k7VwuZQjx1X1l2lJtAsToxZR3qum15R0nM3IauYGGnVWeW1IFzm5Fi1yAX3N3UkijaG-bQo8SU0XKHD5iKA1qHK418TCwFDDQrRMeyEMPJJBUFg-Z-OmqwKZW8vjjSAfIGr_7gd4RHWuEErlvNQHlARJde8JXOpzz0Ge2XfdDHs_55facz9ciG0P4L_WAZsfawkPTSpxfsZceHKyH3u9sbMBA6UiyBWvkeKm8w5nH777hgHr_vOI6SkTylLe4qOI7Whd5_G1QOHso_4P4s9SCzgzfwoQfwmF2O3-w";

  // JWT payload JSON with long exp
  const std::string kJwtPayloadLongExp =
      R"EOF({"iss":"https://example.com","sub":"test@example.com","aud":"example_service","exp":2001001001})EOF";

  // JWT without kid with long exp
  // Header:  {"alg":"RS256","typ":"JWT"}
  // Payload:
  // {"iss":"https://example.com","sub":"test@example.com","aud":"example_service","exp":2001001001}
  const std::string kJwtNoKidLongExp =
      "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9."
      "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIs"
      "ImF1ZCI6ImV4YW1wbGVfc2VydmljZSIsImV4cCI6MjAwMTAwMTAwMX0."
      "n45uWZfIBZwCIPiL0K8Ca3tmm-ZlsDrC79_"
      "vXCspPwk5oxdSn983tuC9GfVWKXWUMHe11DsB02b19Ow-"
      "fmoEzooTFn65Ml7G34nW07amyM6lETiMhNzyiunctplOr6xKKJHmzTUhfTirvDeG-q9n24-"
      "8lH7GP8GgHvDlgSM9OY7TGp81bRcnZBmxim_UzHoYO3_"
      "c8OP4ZX3xG5PfihVk5G0g6wcHrO70w0_64JgkKRCrLHMJSrhIgp9NHel_"
      "CNOnL0AjQKe9IGblJrMuouqYYS0zEWwmOVUWUSxQkoLpldQUVefcfjQeGjz8IlvktRa77FYe"
      "xfP590ACPyXrivtsxg";
  // JWT with correct kid
  // Header:
  // {"alg":"RS256","typ":"JWT","kid":"b3319a147514df7ee5e4bcdee51350cc890cc89e"}
  // Payload:
  // {"iss":"https://example.com","sub":"test@example.com","exp":9223372036854775807, "aud":"aud1"}
  // jwt_generator.py -x 9223372036854775807 -k b3319a147514df7ee5e4bcdee51350cc890cc89e ${RSA_KEY_FILE2} RS256 https://example.com test@example.com aud1
  const std::string kJwtWithCorrectKid =
    "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6ImIzMzE5YTE0NzUxNGRmN2VlNWU0YmNkZWU1MTM1MGNjODkwY2M4OWUifQ."
    "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIsImV4cCI6OTIyMzM3MjAzNjg1NDc3NTgwNywiYXVkIjoiYXVkMSJ9."
    "cCeIrqTsS3LMntTKvPIYdrTUHtThmHKfMQkfhiNJXLnIqNbmYwbCZqHnXe9NysP4ZJMLSNVh1mTIewwI2n3lTxgZRbSIEF3QyokU130fzKnHEFIeg_hEiN8PbVd5x1twx7r2hUmIMb93NrQXaVgZ5KuYCbc9LJFiTYis8EAF_2Qcs4mHjUIi4s6FuiI0hXg7U0XYVlSSVNiFSaxPjnx-gaYFUKV_xIXW83m8p6XNNY11ohfqQdcmqS93k8CtwYs897kQ4GdZwibSTDpKjj_DXWbXrpwYiE-rBBZtbWm1iTNm_8zTyPPUXMrSXNjWiP8o09ABHYbxXSFkD-tZ7vLJ4Q";

  // JWT with existing but incorrect kid
  // Header:
  // {"alg":"RS256","typ":"JWT","kid":"62a93512c9ee4c7f8067b5a216dade2763d32a47"}
  // Payload:
  // {"iss":"https://example.com","sub":"test@example.com","exp":9223372036854775807, "aud":"aud1"}
  // jwt_generator.py -x 9223372036854775807 -k 62a93512c9ee4c7f8067b5a216dade2763d32a47 ${RSA_KEY_FILE2} RS256 https://example.com test@example.com aud1
  const std::string kJwtWithIncorrectKid =
    "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6IjYyYTkzNTEyYzllZTRjN2Y4MDY3YjVhMjE2ZGFkZTI3NjNkMzJhNDcifQ."
    "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIsImV4cCI6OTIyMzM3MjAzNjg1NDc3NTgwNywiYXVkIjoiYXVkMSJ9."
    "GhXFC8VjpUGDpL7u2eJiPrBPn-QmgtKaMY4gWNQybXNvmpLysXlyWhffxtMjNVMxx38RkdycHqiXiG7AxpqDd-M5jGT2dpdebQS-_un6rP5SU9YTBEYktoSPl6JPMt7lBf-hhgRPrp8EQgzhJZB0XewutrqPJQkqfK_YBT6T2ZH6OKJjFslkfROEIQD6x5zZCM32sqnB6-7aaBSSXeACXZc_qjdSopaHgv2_HhG4_tjn5Ic2X1uBWswWFNJH5-eUqU-QFOlOYyZixVuVZCCeZ2RcNpuuvIlBynAK0Y2_zPXC_W-c8H-GAeFvI1-kCcPUdNtGWftV74-24dxQ5LO7zg";

  // JWT with nonexist kid
  // Header:  {"alg":"RS256","typ":"JWT","kid":"blahblahblah"}
  // Payload:
  // {"iss":"https://example.com","sub":"test@example.com","exp":9223372036854775807, "aud":"aud1"}
  // jwt_generator.py -x 9223372036854775807 -k blahblahblah ${RSA_KEY_FILE2} RS256 https://example.com test@example.com aud1
  const std::string kJwtWithNonExistKid =
      "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6ImJsYWhibGFoYmxhaCJ9."
      "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIsImV4cCI6OTIyMzM3MjAzNjg1NDc3NTgwNywiYXVkIjpbImF1ZDEiXX0."
      "gGgapUmd_dYXdYsT4d9FHtRcK1Hb9j1OG6fjvjEKcCpDAggEHCcBMrKER3qLAuZh_kIm4XNcwT7KRtSt9cwbD-fFxx3VD6q3X-InM3IjaVZHMDup8B645ssVDE1z1jj7q6Ffyc1HBSq1cqT3B7HHbJJPVVlQn1XvnDDH__XIOo525_1BfJ50HW00RekF-xWCWuSYya-2ki5REVI0U0RZvf9kQYvmNhmEsVtqILyO7RlAd7bgEBF664oslt4g1VcoK7RelIdfvf-d-yZN36opcWTstwr1RLgIK6xB27Dwll35Og67kOMllecw43kd3i2ri0di8DLZetNMktmh-1Rmqg";

  // JWT with bad-formatted kid
  // Header:  {"alg":"RS256","typ":"JWT","kid":1}
  // Payload:
  // {"iss":"https://example.com","sub":"test@example.com","exp":9223372036854775807, "aud":"aud1"}
  // jwt_generator.py -x 9223372036854775807 -k 1 ${RSA_KEY_FILE2} RS256 https://example.com test@example.com aud1
  // Note the signature is invalid
  const std::string kJwtWithBadFormatKid =
      "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6MX0K."
      "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIsImV4cCI6OTIyMzM3MjAzNjg1NDc3NTgwNywiYXVkIjpbImF1ZDEiXX0."
      "cE6ffuV3yl3i6uXLL4CFVpbsAbEnP4XTipa8EABAgm0HqFyo3W74RYw73hFmLNx6DzRsw9DXMwR_nW3yWA5vsiXEnTdRhjMxJuhK8DmLPWls0a937G6E1NOeX2YTZ9DTZbqEyizeBJZ3Y-acbrwPfcIjFXqwg7wSjZt32shuuDGeL7Aupej-v7M9RiLCD9eugToC1X7AMb9jhNjom5UYxXog5FcHqeDlkhosF69HM09FwcP1jX0GMsL_Lj4-xbljidhIQjHtI7XSJAoQgCmoIaPSejmdR0svrvLxOY0X4QG1m9UqVIKkx0iiR8_tMGKmVtdoRY16qES6Y1TKi6m_Rw";

  // JWT payload JSON with ES256
  const std::string kJwtPayloadEC =
      R"EOF({"iss":"https://example.com",
      "sub":"test@example.com",
      "exp":9223372036854775807,
      "aud":"aud1"})EOF";

  // Please see jwt_generator.py and jwk_generator.py under /tools/.
  // for ES256-signed jwt token and public jwk generation, respectively.
  // jwt_generator.py uses ES256 private key file to generate JWT token.
  // ES256 private key file can be generated by:
  // $ openssl ecparam -genkey -name prime256v1 -noout -out private_key.pem
  // jwk_generator.py uses ES256 public key file to generate JWK. ES256
  // public key file can be generated by:
  // $ openssl ec -in private_key.pem -pubout -out public_key.pem.

  // ES256 private key:
  // "-----BEGIN EC PRIVATE KEY-----"
  // "MHcCAQEEIOyf96eKdFeSFYeHiM09vGAylz+/auaXKEr+fBZssFsJoAoGCCqGSM49"
  // "AwEHoUQDQgAEEB54wykhS7YJFD6RYJNnwbWEz3cI7CF5bCDTXlrwI5n3ZsIFO8wV"
  // "DyUptLYxuCNPdh+Zijoec8QTa2wCpZQnDw=="
  // "-----END EC PRIVATE KEY-----"

  // ES256 public key:
  // "-----BEGIN PUBLIC KEY-----"
  // "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEEB54wykhS7YJFD6RYJNnwbWEz3cI"
  // "7CF5bCDTXlrwI5n3ZsIFO8wVDyUptLYxuCNPdh+Zijoec8QTa2wCpZQnDw=="
  // "-----END PUBLIC KEY-----"

  const std::string kPublicKeyJwkEC =
      "{\"keys\": ["
      "{"
      "\"kty\": \"EC\","
      "\"crv\": \"P-256\","
      "\"x\": \"EB54wykhS7YJFD6RYJNnwbWEz3cI7CF5bCDTXlrwI5k\","
      "\"y\": \"92bCBTvMFQ8lKbS2MbgjT3YfmYo6HnPEE2tsAqWUJw8\","
      "\"alg\": \"ES256\","
      "\"kid\": \"abc\""
      "},"
      "{"
      "\"kty\": \"EC\","
      "\"crv\": \"P-256\","
      "\"x\": \"EB54wykhS7YJFD6RYJNnwbWEz3cI7CF5bCDTXlrwI5k\","
      "\"y\": \"92bCBTvMFQ8lKbS2MbgjT3YfmYo6HnPEE2tsAqWUJw8\","
      "\"alg\": \"ES256\","
      "\"kid\": \"xyz\""
      "}"
      "]}";

  // "{"kid":"abc"}"
  // jwt_generator.py -x 9223372036854775807 -k abc ${EC_KEY_FILE1} ES256 https://example.com test@example.com aud1
  const std::string kTokenEC =
    "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6ImFiYyJ9."
    "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIsImV4cCI6OTIyMzM3MjAzNjg1NDc3NTgwNywiYXVkIjoiYXVkMSJ9."
    "BNM2vzo8RLANgfWcsq-yDgY60U-_A0FvVvJ84hxIrjbkh2gwBBD3-yhXo69FWCW4My5puM-VdZTqaHo-K6bsjA";

  // "{"kid":"blahblahblah"}"
  // jwt_generator.py -x 9223372036854775807 -k blahblahblah ${EC_KEY_FILE1} ES256 https://example.com test@example.com aud1
  const std::string kJwtWithNonExistKidEC =
    "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6ImJsYWhibGFoYmxhaCJ9."
    "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIsImV4cCI6OTIyMzM3MjAzNjg1NDc3NTgwNywiYXVkIjoiYXVkMSJ9."
    "Wiw_TeP06EC9_E0iBWpzCTO-54U92ngwQ3i9f_IT-Z-xVew-EJHm_A1wGwKcQkjffUoc5-vSksLlqJ2fQVKwog";

  // jwt_generator.py -x 9223372036854775807 ${EC_KEY_FILE1} ES256 https://example.com test@example.com aud1
  const std::string kTokenECNoKid =
    "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9."
    "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIsImV4cCI6OTIyMzM3MjAzNjg1NDc3NTgwNywiYXVkIjoiYXVkMSJ9."
    "LFx9nwj74A4XvH05Usq0a9LNU2Poa9VncPhrOSJq7lAA3J-HUqggDaWfx6YltICqN6GPBrJ6m23cuLaVSlMzcA";
};

namespace {

bool EqJson(Json::ObjectSharedPtr p1, Json::ObjectSharedPtr p2) {
  return p1->asJsonString() == p2->asJsonString();
}
}  // namespace

class JwtTest : public testing::Test {
 protected:
  void DoTest(std::string jwt_str, std::string pkey, std::string pkey_type,
              bool verified, Status status, Json::ObjectSharedPtr payload) {
    Jwt jwt(jwt_str);
    Verifier v;
    std::unique_ptr<Pubkeys> key;
    if (pkey_type == "pem") {
      key = Pubkeys::CreateFrom(pkey, Pubkeys::Type::PEM);
    } else if (pkey_type == "jwks") {
      key = Pubkeys::CreateFrom(pkey, Pubkeys::Type::JWKS);
    } else {
      ASSERT_TRUE(0);
    }
    EXPECT_EQ(verified, v.Verify(jwt, *key));
    EXPECT_EQ(status, v.GetStatus());
    if (verified) {
      ASSERT_TRUE(jwt.Payload());
      EXPECT_TRUE(EqJson(payload, jwt.Payload()));
    }
  }
};

// Test cases w/ PEM-formatted public key

class JwtTestPem : public JwtTest {
 protected:
  DatasetPem ds;
};

TEST_F(JwtTestPem, OK) {
  auto payload = Json::Factory::loadFromString(ds.kJwtPayload);
  DoTest(ds.kJwt, ds.kPublicKey, "pem", true, Status::OK, payload);
}

TEST_F(JwtTestPem, MultiAudiences) {
  Jwt jwt(ds.kJwtMultiSub);
  ASSERT_EQ(jwt.Aud(), std::vector<std::string>({"aud1", "aud2"}));
}

TEST_F(JwtTestPem, NotYetValid) {
  DoTest(ds.kJwtNotValidYet, ds.kPublicKey, "pem", false, Status::JWT_NOT_VALID_YET, nullptr);
}

TEST_F(JwtTestPem, Expired) {
  DoTest(ds.kJwtExpired, ds.kPublicKey, "pem", false, Status::JWT_EXPIRED, nullptr);
}

TEST_F(JwtTestPem, InvalidSignature) {
  auto invalid_jwt = ds.kJwt;
  invalid_jwt[ds.kJwt.length() - 2] =
      ds.kJwt[ds.kJwt.length() - 2] != 'a' ? 'a' : 'b';
  DoTest(invalid_jwt, ds.kPublicKey, "pem", false,
         Status::JWT_INVALID_SIGNATURE, nullptr);
}

TEST_F(JwtTestPem, InvalidPublicKey) {
  auto invalid_pubkey = ds.kPublicKey;
  invalid_pubkey[0] = ds.kPublicKey[0] != 'a' ? 'a' : 'b';
  DoTest(ds.kJwt, invalid_pubkey, "pem", false, Status::PEM_PUBKEY_PARSE_ERROR,
         nullptr);
}

TEST_F(JwtTestPem, PublicKeyInvalidBase64) {
  auto invalid_pubkey = "a";
  DoTest(ds.kJwt, invalid_pubkey, "pem", false, Status::PEM_PUBKEY_BAD_BASE64,
         nullptr);
}

TEST_F(JwtTestPem, Base64urlBadInputHeader) {
  auto invalid_header = ds.kJwtHeaderEncoded + "a";
  auto invalid_jwt = StringUtil::join(
      std::vector<std::string>{invalid_header, ds.kJwtPayloadEncoded,
                               ds.kJwtSignatureEncoded},
      ".");
  DoTest(invalid_jwt, ds.kPublicKey, "pem", false,
         Status::JWT_HEADER_PARSE_ERROR, nullptr);
}

TEST_F(JwtTestPem, Base64urlBadInputPayload) {
  auto invalid_payload = ds.kJwtPayloadEncoded + "a";
  auto invalid_jwt = StringUtil::join(
      std::vector<std::string>{ds.kJwtHeaderEncoded, invalid_payload,
                               ds.kJwtSignatureEncoded},
      ".");
  DoTest(invalid_jwt, ds.kPublicKey, "pem", false,
         Status::JWT_PAYLOAD_PARSE_ERROR, nullptr);
}

TEST_F(JwtTestPem, Base64urlBadinputSignature) {
  auto invalid_signature = "a";
  auto invalid_jwt = StringUtil::join(
      std::vector<std::string>{ds.kJwtHeaderEncoded, ds.kJwtPayloadEncoded,
                               invalid_signature},
      ".");
  DoTest(invalid_jwt, ds.kPublicKey, "pem", false,
         Status::JWT_SIGNATURE_PARSE_ERROR, nullptr);
}

TEST_F(JwtTestPem, JwtInvalidNumberOfDots) {
  auto invalid_jwt = ds.kJwt + '.';
  DoTest(invalid_jwt, ds.kPublicKey, "pem", false, Status::JWT_BAD_FORMAT,
         nullptr);
}

TEST_F(JwtTestPem, JsonBadInputHeader) {
  DoTest(ds.kJwtWithBadJsonHeader, ds.kPublicKey, "pem", false,
         Status::JWT_HEADER_PARSE_ERROR, nullptr);
}

TEST_F(JwtTestPem, JsonBadInputPayload) {
  DoTest(ds.kJwtWithBadJsonPayload, ds.kPublicKey, "pem", false,
         Status::JWT_PAYLOAD_PARSE_ERROR, nullptr);
}

TEST_F(JwtTestPem, AlgAbsentInHeader) {
  DoTest(ds.kJwtWithAlgAbsent, ds.kPublicKey, "pem", false,
         Status::JWT_HEADER_NO_ALG, nullptr);
}

TEST_F(JwtTestPem, AlgIsNotString) {
  DoTest(ds.kJwtWithAlgIsNotString, ds.kPublicKey, "pem", false,
         Status::JWT_HEADER_BAD_ALG, nullptr);
}

TEST_F(JwtTestPem, InvalidAlg) {
  DoTest(ds.kJwtWithInvalidAlg, ds.kPublicKey, "pem", false,
         Status::ALG_NOT_IMPLEMENTED, nullptr);
}

TEST(JwtSubExtractionTest, NonEmptyJwtSubShouldEqual) {
  DatasetPem ds;
  Jwt jwt(ds.kJwt);
  EXPECT_EQ(jwt.Sub(), ds.kJwtSub);
}

TEST(JwtSubExtractionTest, EmptyJwtSubShouldEqual) {
  Jwt jwt("");
  EXPECT_EQ(jwt.Sub(), "");
}

// Test cases w/ JWKs-formatted public key

class JwtTestJwks : public JwtTest {
 protected:
  DatasetJwk ds;
};

TEST_F(JwtTestJwks, OkNoKid) {
  auto payload = Json::Factory::loadFromString(ds.kJwtPayload);
  DoTest(ds.kJwtNoKid, ds.kPublicKeyRSA, "jwks", true, Status::OK, payload);
}

TEST_F(JwtTestJwks, OkTokenJwkRSAPublicKeyOptionalAlgKid) {
  auto payload = Json::Factory::loadFromString(ds.kJwtPayload);
  // Remove "alg" claim from public key.
  std::string alg_claim = "\"alg\": \"RS256\",";
  std::string pubkey_no_alg = ds.kPublicKeyRSA;
  std::size_t alg_pos = pubkey_no_alg.find(alg_claim);
  while (alg_pos != std::string::npos) {
    pubkey_no_alg.erase(alg_pos, alg_claim.length());
    alg_pos = pubkey_no_alg.find(alg_claim);
  }
  DoTest(ds.kJwtNoKid, pubkey_no_alg, "jwks", true, Status::OK, payload);

  // Remove "kid" claim from public key.
  std::string kid_claim1 =
      ",\"kid\": \"62a93512c9ee4c7f8067b5a216dade2763d32a47\"";
  std::string kid_claim2 =
      ",\"kid\": \"b3319a147514df7ee5e4bcdee51350cc890cc89e\"";
  std::string pubkey_no_kid = ds.kPublicKeyRSA;
  std::size_t kid_pos = pubkey_no_kid.find(kid_claim1);
  pubkey_no_kid.erase(kid_pos, kid_claim1.length());
  kid_pos = pubkey_no_kid.find(kid_claim2);
  pubkey_no_kid.erase(kid_pos, kid_claim2.length());
  DoTest(ds.kJwtNoKid, pubkey_no_kid, "jwks", true, Status::OK, payload);
}

TEST_F(JwtTestJwks, OkNoKidLongExp) {
  auto payload = Json::Factory::loadFromString(ds.kJwtPayloadLongExp);
  DoTest(ds.kJwtNoKidLongExp, ds.kPublicKeyRSA, "jwks", true, Status::OK,
         payload);
}

TEST_F(JwtTestJwks, OkCorrectKid) {
  auto payload = Json::Factory::loadFromString(ds.kJwtPayload);
  DoTest(ds.kJwtWithCorrectKid, ds.kPublicKeyRSA, "jwks", true, Status::OK,
         payload);
}

TEST_F(JwtTestJwks, IncorrectKid) {
  DoTest(ds.kJwtWithIncorrectKid, ds.kPublicKeyRSA, "jwks", false,
         Status::JWT_INVALID_SIGNATURE, nullptr);
}

TEST_F(JwtTestJwks, NonExistKid) {
  DoTest(ds.kJwtWithNonExistKid, ds.kPublicKeyRSA, "jwks", false,
         Status::KID_ALG_UNMATCH, nullptr);
}

TEST_F(JwtTestJwks, BadFormatKid) {
  DoTest(ds.kJwtWithBadFormatKid, ds.kPublicKeyRSA, "jwks", false,
         Status::JWT_HEADER_BAD_KID, nullptr);
}

TEST_F(JwtTestJwks, JwkBadJson) {
  std::string invalid_pubkey = "foobar";
  DoTest(ds.kJwtNoKid, invalid_pubkey, "jwks", false, Status::JWK_PARSE_ERROR,
         nullptr);
}

TEST_F(JwtTestJwks, JwkNoKeys) {
  std::string invalid_pubkey = R"EOF({"foo":"bar"})EOF";
  DoTest(ds.kJwtNoKid, invalid_pubkey, "jwks", false, Status::JWK_NO_KEYS,
         nullptr);
}

TEST_F(JwtTestJwks, JwkBadKeys) {
  std::string invalid_pubkey = R"EOF({"keys":"foobar"})EOF";
  DoTest(ds.kJwtNoKid, invalid_pubkey, "jwks", false, Status::JWK_BAD_KEYS,
         nullptr);
}

TEST_F(JwtTestJwks, JwkBadPublicKey) {
  std::string invalid_pubkey = R"EOF({"keys":[]})EOF";
  DoTest(ds.kJwtNoKid, invalid_pubkey, "jwks", false,
         Status::JWK_NO_VALID_PUBKEY, nullptr);
}

TEST_F(JwtTestJwks, OkTokenJwkEC) {
  auto payload = Json::Factory::loadFromString(ds.kJwtPayloadEC);
  // ES256-signed token with kid specified.
  DoTest(ds.kTokenEC, ds.kPublicKeyJwkEC, "jwks", true, Status::OK, payload);
  // ES256-signed token without kid specified.
  //DoTest(ds.kTokenECNoKid, ds.kPublicKeyJwkEC, "jwks", true, Status::OK, payload);
}

TEST_F(JwtTestJwks, OkTokenJwkECPublicKeyOptionalAlgKid) {
  auto payload = Json::Factory::loadFromString(ds.kJwtPayloadEC);
  // Remove "alg" claim from public key.
  std::string alg_claim = "\"alg\": \"ES256\",";
  std::string pubkey_no_alg = ds.kPublicKeyJwkEC;
  std::size_t alg_pos = pubkey_no_alg.find(alg_claim);
  while (alg_pos != std::string::npos) {
    pubkey_no_alg.erase(alg_pos, alg_claim.length());
    alg_pos = pubkey_no_alg.find(alg_claim);
  }
  DoTest(ds.kTokenEC, pubkey_no_alg, "jwks", true, Status::OK, payload);

  // Remove "kid" claim from public key.
  std::string kid_claim1 = ",\"kid\": \"abc\"";
  std::string kid_claim2 = ",\"kid\": \"xyz\"";
  std::string pubkey_no_kid = ds.kPublicKeyJwkEC;
  std::size_t kid_pos = pubkey_no_kid.find(kid_claim1);
  pubkey_no_kid.erase(kid_pos, kid_claim1.length());
  kid_pos = pubkey_no_kid.find(kid_claim2);
  pubkey_no_kid.erase(kid_pos, kid_claim2.length());
  DoTest(ds.kTokenEC, pubkey_no_kid, "jwks", true, Status::OK, payload);
}

TEST_F(JwtTestJwks, NonExistKidEC) {
  DoTest(ds.kJwtWithNonExistKidEC, ds.kPublicKeyJwkEC, "jwks", false,
         Status::KID_ALG_UNMATCH, nullptr);
}

TEST_F(JwtTestJwks, InvalidPublicKeyEC) {
  auto invalid_pubkey = ds.kPublicKeyJwkEC;
  invalid_pubkey.replace(12, 9, "kty\":\"RSA");
  DoTest(ds.kTokenEC, invalid_pubkey, "jwks", false, Status::KID_ALG_UNMATCH,
         nullptr);
}

}  // namespace JwtAuth
}  // namespace Utils
}  // namespace Envoy
