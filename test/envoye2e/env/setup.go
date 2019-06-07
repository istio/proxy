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
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"testing"
	"time"
)

// TestSetup store data for a test.
type TestSetup struct {
	t      *testing.T
	epoch  int
	ports  *Ports

	envoy             *Envoy
	clientEnvoy       *Envoy
	serverEnvoy       *Envoy
	backend           *HTTPServer
	testName          uint16
	stress            bool
	noProxy           bool
	noBackend         bool
	noBackedn2        bool
	disableHotRestart bool
	checkDict         bool

	FiltersBeforeMixer string

	// ClientEnvoyTemplate is the bootstrap config used by client envoy.
	ClientEnvoyTemplate string

	// ServerEnvoyTemplate is the bootstrap config used by client envoy.
	ServerEnvoyTemplate string

	// EnvoyParams contain extra envoy parameters to pass in the CLI (cluster, node)
	EnvoyParams []string

	// EnvoyConfigOpt allows passing additional parameters to the EnvoyTemplate
	EnvoyConfigOpt map[string]interface{}

	// IstioSrc is the base directory of istio sources. May be set for finding testdata or
	// other files in the source tree
	IstioSrc string

	// IstioOut is the base output directory.
	IstioOut string

	// AccessLogPath is the access log path for Envoy
	AccessLogPath string

	// AccessLogPath is the access log path for Envoy
	ClientAccessLogPath string

	// AccessLogPath is the access log path for Envoy
	ServerAccessLogPath string

	// expected source.uid attribute at the mixer gRPC metadata
	mixerSourceUID string

	// Dir is the working dir for envoy
	Dir string
}

func NewClientServerEnvoyTestSetup(name uint16, t *testing.T) *TestSetup {
	return &TestSetup{
		t:             t,
		ports:         NewPorts(name),
		testName:      name,
		ClientAccessLogPath: "/tmp/envoy-client-access.log",
		ServerAccessLogPath: "/tmp/envoy-server-access.log",
	}
}

// Ports get ports object
func (s *TestSetup) Ports() *Ports {
	return s.ports
}

// SetStress set the stress flag
func (s *TestSetup) SetStress(stress bool) {
	s.stress = stress
}

// SetCheckDict set the checkDict flag
func (s *TestSetup) SetCheckDict(checkDict bool) {
	s.checkDict = checkDict
}

// SetDisableHotRestart sets whether disable the HotRestart feature of Envoy
func (s *TestSetup) SetDisableHotRestart(disable bool) {
	s.disableHotRestart = disable
}

// SetNoProxy set NoProxy flag
func (s *TestSetup) SetNoProxy(no bool) {
	s.noProxy = no
}

// SetNoBackend set NoBackend flag
func (s *TestSetup) SetNoBackend(no bool) {
	s.noBackend = no
}

func (s *TestSetup) SetUpClientServerEnvoy() error {
	var err error

	log.Printf("Creating server envoy at %v", s.ports.ServerAdminPort)
	s.serverEnvoy, err = s.NewServerEnvoy()
	if err != nil {
		log.Printf("unable to create Envoy %v", err)
		return err
	}

	log.Printf("Starting server envoy at %v", s.ports.ServerAdminPort)
	err = s.serverEnvoy.Start(s.ports.ServerAdminPort)
	if err != nil {
		return err
	}

	log.Printf("Creating client envoy at %v", s.ports.ClientAdminPort)
	s.clientEnvoy, err = s.NewClientEnvoy()
	if err != nil {
		log.Printf("unable to create Envoy %v", err)
		return err
	}

	log.Printf("Starting client envoy at %v", s.ports.ClientAdminPort)
	err = s.clientEnvoy.Start(s.ports.ClientAdminPort)
	if err != nil {
		return err
	}

	if !s.noBackend {
		s.backend, err = NewHTTPServer(s.ports.BackendPort)
		if err != nil {
			log.Printf("unable to create HTTP server %v", err)
		} else {
			errCh := s.backend.Start()
			if err = <-errCh; err != nil {
				log.Fatalf("backend server start failed %v", err)
			}
		}
	}

	s.WaitClientEnvoyReady()
	s.WaitServerEnvoyReady()

	return nil
}

func (s *TestSetup) TearDownClientServerEnvoy() {
	if err := s.clientEnvoy.Stop(s.Ports().ClientAdminPort); err != nil {
		s.t.Errorf("error quitting client envoy: %v", err)
	}
	s.clientEnvoy.TearDown()

	if err := s.serverEnvoy.Stop(s.Ports().ServerAdminPort); err != nil {
		s.t.Errorf("error quitting client envoy: %v", err)
	}
	s.serverEnvoy.TearDown()

	if s.backend != nil {
		s.backend.Stop()
	}
}

// LastRequestHeaders returns last backend request headers
func (s *TestSetup) LastRequestHeaders() http.Header {
	if s.backend != nil {
		return s.backend.LastRequestHeaders()
	}
	return nil
}

