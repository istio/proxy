// Copyright 2019 Istio Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package tcp_metadata_exchange_test

import (
	"bytes"
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"io/ioutil"

	"testing"
	"text/template"

	"istio.io/proxy/test/envoye2e/env"
)

const metadataExchangeIstioConfigFilter = `
- name: envoy.filters.network.metadata_exchange
  config:
    protocol: istio2
`

const metadataExchangeIstioUpstreamConfigFilterChain = `
filters:
- name: envoy.filters.network.upstream.metadata_exchange
  typed_config: 
    "@type": type.googleapis.com/envoy.tcp.metadataexchange.config.MetadataExchange
    protocol: istio2
`

const tlsContext = `
tls_context:
  common_tls_context:
    alpn_protocols:
    - istio2
    tls_certificates:
    - certificate_chain:
        inline_string: "-----BEGIN CERTIFICATE-----\nMIIFMTCCAxmgAwIBAgIDAxaCMA0GCSqGSIb3DQEBCwUAMCIxDjAMBgNVBAoMBUlz\ndGlvMRAwDgYDVQQDDAdSb290IENBMB4XDTE5MDcyMjIxMzAzMloXDTIxMDcyMTIx\nMzAzMlowNzEOMAwGA1UECgwFSXN0aW8xGDAWBgNVBAMMD0ludGVybWVkaWF0ZSBD\nQTELMAkGA1UEBwwCY2EwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQC4\nrEngnty6lVXmyFqC5DsLpoDXWPaOXr4bmKYHL4PeL5vX/OOn+kbHhm4JjYOxKTmO\nYtdLttsQZT7jUd+3WercyVIesWULIO33VbEtNvqKE408J0+5W266+Y+dSmVbrbOf\na6nKP6gpVf1r7Rf0NeS4S3XnUQ0igWo/Pbqn3S2C+ewkR66sCAB5vopKLzdABIN1\n7oLXil2mY4cotk4QPDRgk+AHh+uw1w6JC2c3FcNi3MLh7DIVsLyX//3BWX2bs4jR\nFKva6w1KX2nohECj5FqCd7JuFdqtQO+XW5Ihhag3Hzq9VrqDgR2h0XACLqRNJQG4\n0yzP0b0SvOdpOj6JE33IxOBcLGTvrteBadA0sMzWoCfqYeFLOBhFUGSDamHqd0Or\nqIAza/dE3Pb3VX0OZzW601PqnWXr4YDIKIdb3tgc97j/zbYvcjp40MQfgik6S/lZ\nv8E5ZHHc1Je0zGojL8mAjoklCET1HyP/aRSMIRekdYuCjPqjVyrGeeS3R/Fatigm\ngicVYvFDT7iGauyHPA7894CavHVaA40q20Y78bDJSVgsiznNGN7n2oenBZ7P8kbk\nY2pbNnqhn67v5Na1uSHVGMjB+kbVn0WZbbSawKp0W30TCtnuaBdfI1QjOWYdkIEs\npvtdI31V3cLJO9vzegwhcdYS7YG95m6VrdMQbaBE3wIDAQABo1swWTAdBgNVHQ4E\nFgQUuTgg1nLlC0d35VPxZh1T6NqkDg8wEgYDVR0TAQH/BAgwBgEB/wIBADAOBgNV\nHQ8BAf8EBAMCAuQwFAYDVR0RBA0wC4IJbG9jYWxob3N0MA0GCSqGSIb3DQEBCwUA\nA4ICAQBV0mZPDPDOna692/cRVP2qHoHzEsYLttTioRCmQT8ideW8tpW7IwWozpKr\nBlcaCXUc1K8hoMFSgYCcuh+VMH8qNCQHDEcWoPHPBFrr83ALRVdh4cYeMa7ZcIRS\nl08Fa5TbVQXDkkj+t0KFr6VIBzXvVw8W/r8bgy4LSu/33WGQg4fRecp9mm0j/P8Y\nDaWalN1m8TeRZtN1k7ltHmkeOPH+3NlgZ4YvlZ+ltPMrXowdP+/nCZgeR1BzFmer\n0EVZ0Hq35EvXrmrrN5X4cc3b9OmaQpPQxqSlA/8hwyd0ItLZCYv1v4CB+0AI6CvY\nP2RtxJ87UCz9wlthIlV2a8/d0NItV08HATfK5nXjuY8Ndm3V+jgEGGivizEaSeso\ngrBKJ/TbyoUpsfji5Fc2ogzrGkon1EFgR/WJ8FVlty2YVnjTfjVxD8OJ8Znjm1MH\nYbisHAdTqTND0Fa2F7GFxtltD0DxQ2zsH3D8W98dxeRRigYCifixqFtk72iE702o\n4K3CfPhi7MN4dxbQNFXtjrjnIQn9lN+ih+E1RK0Z4LTrd4WwsJF1MHBm6MRIFu4t\nxaJb3fB5Artwn6DJ1vhfLoONDfwbrRL9/QDt0fFKtnCMbHcApsGJmrXskGim8Kma\nCw3FWjtdhpzmgK5L0SVell2IK3gEF3rphETn37YFDCttOUzpCg==\n-----END CERTIFICATE-----\n-----BEGIN CERTIFICATE-----\nMIIFFDCCAvygAwIBAgIUZqU0Sviq/wULK6UV7PoAZ7B+nqAwDQYJKoZIhvcNAQEL\nBQAwIjEOMAwGA1UECgwFSXN0aW8xEDAOBgNVBAMMB1Jvb3QgQ0EwHhcNMTkwNzIy\nMjEzMDA0WhcNMjkwNzE5MjEzMDA0WjAiMQ4wDAYDVQQKDAVJc3RpbzEQMA4GA1UE\nAwwHUm9vdCBDQTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBANNl5/pH\n/ktdqEsb83cqHrYJCyzbvWce6k/iud4Czu6FClFX8b+n/Rv9GrZFxJwKAFlUx3iA\nBGlSn/1XYpnhudQhgVGvyuWNO5kX4BfrAJwfWt+7Mn6NcWvunDqwqUPxI07sgCJW\nAYBAwkZH/Nhn6tj571XWNPziUtCwlPNkFMiRu/2nI/tq12IgwimFjVgiCuprNfyX\ntQz/DMVTWpCRQLK5ptlYMfk0P25UKyJdKHnr1MPQBJmPXMfSSqpGjksikV4QnYc7\nCXB3ucq7ty0IWA8QXH+86WqMTh22mosWVXHe0OGbzYtuyVnXc1G7YRv4D87G3Ves\nG4n/8e+RaDTacvwOsYEkuQGk+s8pggPkIqydGy02JNZ4cSRpXJRTzME2BgBZxT8S\nEw1Omr5+iuLNRAKEYRM/eWI7qrs5fxpD6K9JELHS41hWHGdW94PP0wKz70trx5pM\nfLpcVm7BQ5ppgf+t4vgKnrNiACQpfyZbInCBU0doaZaqVMnKH0vgyM7xrC43fsOP\ny5URy3tEH8Uk7Dbvsmj7AXR7IPKlYtgcqcJXmeWa+kLOpx3G55hgJL1ySrxXg/qz\nAobgmV0IycH2ntn5lXvjbwe0cfXAnZgGoALZjJVuEazyBmmVzjBjG2Qcq35nHfp8\nRm6WnCZIaGsZqgoDuSJD280ZLWW7R0PMcnypAgMBAAGjQjBAMB0GA1UdDgQWBBQZ\nh3/ckcK23ZYKO+JsZowd3dIobDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQE\nAwIC5DANBgkqhkiG9w0BAQsFAAOCAgEAjh4CdrwnLsqwVxyVSgxd7TfSHtKE/J2Y\n2IZ4fJYXGkq3McPk2e9u0zjCH0buvfDwyAItLIacD+YwIP+OC2WxLe+YMZ5KkXl3\nLuhQ2TOoRlrbp5tYLQITZIIl9+vNkgnn1DkdxkLm9cDDag19LSxa9Rjrnb3wwFAT\nIzEhy+d18FpQtdMMhmonU/L8Oy5LqjT5BR3T8VrXYUsaAkcUs/yHNTFAY3iJFBWL\nZ8dFa5v0A1Ryi8quSNo7lK/hSEZvvV9k4XfFAolXSUqe8BCuXe0rbAq3Jq9HgDww\noImGM0uz4Zf89uhTk1O7UOUfQoSTmA0yZICtQkCiOC0J4AlAOTmiEXUC9gicV3R8\ndvVOqNBOcBELglZ+NIMm6FQQqPh1nZ6A3Bh+JRTPerAF12725RZZE6XMxq2MSr3G\nk5yH10QPMH7/DJRQUhRHAhbge+jk2csa7EGSxABcbsPLSV+cEzXRO4cJeItoZQLh\nsaFhIn9lGukXG6lgiperOqZl6DFVcUG6/nogK7KOTAnV9zjR/7vNwvYzPI9iOR3V\n6dbG38KnipcfL885VLJVTnfhvYHlxFklCKTEnOHnmKsM0qjQuky3DBzmDA6iqeOM\nSHRje5LKxi7mllJfu/X0MxYJWiu6i4gMCWZsC3UtAJQ09x7iwcNr/1bl9ApGszOy\nUff0OxD2hzk=\n-----END CERTIFICATE-----\n"
      private_key:
        inline_string: "-----BEGIN RSA PRIVATE KEY-----\n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\n-----END RSA PRIVATE KEY-----\n"
    validation_context:
      trusted_ca:
        inline_string: "-----BEGIN CERTIFICATE-----\nMIIFFDCCAvygAwIBAgIUZqU0Sviq/wULK6UV7PoAZ7B+nqAwDQYJKoZIhvcNAQEL\nBQAwIjEOMAwGA1UECgwFSXN0aW8xEDAOBgNVBAMMB1Jvb3QgQ0EwHhcNMTkwNzIy\nMjEzMDA0WhcNMjkwNzE5MjEzMDA0WjAiMQ4wDAYDVQQKDAVJc3RpbzEQMA4GA1UE\nAwwHUm9vdCBDQTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBANNl5/pH\n/ktdqEsb83cqHrYJCyzbvWce6k/iud4Czu6FClFX8b+n/Rv9GrZFxJwKAFlUx3iA\nBGlSn/1XYpnhudQhgVGvyuWNO5kX4BfrAJwfWt+7Mn6NcWvunDqwqUPxI07sgCJW\nAYBAwkZH/Nhn6tj571XWNPziUtCwlPNkFMiRu/2nI/tq12IgwimFjVgiCuprNfyX\ntQz/DMVTWpCRQLK5ptlYMfk0P25UKyJdKHnr1MPQBJmPXMfSSqpGjksikV4QnYc7\nCXB3ucq7ty0IWA8QXH+86WqMTh22mosWVXHe0OGbzYtuyVnXc1G7YRv4D87G3Ves\nG4n/8e+RaDTacvwOsYEkuQGk+s8pggPkIqydGy02JNZ4cSRpXJRTzME2BgBZxT8S\nEw1Omr5+iuLNRAKEYRM/eWI7qrs5fxpD6K9JELHS41hWHGdW94PP0wKz70trx5pM\nfLpcVm7BQ5ppgf+t4vgKnrNiACQpfyZbInCBU0doaZaqVMnKH0vgyM7xrC43fsOP\ny5URy3tEH8Uk7Dbvsmj7AXR7IPKlYtgcqcJXmeWa+kLOpx3G55hgJL1ySrxXg/qz\nAobgmV0IycH2ntn5lXvjbwe0cfXAnZgGoALZjJVuEazyBmmVzjBjG2Qcq35nHfp8\nRm6WnCZIaGsZqgoDuSJD280ZLWW7R0PMcnypAgMBAAGjQjBAMB0GA1UdDgQWBBQZ\nh3/ckcK23ZYKO+JsZowd3dIobDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQE\nAwIC5DANBgkqhkiG9w0BAQsFAAOCAgEAjh4CdrwnLsqwVxyVSgxd7TfSHtKE/J2Y\n2IZ4fJYXGkq3McPk2e9u0zjCH0buvfDwyAItLIacD+YwIP+OC2WxLe+YMZ5KkXl3\nLuhQ2TOoRlrbp5tYLQITZIIl9+vNkgnn1DkdxkLm9cDDag19LSxa9Rjrnb3wwFAT\nIzEhy+d18FpQtdMMhmonU/L8Oy5LqjT5BR3T8VrXYUsaAkcUs/yHNTFAY3iJFBWL\nZ8dFa5v0A1Ryi8quSNo7lK/hSEZvvV9k4XfFAolXSUqe8BCuXe0rbAq3Jq9HgDww\noImGM0uz4Zf89uhTk1O7UOUfQoSTmA0yZICtQkCiOC0J4AlAOTmiEXUC9gicV3R8\ndvVOqNBOcBELglZ+NIMm6FQQqPh1nZ6A3Bh+JRTPerAF12725RZZE6XMxq2MSr3G\nk5yH10QPMH7/DJRQUhRHAhbge+jk2csa7EGSxABcbsPLSV+cEzXRO4cJeItoZQLh\nsaFhIn9lGukXG6lgiperOqZl6DFVcUG6/nogK7KOTAnV9zjR/7vNwvYzPI9iOR3V\n6dbG38KnipcfL885VLJVTnfhvYHlxFklCKTEnOHnmKsM0qjQuky3DBzmDA6iqeOM\nSHRje5LKxi7mllJfu/X0MxYJWiu6i4gMCWZsC3UtAJQ09x7iwcNr/1bl9ApGszOy\nUff0OxD2hzk=\n-----END CERTIFICATE-----\n"
  require_client_certificate: true
`

