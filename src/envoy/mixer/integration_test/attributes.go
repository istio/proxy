// Copyright 2017 Istio Authors
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
	"encoding/json"
	"fmt"
	"reflect"
	"time"

	ptypes "github.com/gogo/protobuf/types"
	mixerpb "istio.io/api/mixer/v1"
)

type Dictionary map[int32]string

type Bag struct {
	strings    map[string]string
	int64s     map[string]int64
	float64s   map[string]float64
	bools      map[string]bool
	times      map[string]time.Time
	durations  map[string]time.Duration
	bytes      map[string][]uint8
	stringMaps map[string]map[string]string
}

func newBag() *Bag {
	return &Bag{
		strings:    make(map[string]string),
		int64s:     make(map[string]int64),
		float64s:   make(map[string]float64),
		bools:      make(map[string]bool),
		times:      make(map[string]time.Time),
		durations:  make(map[string]time.Duration),
		bytes:      make(map[string][]uint8),
		stringMaps: make(map[string]map[string]string),
	}
}

func (rb *Bag) update(dictionary Dictionary, attrs *mixerpb.Attributes) error {
	// delete requested attributes
	for _, d := range attrs.DeletedAttributes {
		if name, present := dictionary[d]; present {
			delete(rb.strings, name)
			delete(rb.int64s, name)
			delete(rb.float64s, name)
			delete(rb.bools, name)
			delete(rb.times, name)
			delete(rb.durations, name)
			delete(rb.bytes, name)
			delete(rb.stringMaps, name)
		}
	}

	// apply all attributes
	for k, v := range attrs.StringAttributes {
		rb.strings[dictionary[k]] = v
	}
	for k, v := range attrs.Int64Attributes {
		rb.int64s[dictionary[k]] = v
	}
	for k, v := range attrs.DoubleAttributes {
		rb.float64s[dictionary[k]] = v
	}
	for k, v := range attrs.BoolAttributes {
		rb.bools[dictionary[k]] = v
	}
	for k, v := range attrs.TimestampAttributes {
		rb.times[dictionary[k]], _ = ptypes.TimestampFromProto(v)
	}
	for k, v := range attrs.DurationAttributes {
		rb.durations[dictionary[k]], _ = ptypes.DurationFromProto(v)
	}
	for k, v := range attrs.BytesAttributes {
		rb.bytes[dictionary[k]] = v
	}
	for k, v := range attrs.StringMapAttributes {
		m := rb.stringMaps[dictionary[k]]
		if m == nil {
			m = make(map[string]string)
			rb.stringMaps[dictionary[k]] = m
		}
		for k2, v2 := range v.Map {
			m[dictionary[k2]] = v2
		}
	}

	return nil
}

func (rb *Bag) getAllKeys() map[string]bool {
	all_keys := make(map[string]bool)
	for k := range rb.strings {
		all_keys[k] = true
	}
	for k := range rb.int64s {
		all_keys[k] = true
	}
	for k := range rb.float64s {
		all_keys[k] = true
	}
	for k := range rb.bools {
		all_keys[k] = true
	}
	for k := range rb.times {
		all_keys[k] = true
	}
	for k := range rb.durations {
		all_keys[k] = true
	}
	for k := range rb.bytes {
		all_keys[k] = true
	}
	for k := range rb.stringMaps {
		all_keys[k] = true
	}
	return all_keys
}

func verifyStringMap(actual map[string]string, expected map[string]interface{}) error {
	for k, v := range expected {
		vstring := v.(string)
		// "-" make sure the key does not exist.
		if vstring == "-" {
			if _, ok := actual[k]; ok {
				return fmt.Errorf("key %+v is NOT expected", k)
			}
		} else {
			if val, ok := actual[k]; ok {
				// "*" only check key exist
				if val != vstring && vstring != "*" {
					return fmt.Errorf("key %+v value doesn't match. Actual %+v, expected %+v",
						k, val, vstring)
				}
			} else {
				return fmt.Errorf("key %+v is expected", k)
			}
		}
	}
	return nil
}

// Please see the comment at top of mixer_test.go for verification rules
func (rb *Bag) Verify(json_results string) error {
	var r map[string]interface{}
	if err := json.Unmarshal([]byte(json_results), &r); err != nil {
		return fmt.Errorf("unable to decode json %v", err)
	}

	all_keys := rb.getAllKeys()

	for k, v := range r {
		switch vv := v.(type) {
		case string:
			// "*" means only checking key.
			if vv == "*" {
				if _, ok := all_keys[k]; !ok {
					return fmt.Errorf("attribute %+v is expected", k)
				}
			} else {
				if val, ok := rb.strings[k]; ok {
					vstring := v.(string)
					if val != vstring {
						return fmt.Errorf("attribute %+v value doesn't match. Actual %+v, expected %+v",
							k, val, vstring)
					}
				} else {
					return fmt.Errorf("attribute %+v is expected", k)
				}
			}
		case float64:
			// Json converts all integers to float64,
			// Our tests only verify size related attributes which are int64 type
			if val, ok := rb.int64s[k]; ok {
				vint64 := int64(vv)
				if val != vint64 {
					return fmt.Errorf("attribute %+v value doesn't match. Actual %+v, expected %+v",
						k, val, vint64)
				}
			} else {
				return fmt.Errorf("attribute %+v is expected", k)
			}
		case map[string]interface{}:
			if val, ok := rb.stringMaps[k]; ok {
				if err := verifyStringMap(val, v.(map[string]interface{})); err != nil {
					return fmt.Errorf("attribute %+v StringMap doesn't match: %+v", k, err)
				}
			} else {
				return fmt.Errorf("attribute %+v is expected", k)
			}
		default:
			return fmt.Errorf("attribute %+v is of a type %+v that I don't know how to handle ",
				k, reflect.TypeOf(v))
		}
		delete(all_keys, k)

	}

	if len(all_keys) > 0 {
		var s string
		for k, _ := range all_keys {
			s += k + ", "
		}
		return fmt.Errorf("Following attributes are not expected: %s", s)
	}
	return nil
}

type Context struct {
	dict Dictionary
	curr *Bag
}

func NewContext() *Context {
	return &Context{
		curr: newBag(),
	}
}

func (c *Context) Update(attrs *mixerpb.Attributes) error {
	if len(attrs.Dictionary) > 0 {
		c.dict = attrs.Dictionary
	}
	return c.curr.update(c.dict, attrs)
}