// WaitForStatsUpdateAndGetStats waits for waitDuration seconds to let Envoy update stats, and sends
// request to Envoy for stats. Returns stats response.
func (s *TestSetup) WaitForStatsUpdateAndGetStats(waitDuration int, port uint16) (string, error) {
	time.Sleep(time.Duration(waitDuration) * time.Second)
	statsURL := fmt.Sprintf("http://localhost:%d/stats?format=json&usedonly", port)
	code, respBody, err := HTTPGet(statsURL)
	if err != nil {
		return "", fmt.Errorf("sending stats request returns an error: %v", err)
	}
	if code != 200 {
		return "", fmt.Errorf("sending stats request returns unexpected status code: %d", code)
	}
	return respBody, nil
}

type statEntry struct {
	Name  string `json:"name"`
	Value int    `json:"value"`
}

type stats struct {
	StatList []statEntry `json:"stats"`
}

// WaitEnvoyReady waits until envoy receives and applies all config
func (s *TestSetup) WaitEnvoyReady(port uint16) {
	// Sometimes on circle CI, connection is refused even when envoy reports warm clusters and listeners...
	// Inject a 1 second delay to force readiness
	time.Sleep(1 * time.Second)

	delay := 200 * time.Millisecond
	total := 3 * time.Second
	var stats map[string]int
	for attempt := 0; attempt < int(total/delay); attempt++ {
		statsURL := fmt.Sprintf("http://localhost:%d/stats?format=json&usedonly", port)
		code, respBody, errGet := HTTPGet(statsURL)
		if errGet == nil && code == 200 {
			stats = s.unmarshalStats(respBody)
			warmingListeners, hasListeners := stats["listener_manager.total_listeners_warming"]
			warmingClusters, hasClusters := stats["cluster_manager.warming_clusters"]
			if hasListeners && hasClusters && warmingListeners == 0 && warmingClusters == 0 {
				return
			}
		}
		time.Sleep(delay)
	}

	s.t.Fatalf("envoy failed to get ready: %v", stats)
}

// WaitClientEnvoyReady waits until envoy receives and applies all config
func (s *TestSetup) WaitClientEnvoyReady() {
	s.WaitEnvoyReady(s.Ports().ClientAdminPort)
}

// WaitEnvoyReady waits until envoy receives and applies all config
func (s *TestSetup) WaitServerEnvoyReady() {
	s.WaitEnvoyReady(s.Ports().ServerAdminPort)
}

// UnmarshalStats Unmarshals Envoy stats from JSON format into a map, where stats name is
// key, and stats value is value.
func (s *TestSetup) unmarshalStats(statsJSON string) map[string]int {
	statsMap := make(map[string]int)

	var statsArray stats
	if err := json.Unmarshal([]byte(statsJSON), &statsArray); err != nil {
		s.t.Fatalf("unable to unmarshal stats from json")
	}

	for _, v := range statsArray.StatList {
		statsMap[v.Name] = v.Value
	}
	return statsMap
}

// VerifyStats verifies Envoy stats.
func (s *TestSetup) VerifyStats(expectedStats map[string]int, port uint16) {
	s.t.Helper()

	check := func(actualStatsMap map[string]int) error {
		for eStatsName, eStatsValue := range expectedStats {
			aStatsValue, ok := actualStatsMap[eStatsName]
			if !ok && eStatsValue != 0 {
				return fmt.Errorf("failed to find expected stat %s", eStatsName)
			}
			if aStatsValue != eStatsValue {
				return fmt.Errorf("stats %s does not match. expected vs actual: %d vs %d",
					eStatsName, eStatsValue, aStatsValue)
			}

			log.Printf("stat %s is matched. value is %d", eStatsName, eStatsValue)
		}
		return nil
	}

	delay := 200 * time.Millisecond
	total := 3 * time.Second

	var err error
	for attempt := 0; attempt < int(total/delay); attempt++ {
		statsURL := fmt.Sprintf("http://localhost:%d/stats?format=json&usedonly", port)
		code, respBody, errGet := HTTPGet(statsURL)
		if errGet != nil {
			log.Printf("sending stats request returns an error: %v", errGet)
		} else if code != 200 {
			log.Printf("sending stats request returns unexpected status code: %d", code)
		} else {
			actualStatsMap := s.unmarshalStats(respBody)
			for key, value := range actualStatsMap {
				log.Printf("key: %v, value %v", key, value)
			}
			if err = check(actualStatsMap); err == nil {
				return
			}
			log.Printf("failed to verify stats: %v", err)
		}
		time.Sleep(delay)
	}
	s.t.Errorf("failed to find expected stats: %v", err)
}

// VerifyStatsLT verifies that Envoy stats contains stat expectedStat, whose value is less than
// expectedStatVal.
func (s *TestSetup) VerifyStatsLT(actualStats string, expectedStat string, expectedStatVal int) {
	s.t.Helper()
	actualStatsMap := s.unmarshalStats(actualStats)

	aStatsValue, ok := actualStatsMap[expectedStat]
	if !ok {
		s.t.Fatalf("Failed to find expected Stat %s\n", expectedStat)
	} else if aStatsValue >= expectedStatVal {
		s.t.Fatalf("Stat %s does not match. Expected value < %d, actual stat value is %d",
			expectedStat, expectedStatVal, aStatsValue)
	} else {
		log.Printf("stat %s is matched. %d < %d", expectedStat, aStatsValue, expectedStatVal)
	}
}
