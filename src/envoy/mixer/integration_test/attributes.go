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

	"github.com/istio/mixer/pkg/attribute"
)

func (rb *attribute.MutableBag) getAllKeys() map[string]bool {
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
