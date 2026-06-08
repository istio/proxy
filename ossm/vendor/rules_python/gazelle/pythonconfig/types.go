// Copyright 2023 The Bazel Authors. All rights reserved.
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

package pythonconfig

import (
	"fmt"
	"sort"
	"strings"
)

// StringSet satisfies the flag.Value interface. It constructs a set backed by
// a hashmap by parsing the flag string value using the provided separator.
type StringSet struct {
	set       map[string]struct{}
	separator string
}

// NewStringSet constructs a new StringSet with the given separator.
func NewStringSet(separator string) *StringSet {
	return &StringSet{
		set:       make(map[string]struct{}),
		separator: separator,
	}
}

// String satisfies flag.Value.String.
func (ss *StringSet) String() string {
	keys := make([]string, 0, len(ss.set))
	for key := range ss.set {
		keys = append(keys, key)
	}
	return fmt.Sprintf("%v", sort.StringSlice(keys))
}

// Set satisfies flag.Value.Set.
func (ss *StringSet) Set(s string) error {
	list := strings.Split(s, ss.separator)
	for _, v := range list {
		trimmed := strings.TrimSpace(v)
		if trimmed == "" {
			continue
		}
		ss.set[trimmed] = struct{}{}
	}
	return nil
}

// Contains returns whether the StringSet contains the provided element or not.
func (ss *StringSet) Contains(s string) bool {
	_, contains := ss.set[s]
	return contains
}

// StringMapList satisfies the flag.Value interface. It constructs a string map
// by parsing the flag string value using the provided list and map separators.
type StringMapList struct {
	mapping       map[string]string
	listSeparator string
	mapSeparator  string
}

// NewStringMapList constructs a new StringMapList with the given separators.
func NewStringMapList(listSeparator, mapSeparator string) *StringMapList {
	return &StringMapList{
		mapping:       make(map[string]string),
		listSeparator: listSeparator,
		mapSeparator:  mapSeparator,
	}
}

// String satisfies flag.Value.String.
func (sml *StringMapList) String() string {
	return fmt.Sprintf("%v", sml.mapping)
}

// Set satisfies flag.Value.Set.
func (sml *StringMapList) Set(s string) error {
	list := strings.Split(s, sml.listSeparator)
	for _, v := range list {
		trimmed := strings.TrimSpace(v)
		if trimmed == "" {
			continue
		}
		mapList := strings.SplitN(trimmed, sml.mapSeparator, 2)
		if len(mapList) < 2 {
			return fmt.Errorf(
				"%q is not a valid map using %q as a separator",
				trimmed, sml.mapSeparator,
			)
		}
		key := mapList[0]
		if _, exists := sml.mapping[key]; exists {
			return fmt.Errorf("key %q already set", key)
		}
		val := mapList[1]
		sml.mapping[key] = val
	}
	return nil
}

// Get returns the value for the given key.
func (sml *StringMapList) Get(key string) (string, bool) {
	val, exists := sml.mapping[key]
	return val, exists
}