// Copyright 2017 Istio Authors. All Rights Reserved.
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

package test

import (
	"fmt"
	"testing"

	rpc "github.com/googleapis/googleapis/google/rpc"
)

const (
	okRequestNum = 10
	// Pool may have some prefetched tokens.
	// In order to see rejected request, reject request num > 20
	// the minPrefetch * 2.
	rejectRequestNum = 30
)

func TestQuotaCache(t *testing.T) {
	s, err := SetUp(t, basicConfig+","+quotaCacheConfig)
	if err != nil {
		t.Fatalf("Failed to setup test: %v", err)
	}
	defer s.TearDown()

	// drain all channel.
	s.DrainMixerAllChannels()

	url := fmt.Sprintf("http://localhost:%d/echo", ClientProxyPort)

	// Issues a GET echo request with 0 size body
	tag := "OKGet"
	ok := 0
	reject := 0
	// Will trigger a new prefetch after half of minPrefech is used.
	for i := 0; i < okRequestNum; i++ {
		code, _, err := HTTPGet(url)
		if err != nil {
			t.Errorf("Failed in request %s: %v", tag, err)
		}
		if code == 200 {
			ok++
		} else {
			reject++
		}
	}
	// Prefetch quota calls should be very small, less than 5.
	if s.mixer.quota.count >= okRequestNum/2 {
		s.t.Fatalf("%s mixer quota call count: %v, should be less than %v",
			tag, s.mixer.quota.count, okRequestNum/2)
	}
	if ok < okRequestNum {
		s.t.Fatalf("%s granted request count: %v, should be %v",
			tag, ok, okRequestNum)
	}

	// Reject the quota call from Mixer.
	tag = "QuotaFail"
	s.mixer.quota.r_status = rpc.Status{
		Code:    int32(rpc.RESOURCE_EXHAUSTED),
		Message: "Not enought qouta.",
	}
	for i := 0; i < rejectRequestNum; i++ {
		code, _, err := HTTPGet(url)
		if err != nil {
			t.Errorf("Failed in request %s: %v", tag, err)
		}
		if code == 200 {
			ok++
		} else {
			reject++
		}
	}
	// Prefetch quota calls should be very small, less than 10.
	if s.mixer.quota.count >= okRequestNum {
		s.t.Fatalf("%s mixer quota call count: %v, should be less than %v",
			tag, s.mixer.quota.count, okRequestNum)
	}
	if reject == 0 {
		s.t.Fatalf("%s rejected request count: %v should not be zero.",
			tag, reject)
	}
}
