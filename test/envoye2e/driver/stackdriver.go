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

	edgespb "cloud.google.com/go/meshtelemetry/v1alpha1"
	"github.com/golang/protobuf/proto"
	logging "google.golang.org/genproto/googleapis/logging/v2"
	monitoring "google.golang.org/genproto/googleapis/monitoring/v3"
	"istio.io/proxy/test/envoye2e/env"
)

const ResponseLatencyMetricName = "istio.io/service/server/response_latencies"

type Stackdriver struct {
	sync.Mutex

	Port  uint16
	Delay time.Duration

	done  chan error
	tsReq []*monitoring.CreateTimeSeriesRequest
	ts    map[string]struct{}
	ls    map[string]struct{}
	es    map[string]struct{}
}

type SDLogEntry struct {
	LogBaseFile   string
	LogEntryFile  string
	LogEntryCount int
}

var _ Step = &Stackdriver{}

func (sd *Stackdriver) Run(p *Params) error {
	sd.done = make(chan error, 1)
	sd.ls = make(map[string]struct{})
	sd.ts = make(map[string]struct{})
	sd.es = make(map[string]struct{})
	sd.tsReq = make([]*monitoring.CreateTimeSeriesRequest, 0, 20)
	metrics, logging, edge, _, _ := NewFakeStackdriver(sd.Port, sd.Delay, true, ExpectedBearer)

	go func() {
		for {
			select {
			case req := <-metrics.RcvMetricReq:
				log.Printf("sd received metric request: %d\n", len(req.TimeSeries))
				sd.Lock()
				sd.tsReq = append(sd.tsReq, req)
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
			case req := <-edge.RcvTrafficAssertionsReq:
				req.Timestamp = nil
				sd.Lock()
				sd.es[proto.MarshalTextString(req)] = struct{}{}
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

func (sd *Stackdriver) Check(p *Params, tsFiles []string, lsFiles []SDLogEntry, edgeFiles []string) Step {
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
		e := &logging.LogEntry{}
		p.LoadTestProto(l.LogBaseFile, pb)
		p.LoadTestProto(l.LogEntryFile, e)
		for i := 0; i < l.LogEntryCount; i++ {
			pb.Entries = append(pb.Entries, e)
		}
		lwant[proto.MarshalTextString(pb)] = struct{}{}
	}
	ewant := make(map[string]struct{})
	for _, e := range edgeFiles {
		pb := &edgespb.ReportTrafficAssertionsRequest{}
		p.LoadTestProto(e, pb)
		ewant[proto.MarshalTextString(pb)] = struct{}{}
	}
	return &checkStackdriver{
		sd:    sd,
		twant: twant,
		lwant: lwant,
		ewant: ewant,
	}
}

type checkStackdriver struct {
	sd    *Stackdriver
	twant map[string]struct{}
	lwant map[string]struct{}
	ewant map[string]struct{}
}

func (s *checkStackdriver) Run(p *Params) error {
	foundAllLogs := false
	foundAllMetrics := false
	foundAllEdge := false
	verfiedLatency := false
	for i := 0; i < 30; i++ {
		s.sd.Lock()
		if len(s.lwant) == 0 {
			foundAllLogs = true
		} else {
			foundAllLogs = reflect.DeepEqual(s.sd.ls, s.lwant)
		}
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

		if len(s.twant) == 0 {
			foundAllMetrics = true
		} else {
			foundAllMetrics = reflect.DeepEqual(s.sd.ts, s.twant)
		}
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

		if len(s.ewant) == 0 {
			foundAllEdge = true
		} else {
			foundAllEdge = reflect.DeepEqual(s.sd.es, s.ewant)
		}
		if !foundAllEdge {
			log.Printf("got edges %d, want %d\n", len(s.sd.es), len(s.ewant))
			if len(s.sd.es) >= len(s.ewant) {
				for got := range s.sd.es {
					log.Println(got)
				}
				log.Println("--- but want ---")
				for want := range s.ewant {
					log.Println(want)
				}
				return fmt.Errorf("failed to receive expected edges")
			}
		}

		// Sanity check response latency
		for _, r := range s.sd.tsReq {
			if verfied, err := verifyResponseLatency(r); err != nil {
				return fmt.Errorf("failed to verify latency metric: %v", err)
			} else if verfied {
				verfiedLatency = true
				break
			}
		}
		s.sd.Unlock()

		if foundAllLogs && foundAllMetrics && foundAllEdge && verfiedLatency {
			return nil
		}

		log.Println("sleeping till next check")
		time.Sleep(1 * time.Second)
	}
	return fmt.Errorf("found all metrics %v, all logs %v, all edge %v, verified latency %v", foundAllMetrics, foundAllLogs, foundAllEdge, verfiedLatency)
}

func (s *checkStackdriver) Cleanup() {}

// Check that response latency is within a reasonable range (less than 256 milliseconds).
func verifyResponseLatency(got *monitoring.CreateTimeSeriesRequest) (bool, error) {
	for _, t := range got.TimeSeries {
		if t.Metric.Type != ResponseLatencyMetricName {
			continue
		}
		p := t.Points[0]
		d := p.Value.GetDistributionValue()
		bo := d.GetBucketOptions()
		if bo == nil {
			return true, fmt.Errorf("expect response latency metrics bucket option not to be empty: %v", got)
		}
		eb := bo.GetExplicitBuckets()
		if eb == nil {
			return true, fmt.Errorf("explicit response latency metrics buckets should not be empty: %v", got)
		}
		bounds := eb.GetBounds()
		maxLatencyInMilli := 0.0
		for i, b := range d.GetBucketCounts() {
			if b != 0 {
				maxLatencyInMilli = bounds[i]
			}
		}
		wantMaxLatencyInMilli := 256.0
		if env.IsTSanASan() {
			wantMaxLatencyInMilli = 1024.0
		}
		if maxLatencyInMilli > wantMaxLatencyInMilli {
			return true, fmt.Errorf("latency metric is too large, got %vms, but want < %vms", maxLatencyInMilli, wantMaxLatencyInMilli)
		}
		return true, nil
	}
	return false, nil
}