const clusterTlsContext = `
tls_context:
  common_tls_context:
    alpn_protocols:
    - istio2
    tls_certificates:
    - certificate_chain:
        inline_string: "-----BEGIN CERTIFICATE-----\nMIIFMTCCAxmgAwIBAgIDAxaCMA0GCSqGSIb3DQEBCwUAMCIxDjAMBgNVBAoMBUlz\ndGlvMRAwDgYDVQQDDAdSb290IENBMB4XDTE5MDcyMjIxMzAzMloXDTIxMDcyMTIx\nMzAzMlowNzEOMAwGA1UECgwFSXN0aW8xGDAWBgNVBAMMD0ludGVybWVkaWF0ZSBD\nQTELMAkGA1UEBwwCY2EwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQC4\nrEngnty6lVXmyFqC5DsLpoDXWPaOXr4bmKYHL4PeL5vX/OOn+kbHhm4JjYOxKTmO\nYtdLttsQZT7jUd+3WercyVIesWULIO33VbEtNvqKE408J0+5W266+Y+dSmVbrbOf\na6nKP6gpVf1r7Rf0NeS4S3XnUQ0igWo/Pbqn3S2C+ewkR66sCAB5vopKLzdABIN1\n7oLXil2mY4cotk4QPDRgk+AHh+uw1w6JC2c3FcNi3MLh7DIVsLyX//3BWX2bs4jR\nFKva6w1KX2nohECj5FqCd7JuFdqtQO+XW5Ihhag3Hzq9VrqDgR2h0XACLqRNJQG4\n0yzP0b0SvOdpOj6JE33IxOBcLGTvrteBadA0sMzWoCfqYeFLOBhFUGSDamHqd0Or\nqIAza/dE3Pb3VX0OZzW601PqnWXr4YDIKIdb3tgc97j/zbYvcjp40MQfgik6S/lZ\nv8E5ZHHc1Je0zGojL8mAjoklCET1HyP/aRSMIRekdYuCjPqjVyrGeeS3R/Fatigm\ngicVYvFDT7iGauyHPA7894CavHVaA40q20Y78bDJSVgsiznNGN7n2oenBZ7P8kbk\nY2pbNnqhn67v5Na1uSHVGMjB+kbVn0WZbbSawKp0W30TCtnuaBdfI1QjOWYdkIEs\npvtdI31V3cLJO9vzegwhcdYS7YG95m6VrdMQbaBE3wIDAQABo1swWTAdBgNVHQ4E\nFgQUuTgg1nLlC0d35VPxZh1T6NqkDg8wEgYDVR0TAQH/BAgwBgEB/wIBADAOBgNV\nHQ8BAf8EBAMCAuQwFAYDVR0RBA0wC4IJbG9jYWxob3N0MA0GCSqGSIb3DQEBCwUA\nA4ICAQBV0mZPDPDOna692/cRVP2qHoHzEsYLttTioRCmQT8ideW8tpW7IwWozpKr\nBlcaCXUc1K8hoMFSgYCcuh+VMH8qNCQHDEcWoPHPBFrr83ALRVdh4cYeMa7ZcIRS\nl08Fa5TbVQXDkkj+t0KFr6VIBzXvVw8W/r8bgy4LSu/33WGQg4fRecp9mm0j/P8Y\nDaWalN1m8TeRZtN1k7ltHmkeOPH+3NlgZ4YvlZ+ltPMrXowdP+/nCZgeR1BzFmer\n0EVZ0Hq35EvXrmrrN5X4cc3b9OmaQpPQxqSlA/8hwyd0ItLZCYv1v4CB+0AI6CvY\nP2RtxJ87UCz9wlthIlV2a8/d0NItV08HATfK5nXjuY8Ndm3V+jgEGGivizEaSeso\ngrBKJ/TbyoUpsfji5Fc2ogzrGkon1EFgR/WJ8FVlty2YVnjTfjVxD8OJ8Znjm1MH\nYbisHAdTqTND0Fa2F7GFxtltD0DxQ2zsH3D8W98dxeRRigYCifixqFtk72iE702o\n4K3CfPhi7MN4dxbQNFXtjrjnIQn9lN+ih+E1RK0Z4LTrd4WwsJF1MHBm6MRIFu4t\nxaJb3fB5Artwn6DJ1vhfLoONDfwbrRL9/QDt0fFKtnCMbHcApsGJmrXskGim8Kma\nCw3FWjtdhpzmgK5L0SVell2IK3gEF3rphETn37YFDCttOUzpCg==\n-----END CERTIFICATE-----\n-----BEGIN CERTIFICATE-----\nMIIFFDCCAvygAwIBAgIUZqU0Sviq/wULK6UV7PoAZ7B+nqAwDQYJKoZIhvcNAQEL\nBQAwIjEOMAwGA1UECgwFSXN0aW8xEDAOBgNVBAMMB1Jvb3QgQ0EwHhcNMTkwNzIy\nMjEzMDA0WhcNMjkwNzE5MjEzMDA0WjAiMQ4wDAYDVQQKDAVJc3RpbzEQMA4GA1UE\nAwwHUm9vdCBDQTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBANNl5/pH\n/ktdqEsb83cqHrYJCyzbvWce6k/iud4Czu6FClFX8b+n/Rv9GrZFxJwKAFlUx3iA\nBGlSn/1XYpnhudQhgVGvyuWNO5kX4BfrAJwfWt+7Mn6NcWvunDqwqUPxI07sgCJW\nAYBAwkZH/Nhn6tj571XWNPziUtCwlPNkFMiRu/2nI/tq12IgwimFjVgiCuprNfyX\ntQz/DMVTWpCRQLK5ptlYMfk0P25UKyJdKHnr1MPQBJmPXMfSSqpGjksikV4QnYc7\nCXB3ucq7ty0IWA8QXH+86WqMTh22mosWVXHe0OGbzYtuyVnXc1G7YRv4D87G3Ves\nG4n/8e+RaDTacvwOsYEkuQGk+s8pggPkIqydGy02JNZ4cSRpXJRTzME2BgBZxT8S\nEw1Omr5+iuLNRAKEYRM/eWI7qrs5fxpD6K9JELHS41hWHGdW94PP0wKz70trx5pM\nfLpcVm7BQ5ppgf+t4vgKnrNiACQpfyZbInCBU0doaZaqVMnKH0vgyM7xrC43fsOP\ny5URy3tEH8Uk7Dbvsmj7AXR7IPKlYtgcqcJXmeWa+kLOpx3G55hgJL1ySrxXg/qz\nAobgmV0IycH2ntn5lXvjbwe0cfXAnZgGoALZjJVuEazyBmmVzjBjG2Qcq35nHfp8\nRm6WnCZIaGsZqgoDuSJD280ZLWW7R0PMcnypAgMBAAGjQjBAMB0GA1UdDgQWBBQZ\nh3/ckcK23ZYKO+JsZowd3dIobDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQE\nAwIC5DANBgkqhkiG9w0BAQsFAAOCAgEAjh4CdrwnLsqwVxyVSgxd7TfSHtKE/J2Y\n2IZ4fJYXGkq3McPk2e9u0zjCH0buvfDwyAItLIacD+YwIP+OC2WxLe+YMZ5KkXl3\nLuhQ2TOoRlrbp5tYLQITZIIl9+vNkgnn1DkdxkLm9cDDag19LSxa9Rjrnb3wwFAT\nIzEhy+d18FpQtdMMhmonU/L8Oy5LqjT5BR3T8VrXYUsaAkcUs/yHNTFAY3iJFBWL\nZ8dFa5v0A1Ryi8quSNo7lK/hSEZvvV9k4XfFAolXSUqe8BCuXe0rbAq3Jq9HgDww\noImGM0uz4Zf89uhTk1O7UOUfQoSTmA0yZICtQkCiOC0J4AlAOTmiEXUC9gicV3R8\ndvVOqNBOcBELglZ+NIMm6FQQqPh1nZ6A3Bh+JRTPerAF12725RZZE6XMxq2MSr3G\nk5yH10QPMH7/DJRQUhRHAhbge+jk2csa7EGSxABcbsPLSV+cEzXRO4cJeItoZQLh\nsaFhIn9lGukXG6lgiperOqZl6DFVcUG6/nogK7KOTAnV9zjR/7vNwvYzPI9iOR3V\n6dbG38KnipcfL885VLJVTnfhvYHlxFklCKTEnOHnmKsM0qjQuky3DBzmDA6iqeOM\nSHRje5LKxi7mllJfu/X0MxYJWiu6i4gMCWZsC3UtAJQ09x7iwcNr/1bl9ApGszOy\nUff0OxD2hzk=\n-----END CERTIFICATE-----\n"
      private_key:
        inline_string: "-----BEGIN RSA PRIVATE KEY-----\n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\n-----END RSA PRIVATE KEY-----\n"
    validation_context:
      trusted_ca:
        inline_string: "-----BEGIN CERTIFICATE-----\nMIIFFDCCAvygAwIBAgIUZqU0Sviq/wULK6UV7PoAZ7B+nqAwDQYJKoZIhvcNAQEL\nBQAwIjEOMAwGA1UECgwFSXN0aW8xEDAOBgNVBAMMB1Jvb3QgQ0EwHhcNMTkwNzIy\nMjEzMDA0WhcNMjkwNzE5MjEzMDA0WjAiMQ4wDAYDVQQKDAVJc3RpbzEQMA4GA1UE\nAwwHUm9vdCBDQTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBANNl5/pH\n/ktdqEsb83cqHrYJCyzbvWce6k/iud4Czu6FClFX8b+n/Rv9GrZFxJwKAFlUx3iA\nBGlSn/1XYpnhudQhgVGvyuWNO5kX4BfrAJwfWt+7Mn6NcWvunDqwqUPxI07sgCJW\nAYBAwkZH/Nhn6tj571XWNPziUtCwlPNkFMiRu/2nI/tq12IgwimFjVgiCuprNfyX\ntQz/DMVTWpCRQLK5ptlYMfk0P25UKyJdKHnr1MPQBJmPXMfSSqpGjksikV4QnYc7\nCXB3ucq7ty0IWA8QXH+86WqMTh22mosWVXHe0OGbzYtuyVnXc1G7YRv4D87G3Ves\nG4n/8e+RaDTacvwOsYEkuQGk+s8pggPkIqydGy02JNZ4cSRpXJRTzME2BgBZxT8S\nEw1Omr5+iuLNRAKEYRM/eWI7qrs5fxpD6K9JELHS41hWHGdW94PP0wKz70trx5pM\nfLpcVm7BQ5ppgf+t4vgKnrNiACQpfyZbInCBU0doaZaqVMnKH0vgyM7xrC43fsOP\ny5URy3tEH8Uk7Dbvsmj7AXR7IPKlYtgcqcJXmeWa+kLOpx3G55hgJL1ySrxXg/qz\nAobgmV0IycH2ntn5lXvjbwe0cfXAnZgGoALZjJVuEazyBmmVzjBjG2Qcq35nHfp8\nRm6WnCZIaGsZqgoDuSJD280ZLWW7R0PMcnypAgMBAAGjQjBAMB0GA1UdDgQWBBQZ\nh3/ckcK23ZYKO+JsZowd3dIobDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQE\nAwIC5DANBgkqhkiG9w0BAQsFAAOCAgEAjh4CdrwnLsqwVxyVSgxd7TfSHtKE/J2Y\n2IZ4fJYXGkq3McPk2e9u0zjCH0buvfDwyAItLIacD+YwIP+OC2WxLe+YMZ5KkXl3\nLuhQ2TOoRlrbp5tYLQITZIIl9+vNkgnn1DkdxkLm9cDDag19LSxa9Rjrnb3wwFAT\nIzEhy+d18FpQtdMMhmonU/L8Oy5LqjT5BR3T8VrXYUsaAkcUs/yHNTFAY3iJFBWL\nZ8dFa5v0A1Ryi8quSNo7lK/hSEZvvV9k4XfFAolXSUqe8BCuXe0rbAq3Jq9HgDww\noImGM0uz4Zf89uhTk1O7UOUfQoSTmA0yZICtQkCiOC0J4AlAOTmiEXUC9gicV3R8\ndvVOqNBOcBELglZ+NIMm6FQQqPh1nZ6A3Bh+JRTPerAF12725RZZE6XMxq2MSr3G\nk5yH10QPMH7/DJRQUhRHAhbge+jk2csa7EGSxABcbsPLSV+cEzXRO4cJeItoZQLh\nsaFhIn9lGukXG6lgiperOqZl6DFVcUG6/nogK7KOTAnV9zjR/7vNwvYzPI9iOR3V\n6dbG38KnipcfL885VLJVTnfhvYHlxFklCKTEnOHnmKsM0qjQuky3DBzmDA6iqeOM\nSHRje5LKxi7mllJfu/X0MxYJWiu6i4gMCWZsC3UtAJQ09x7iwcNr/1bl9ApGszOy\nUff0OxD2hzk=\n-----END CERTIFICATE-----\n"
`

