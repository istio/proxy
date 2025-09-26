// Copyright 2017 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package main

import (
	"errors"
	"fmt"
	"go/build"
	"strings"
	"unicode"
)

// multiFlag allows repeated string flags to be collected into a slice
type multiFlag []string

func (m *multiFlag) String() string {
	if m == nil || len(*m) == 0 {
		return ""
	}
	return fmt.Sprint(*m)
}

func (m *multiFlag) Set(v string) error {
	(*m) = append(*m, v)
	return nil
}

// quoteMultiFlag allows repeated string flags to be collected into a slice.
// Flags are split on spaces. Single quotes are removed, and spaces within
// quotes are removed. Literal quotes may be escaped with a backslash.
type quoteMultiFlag []string

func (m *quoteMultiFlag) String() string {
	if m == nil || len(*m) == 0 {
		return ""
	}
	return fmt.Sprint(*m)
}

func (m *quoteMultiFlag) Set(v string) error {
	fs, err := splitQuoted(v)
	if err != nil {
		return err
	}
	*m = append(*m, fs...)
	return nil
}

// splitQuoted splits the string s around each instance of one or more consecutive
// white space characters while taking into account quotes and escaping, and
// returns an array of substrings of s or an empty list if s contains only white space.
// Single quotes and double quotes are recognized to prevent splitting within the
// quoted region, and are removed from the resulting substrings. If a quote in s
// isn't closed err will be set and r will have the unclosed argument as the
// last element. The backslash is used for escaping.
//
// For example, the following string:
//
//     a b:"c d" 'e''f'  "g\""
//
// Would be parsed as:
//
//     []string{"a", "b:c d", "ef", `g"`}
//
// Copied from go/build.splitQuoted. Also in Gazelle (where tests are).
func splitQuoted(s string) (r []string, err error) {
	var args []string
	arg := make([]rune, len(s))
	escaped := false
	quoted := false
	quote := '\x00'
	i := 0
	for _, rune := range s {
		switch {
		case escaped:
			escaped = false
		case rune == '\\':
			escaped = true
			continue
		case quote != '\x00':
			if rune == quote {
				quote = '\x00'
				continue
			}
		case rune == '"' || rune == '\'':
			quoted = true
			quote = rune
			continue
		case unicode.IsSpace(rune):
			if quoted || i > 0 {
				quoted = false
				args = append(args, string(arg[:i]))
				i = 0
			}
			continue
		}
		arg[i] = rune
		i++
	}
	if quoted || i > 0 {
		args = append(args, string(arg[:i]))
	}
	if quote != 0 {
		err = errors.New("unclosed quote")
	} else if escaped {
		err = errors.New("unfinished escaping")
	}
	return args, err
}

// tagFlag adds tags to the build.Default context. Tags are expected to be
// formatted as a comma-separated list.
type tagFlag struct{}

func (f *tagFlag) String() string {
	return strings.Join(build.Default.BuildTags, ",")
}

func (f *tagFlag) Set(opt string) error {
	tags := strings.Split(opt, ",")
	build.Default.BuildTags = append(build.Default.BuildTags, tags...)
	return nil
}
