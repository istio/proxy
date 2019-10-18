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
	}
	Step interface {
		Run(*Params) error
		Cleanup()
	}
	Scenario struct {
		Steps []Step
	}
	Repeat struct {
		N    int
		Step Step
	}
	Sleep struct {
		time.Duration
	}
)

var _ Step = &Repeat{}

func (r *Repeat) Run(p *Params) error {
	for i := 0; i < r.N; i++ {
		log.Printf("repeat %d out of %d", i, r.N)
		if err := r.Step.Run(p); err != nil {
			return err
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
	t := template.Must(template.New("params").Option("missingkey=zero").Parse(s))
	var b bytes.Buffer
	if err := t.Execute(&b, p); err != nil {
		return "", err
	}
	return b.String(), nil
}

func (s *Scenario) Run(p *Params) error {
	log.Printf("Parameters %#v\n", p)
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

func Counter(base int) func() int {
	state := base - 1
	return func() int {
		state++
		return state
	}
}