const clientNodeMetadata = `"NAMESPACE": "default",
"INCLUDE_INBOUND_PORTS": "9080",
"app": "productpage",
"EXCHANGE_KEYS": "NAME,NAMESPACE,INSTANCE_IPS,LABELS,OWNER,PLATFORM_METADATA,WORKLOAD_NAME,CANONICAL_TELEMETRY_SERVICE,MESH_ID,SERVICE_ACCOUNT",
"INSTANCE_IPS": "10.52.0.34,fe80::a075:11ff:fe5e:f1cd",
"pod-template-hash": "84975bc778",
"INTERCEPTION_MODE": "REDIRECT",
"SERVICE_ACCOUNT": "bookinfo-productpage",
"CONFIG_NAMESPACE": "default",
"version": "v1",
"OWNER": "kubernetes://api/apps/v1/namespaces/default/deployment/productpage-v1",
"WORKLOAD_NAME": "productpage-v1",
"ISTIO_VERSION": "1.3-dev",
"kubernetes.io/limit-ranger": "LimitRanger plugin set: cpu request for container productpage",
"POD_NAME": "productpage-v1-84975bc778-pxz2w",
"istio": "sidecar",
"PLATFORM_METADATA": {
 "gcp_cluster_name": "test-cluster",
 "gcp_project": "test-project",
 "gcp_cluster_location": "us-east4-b"
},
"LABELS": {
 "app": "productpage",
 "version": "v1",
 "pod-template-hash": "84975bc778"
},
"ISTIO_PROXY_SHA": "istio-proxy:47e4559b8e4f0d516c0d17b233d127a3deb3d7ce",
"NAME": "productpage-v1-84975bc778-pxz2w",`

