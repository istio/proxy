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

package env

import (
	"log"
)

// Dynamic port allocation scheme
// In order to run the tests in parallel. Each test should use unique ports
// Each test has a unique test_name, its ports will be allocated based on that name

// All tests should be listed here to get their test ids
const (
	BasicFlowTest uint16 = iota

	BasicTCPFlowTest

	StackdriverPluginTest

	TCPMetadataExchangeTest
	TCPMetadataExchangeFailTest
	HTTPMetadataExchangeTest

	// xDS driven tests
	BasicHTTP
	BasicHTTPwithTLS
	HTTPExchange
	StackDriverPayload
	StackDriverPayloadGateway
	StackDriverPayloadWithTLS
	StackDriverReload
	StackDriverVMReload
	StackDriverParallel
	BasicHTTPGateway
	StatsPayload
	StatsParallel
	StatsWasm

	StatsPluginTest

	// The number of total tests. has to be the last one.
	maxTestNum
)

const (
	portBase uint16 = 20000
	// Maximum number of ports used in each test.
	portNum uint16 = 20
)

// Ports stores all used ports
type Ports struct {
	ClientAdminPort         uint16
	AppToClientProxyPort    uint16
	ClientToServerProxyPort uint16
	ServerAdminPort         uint16
	// Port used for xDS server
	XDSPort uint16
	// Port used for StackDriver
	SDPort uint16
	// Port used for Secure Token Service
	STSPort uint16

	BackendPort uint16
	ClientAdmin uint16
	ClientPort  uint16
	ServerPort  uint16
	ServerAdmin uint16
}

func allocPortBase(name uint16) uint16 {
	base := portBase + name*portNum
	for i := 0; i < 10; i++ {
		if allPortFree(base, portNum) {
			return base
		}
		base += maxTestNum * portNum
	}
	log.Println("could not find free ports, continue the test...")
	return base
}

func allPortFree(base uint16, ports uint16) bool {
	for port := base; port < base+ports; port++ {
		if IsPortUsed(port) {
			log.Println("port is used ", port)
			return false
		}
	}
	return true
}

// NewPorts allocate all ports based on test id.
func NewPorts(name uint16) *Ports {
	base := allocPortBase(name)
	return &Ports{
		BackendPort:             base,
		ClientAdminPort:         base + 1,
		ClientAdmin:             base + 1,
		AppToClientProxyPort:    base + 2,
		ClientPort:              base + 2,
		ClientToServerProxyPort: base + 3,
		ServerPort:              base + 3,
		ServerAdminPort:         base + 4,
		ServerAdmin:             base + 4,
		XDSPort:                 base + 5,
		SDPort:                  base + 6,
		STSPort:                 base + 7,
	}
}
