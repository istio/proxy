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

package stackdriverplugin

import (
	"fmt"
	"log"
	"reflect"
	"strings"
	"sync"
	"time"

	"cloud.google.com/go/logging/apiv2/loggingpb"
	"cloud.google.com/go/monitoring/apiv3/v2/monitoringpb"
	"github.com/google/go-cmp/cmp"
	metric "google.golang.org/genproto/googleapis/api/metric"
	"google.golang.org/protobuf/encoding/prototext"
	"google.golang.org/protobuf/testing/protocmp"

	"istio.io/proxy/test/envoye2e/driver"
	"istio.io/proxy/test/envoye2e/env"
)

const ResponseLatencyMetricName = "istio.io/service/server/response_latencies"

type Stackdriver struct {
	sync.Mutex

	Port  uint16
	Delay time.Duration

	done  chan error
	tsReq []*monitoringpb.CreateTimeSeriesRequest
	ts    map[string]int64
	ls    map[string]struct{}
}

type SDLogEntry struct {
	LogBaseFile   string
	LogEntryFile  []string
	LogEntryCount int
}

var _ driver.Step = &Stackdriver{}

func (sd *Stackdriver) Run(p *driver.Params) error {
	sd.done = make(chan error, 1)
	sd.ls = make(map[string]struct{})
	sd.ts = make(map[string]int64)
	sd.tsReq = make([]*monitoringpb.CreateTimeSeriesRequest, 0, 20)
	metrics, logging, _, _ := NewFakeStackdriver(sd.Port, sd.Delay, true, ExpectedBearer)

	go func() {
		for {
			select {
			case req := <-metrics.RcvMetricReq:
				log.Printf("sd received metric request: %d\n", len(req.TimeSeries))
				sd.Lock()
				sd.tsReq = append(sd.tsReq, req)
				for _, ts := range req.TimeSeries {
					if strings.HasSuffix(ts.Metric.Type, "request_count") ||
						strings.HasSuffix(ts.Metric.Type, "connection_open_count") ||
						strings.HasSuffix(ts.Metric.Type, "request_bytes") ||
						strings.HasSuffix(ts.Metric.Type, "received_bytes_count") {
						// clear the timestamps for comparison
						key := prototext.Format(&monitoringpb.TimeSeries{
							Metric:     ts.Metric,
							Resource:   ts.Resource,
							Metadata:   ts.Metadata,
							MetricKind: ts.MetricKind,
							ValueType:  ts.ValueType,
						})
						for _, point := range ts.Points {
							point.Interval = nil
							if ts.MetricKind == metric.MetricDescriptor_DELTA {
								sd.ts[key] += point.Value.GetInt64Value()
							} else {
								sd.ts[key] = point.Value.GetInt64Value()
							}
						}
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
					if entry.HttpRequest != nil {
						entry.HttpRequest.RequestSize = 0
						entry.HttpRequest.ResponseSize = 0
						entry.HttpRequest.Latency = nil
						entry.HttpRequest.RemoteIp = ""
					}
					delete(entry.Labels, "request_id")
					delete(entry.Labels, "source_ip")
					delete(entry.Labels, "source_port")
					delete(entry.Labels, "destination_port")
					delete(entry.Labels, "total_sent_bytes")
					delete(entry.Labels, "total_received_bytes")
					delete(entry.Labels, "connection_id")
					delete(entry.Labels, "upstream_host")
				}
				sd.Lock()
				sd.ls[prototext.Format(req)] = struct{}{}
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

func (sd *Stackdriver) Check(p *driver.Params, tsFiles []string, lsFiles []SDLogEntry, verifyLatency bool) driver.Step {
	// check as sets of strings by marshaling to proto
	twant := make(map[string]int64)
	for _, t := range tsFiles {
		pb := &monitoringpb.TimeSeries{}
		p.LoadTestProto(t, pb)
		if len(pb.Points) != 1 || pb.Points[0].Value.GetInt64Value() == 0 {
			log.Fatal("malformed metric golden")
		}
		point := pb.Points[0]
		pb.Points = nil
		twant[prototext.Format(pb)] = point.Value.GetInt64Value()
	}
	lwant := make(map[string]struct{})
	for _, l := range lsFiles {
		pb := &loggingpb.WriteLogEntriesRequest{}
		p.LoadTestProto(l.LogBaseFile, pb)
		for i := 0; i < l.LogEntryCount; i++ {
			for _, logEntryFile := range l.LogEntryFile {
				e := &loggingpb.LogEntry{}
				p.LoadTestProto(logEntryFile, e)
				pb.Entries = append(pb.Entries, e)
			}
		}
		lwant[prototext.Format(pb)] = struct{}{}
	}
	return &checkStackdriver{
		sd:                    sd,
		twant:                 twant,
		lwant:                 lwant,
		verifyResponseLatency: verifyLatency,
	}
}

func (sd *Stackdriver) Reset() driver.Step {
	return &resetStackdriver{sd: sd}
}

type resetStackdriver struct {
	sd *Stackdriver
}

func (r *resetStackdriver) Run(p *driver.Params) error {
	r.sd.Lock()
	defer r.sd.Unlock()
	r.sd.ls = make(map[string]struct{})
	r.sd.ts = make(map[string]int64)
	r.sd.tsReq = make([]*monitoringpb.CreateTimeSeriesRequest, 0, 20)
	return nil
}

func (r *resetStackdriver) Cleanup() {}

type checkStackdriver struct {
	sd                    *Stackdriver
	twant                 map[string]int64
	lwant                 map[string]struct{}
	verifyResponseLatency bool
}

func (s *checkStackdriver) Run(p *driver.Params) error {
	foundAllLogs := false
	foundAllMetrics := false
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
				// Adding more logs for debugging in case of failures.
				if diff := cmp.Diff(s.sd.ls, s.lwant, protocmp.Transform()); diff != "" {
					log.Printf("t diff: %v\ngot:\n %v\nwant:\n %v\n", diff, s.sd.ls, s.lwant)
				}
				return fmt.Errorf("failed to receive expected logs")
			}
		}
		if len(s.twant) == 0 {
			foundAllMetrics = true
		} else {
			foundAllMetrics = reflect.DeepEqual(s.sd.ts, s.twant)
		}
		if !s.verifyResponseLatency {
			verfiedLatency = true
		} else {
			// Sanity check response latency
			for _, r := range s.sd.tsReq {
				if verfied, err := verifyResponseLatency(r); err != nil {
					return fmt.Errorf("failed to verify latency metric: %v", err)
				} else if verfied {
					verfiedLatency = true
					break
				}
			}
		}
		s.sd.Unlock()

		if foundAllLogs && foundAllMetrics && verfiedLatency {
			return nil
		}

		log.Println("sleeping till next check")
		time.Sleep(1 * time.Second)
	}
	if !foundAllMetrics {
		log.Printf("got metrics %d, want %d\n", len(s.sd.ts), len(s.twant))
		for got, value := range s.sd.ts {
			log.Printf("%s=%d\n", got, value)
		}
		log.Println("--- but want ---")
		for want, value := range s.twant {
			log.Printf("%s=%d\n", want, value)
		}
	}

	return fmt.Errorf("found all metrics %v, all logs %v, verified latency %v", foundAllMetrics, foundAllLogs, verfiedLatency)
}

func (s *checkStackdriver) Cleanup() {}

// Check that response latency is within a reasonable range (less than 256 milliseconds).
func verifyResponseLatency(got *monitoringpb.CreateTimeSeriesRequest) (bool, error) {
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