const serverNodeMetadata = `"NAMESPACE": "default",
"INCLUDE_INBOUND_PORTS": "9080",
"app": "ratings",
"EXCHANGE_KEYS": "NAME,NAMESPACE,INSTANCE_IPS,LABELS,OWNER,PLATFORM_METADATA,WORKLOAD_NAME,CANONICAL_TELEMETRY_SERVICE,MESH_ID,SERVICE_ACCOUNT",
"INSTANCE_IPS": "10.52.0.34,fe80::a075:11ff:fe5e:f1cd",
"pod-template-hash": "84975bc778",
"INTERCEPTION_MODE": "REDIRECT",
"SERVICE_ACCOUNT": "bookinfo-ratings",
"CONFIG_NAMESPACE": "default",
"version": "v1",
"OWNER": "kubernetes://api/apps/v1/namespaces/default/deployment/ratings-v1",
"WORKLOAD_NAME": "ratings-v1",
"ISTIO_VERSION": "1.3-dev",
"kubernetes.io/limit-ranger": "LimitRanger plugin set: cpu request for container ratings",
"POD_NAME": "ratings-v1-84975bc778-pxz2w",
"istio": "sidecar",
"PLATFORM_METADATA": {
 "gcp_cluster_name": "test-cluster",
 "gcp_project": "test-project",
 "gcp_cluster_location": "us-east4-b"
},
"LABELS": {
 "app": "ratings",
 "version": "v1",
 "pod-template-hash": "84975bc778"
},
"ISTIO_PROXY_SHA": "istio-proxy:47e4559b8e4f0d516c0d17b233d127a3deb3d7ce",
"NAME": "ratings-v1-84975bc778-pxz2w",`

