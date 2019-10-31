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
	"bytes"
	"log"
	"strings"
	"text/template"
	"time"

	"github.com/envoyproxy/go-control-plane/pkg/cache"
	"github.com/ghodss/yaml"
	"github.com/golang/protobuf/jsonpb"
	"github.com/golang/protobuf/proto"
)

type (
	Params struct {
		XDS    int
		Config cache.SnapshotCache
		Vars   map[string]string
		N      int
	}
	Step interface {
		Run(*Params) error
		Cleanup()
	}
	Scenario struct {
		Steps []Step
	}
	// Repeat a step either for N number or duration
	Repeat struct {
		N        int
		Duration time.Duration
		Step     Step
	}
	Sleep struct {
		time.Duration
	}
	// Fork will copy params to avoid concurrent access
	Fork struct {
		Fore Step
		Back Step
	}
)

var _ Step = &Repeat{}

func (r *Repeat) Run(p *Params) error {
	if r.Duration != 0 {
		start := time.Now()
		p.N = 0
		for {
			if time.Since(start) >= r.Duration {
				break
			}
			log.Printf("repeat %d elapsed %v out of %v", p.N, time.Since(start), r.Duration)
			if err := r.Step.Run(p); err != nil {
				return err
			}
			p.N++
		}
	} else {
		for i := 0; i < r.N; i++ {
			log.Printf("repeat %d out of %d", i, r.N)
			p.N = i
			if err := r.Step.Run(p); err != nil {
				return err
			}
		}
	}
	return nil
}
func (r *Repeat) Cleanup() {}

var _ Step = &Sleep{}

func (s *Sleep) Run(_ *Params) error {
	log.Printf("sleeping %v\n", s.Duration)
	time.Sleep(s.Duration)
	return nil
}
func (s *Sleep) Cleanup() {}

func (p *Params) Fill(s string) (string, error) {
	t := template.Must(template.New("params").
		Option("missingkey=zero").
		Funcs(template.FuncMap{
			"indent": func(n int, s string) string {
				pad := strings.Repeat(" ", n)
				return pad + strings.Replace(s, "\n", "\n"+pad, -1)
			},
		}).
		Parse(s))
	var b bytes.Buffer
	if err := t.Execute(&b, p); err != nil {
		return "", err
	}
	return b.String(), nil
}

var _ Step = &Fork{}

func (f *Fork) Run(p *Params) error {
	done := make(chan error, 1)
	go func() {
		p2 := *p
		done <- f.Back.Run(&p2)
	}()

	if err := f.Fore.Run(p); err != nil {
		return err
	}

	return <-done
}
func (f *Fork) Cleanup() {}

var _ Step = &Scenario{}

func (s *Scenario) Run(p *Params) error {
	passed := make([]Step, 0, len(s.Steps))
	defer func() {
		for i := range passed {
			passed[len(passed)-1-i].Cleanup()
		}
	}()
	for _, step := range s.Steps {
		if err := step.Run(p); err != nil {
			return err
		}
		passed = append(passed, step)
	}
	return nil
}

func (s *Scenario) Cleanup() {}

func ReadYAML(input string, pb proto.Message) error {
	js, err := yaml.YAMLToJSON([]byte(input))
	if err != nil {
		return err
	}
	reader := strings.NewReader(string(js))
	m := jsonpb.Unmarshaler{}
	return m.Unmarshal(reader, pb)
}

func (p *Params) FillYAML(input string, pb proto.Message) error {
	out, err := p.Fill(input)
	if err != nil {
		return err
	}
	return ReadYAML(out, pb)
}
