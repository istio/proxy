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
	"fmt"
	"log"
	"reflect"
	"strings"
	"sync"
	"time"

	"github.com/golang/protobuf/proto"
	logging "google.golang.org/genproto/googleapis/logging/v2"
	monitoring "google.golang.org/genproto/googleapis/monitoring/v3"
	fs "istio.io/proxy/test/envoye2e/stackdriver_plugin/fake_stackdriver"
)

type Stackdriver struct {
	sync.Mutex

	Port  uint16
	Delay time.Duration

	done chan error
	ts   map[string]struct{}
	ls   map[string]struct{}
}

var _ Step = &Stackdriver{}

func (sd *Stackdriver) Run(p *Params) error {
	sd.done = make(chan error, 1)
	sd.ls = make(map[string]struct{})
	sd.ts = make(map[string]struct{})
	metrics, logging, _ := fs.NewFakeStackdriver(sd.Port, sd.Delay)

	go func() {
		for {
			select {
			case req := <-metrics.RcvMetricReq:
				log.Printf("sd received metric request: %d\n", len(req.TimeSeries))
				sd.Lock()
				for _, ts := range req.TimeSeries {
					if strings.HasSuffix(ts.Metric.Type, "request_count") {
						// clear the timestamps for comparison
						ts.Points[0].Interval = nil
						sd.ts[proto.MarshalTextString(ts)] = struct{}{}
					} else {
						log.Printf("skipping metric type %q\n", ts.Metric.Type)
					}
				}
				sd.Unlock()
			case req := <-logging.RcvLoggingReq:
				log.Println("sd received log request")
				// clear the timestamps, latency request id, and req/resp size for comparison
				for _, entry := range req.Entries {
					entry.Timestamp = nil
					entry.HttpRequest.RequestSize = 0
					entry.HttpRequest.ResponseSize = 0
					entry.HttpRequest.Latency = nil
					entry.HttpRequest.RemoteIp = ""
					delete(entry.Labels, "request_id")
				}
				sd.Lock()
				sd.ls[proto.MarshalTextString(req)] = struct{}{}
				sd.Unlock()
			case <-sd.done:
				return
			}
		}
	}()

	return nil
}

func (sd *Stackdriver) Cleanup() {
	close(sd.done)
}

func (sd *Stackdriver) Check(p *Params, tsFiles []string, lsFiles []string) Step {
	// check as sets of strings by marshaling to proto
	twant := make(map[string]struct{})
	for _, t := range tsFiles {
		pb := &monitoring.TimeSeries{}
		p.LoadTestProto(t, pb)
		twant[proto.MarshalTextString(pb)] = struct{}{}
	}
	lwant := make(map[string]struct{})
	for _, l := range lsFiles {
		pb := &logging.WriteLogEntriesRequest{}
		p.LoadTestProto(l, pb)
		lwant[proto.MarshalTextString(pb)] = struct{}{}
	}
	return &checkStackdriver{
		sd:    sd,
		twant: twant,
		lwant: lwant,
	}
}

type checkStackdriver struct {
	sd    *Stackdriver
	twant map[string]struct{}
	lwant map[string]struct{}
}

func (s *checkStackdriver) Run(p *Params) error {
	foundAllLogs := false
	foundAllMetrics := false
	for i := 0; i < 30; i++ {
		s.sd.Lock()
		foundAllLogs = reflect.DeepEqual(s.sd.ls, s.lwant)
		if !foundAllLogs {
			log.Printf("got log entries %d, want %d\n", len(s.sd.ls), len(s.lwant))
			if len(s.sd.ls) >= len(s.lwant) {
				for got := range s.sd.ls {
					log.Println(got)
				}
				log.Println("--- but want ---")
				for want := range s.lwant {
					log.Println(want)
				}
				return fmt.Errorf("failed to receive expected logs")
			}
		}

		foundAllMetrics = reflect.DeepEqual(s.sd.ts, s.twant)
		if !foundAllMetrics {
			log.Printf("got metrics %d, want %d\n", len(s.sd.ts), len(s.twant))
			if len(s.sd.ts) >= len(s.twant) {
				for got := range s.sd.ts {
					log.Println(got)
				}
				log.Println("--- but want ---")
				for want := range s.twant {
					log.Println(want)
				}
				return fmt.Errorf("failed to receive expected metrics")
			}
		}
		s.sd.Unlock()

		if foundAllLogs && foundAllMetrics {
			return nil
		}

		log.Println("sleeping till next check")
		time.Sleep(1 * time.Second)
	}
	return fmt.Errorf("found all metrics %v, all logs %v", foundAllMetrics, foundAllLogs)
}

func (s *checkStackdriver) Cleanup() {}