// Stats in Client Envoy proxy.
var expectedClientStats = map[string]int{
	"cluster.client.metadata_exchange.alpn_protocol_found":      1,
	"cluster.client.metadata_exchange.alpn_protocol_not_found":  0,
	"cluster.client.metadata_exchange.initial_header_not_found": 0,
	"cluster.client.metadata_exchange.header_not_found":         0,
	"cluster.client.metadata_exchange.metadata_added":           1,
}

// Stats in Server Envoy proxy.
var expectedServerStats = map[string]int{
	"metadata_exchange.alpn_protocol_found":      1,
	"metadata_exchange.alpn_protocol_not_found":  0,
	"metadata_exchange.initial_header_not_found": 0,
	"metadata_exchange.header_not_found":         0,
	"metadata_exchange.metadata_added":           1,
}

func TestTcpMetadataExchange(t *testing.T) {
	s := env.NewClientServerEnvoyTestSetup(env.TcpMetadataExchangeTest, t)
	s.SetNoBackend(true)
	s.SetStartTcpBackend(true)
	s.SetTlsContext(tlsContext)
	s.SetClusterTlsContext(clusterTlsContext)
	s.SetFiltersBeforeEnvoyRouterInClientToApp(metadataExchangeIstioConfigFilter)
	s.SetUpstreamFiltersInClient(metadataExchangeIstioUpstreamConfigFilterChain)
	s.SetEnableTls(true)
	s.SetClientNodeMetadata(clientNodeMetadata)
	s.SetServerNodeMetadata(serverNodeMetadata)
	s.ClientEnvoyTemplate = env.GetTcpClientEnvoyConfTmp()
	s.ServerEnvoyTemplate = env.GetTcpServerEnvoyConfTmp()
	if err := s.SetUpClientServerEnvoy(); err != nil {
		t.Fatalf("Failed to setup te1	st: %v", err)
	}
	defer s.TearDownClientServerEnvoy()

	certPool := x509.NewCertPool()
	bs, err := ioutil.ReadFile("cert-chain.pem")
	if err != nil {
		t.Fatalf("failed to read client ca cert: %s", err)
	}
	ok := certPool.AppendCertsFromPEM(bs)
	if !ok {
		t.Fatal("failed to append client certs")
	}

	certificate, err := tls.LoadX509KeyPair("cert-chain.pem", "key.pem")
	if err != nil {
		t.Fatal("failed to get certificate")
	}
	config := &tls.Config{Certificates: []tls.Certificate{certificate}, ServerName: "localhost", NextProtos: []string{"istio2"}, RootCAs: certPool}

	conn, err := tls.Dial("tcp", fmt.Sprintf("localhost:%d", s.Ports().AppToClientProxyPort), config)
	if err != nil {
		t.Fatal(err)
	}

	conn.Write([]byte("world \n"))
	reply := make([]byte, 256)
	n, err := conn.Read(reply)
	if err != nil {
		t.Fatal(err)
	}

	if fmt.Sprintf("%s", reply[:n]) != "hello world \n" {
		t.Fatalf("Verification Failed. Expected: hello world. Got: %v", fmt.Sprintf("%s", reply[:n]))
	}

	_ = conn.Close()
	s.VerifyStats(getParsedExpectedStats(expectedClientStats, t, s), s.Ports().ClientAdminPort)
	s.VerifyStats(getParsedExpectedStats(expectedServerStats, t, s), s.Ports().ServerAdminPort)
}

func getParsedExpectedStats(expectedStats map[string]int, t *testing.T, s *env.TestSetup) map[string]int {
	parsedExpectedStats := make(map[string]int)
	for key, value := range expectedStats {
		tmpl, err := template.New("parse_state").Parse(key)
		if err != nil {
			t.Errorf("failed to parse config template: %v", err)
		}

		var tpl bytes.Buffer
		err = tmpl.Execute(&tpl, s)
		if err != nil {
			t.Errorf("failed to execute config template: %v", err)
		}
		parsedExpectedStats[tpl.String()] = value
	}

	return parsedExpectedStats
}
