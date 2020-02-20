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

package driver

import (
	"context"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"time"

	"istio.io/proxy/test/envoye2e/env"
)

type SecureTokenService struct {
	Port   uint16
	server *http.Server
}

const (
	ExpectedTokenRequest = "grant_type=urn:ietf:params:oauth:grant-type:token-exchange&subject_token=kombucha&subject_token_type=urn:ietf:params:oauth:token-type:jwt&scope=https://www.googleapis.com/auth/cloud-platform"
	ExpectedBearer       = "kvass"
)

var (
	ExpectedTokenResponse = fmt.Sprintf(`{
 "access_token": "%s",
 "issued_token_type": "urn:ietf:params:oauth:token-type:access_token",
 "token_type": "Bearer",
 "expires_in": 180
}`, ExpectedBearer)
)

var _ Step = &SecureTokenService{}

func (sts *SecureTokenService) Run(_ *Params) error {
	sts.server = &http.Server{
		Addr:         fmt.Sprintf(":%d", sts.Port),
		Handler:      sts,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 10 * time.Second,
	}
	go func() {
		sts.server.ListenAndServe()
	}()
	return env.WaitForHTTPServer(fmt.Sprintf("http://localhost:%d/health", sts.Port))
}

func (sts *SecureTokenService) ServeHTTP(resp http.ResponseWriter, req *http.Request) {
	switch path := req.URL.Path; {
	case path == "/health" && req.Method == http.MethodGet:
		resp.WriteHeader(http.StatusOK)
	case path == "/token" && req.Method == http.MethodPost:
		resp.WriteHeader(http.StatusOK)
		body, _ := ioutil.ReadAll(req.Body)
		if string(body) == ExpectedTokenRequest {
			resp.Write([]byte(ExpectedTokenResponse))
		} else {
			log.Printf("STS: unexpected request body %q\n", string(body))
		}
	default:
		resp.WriteHeader(http.StatusNotFound)
	}
}

func (sts *SecureTokenService) Cleanup() {
	sts.server.Shutdown(context.Background())
}
