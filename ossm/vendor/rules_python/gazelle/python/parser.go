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

package python

import (
	"context"
	_ "embed"
	"fmt"
	"strings"

	"github.com/emirpasic/gods/sets/treeset"
	godsutils "github.com/emirpasic/gods/utils"
	"golang.org/x/sync/errgroup"
)

// python3Parser implements a parser for Python files that extracts the modules
// as seen in the import statements.
type python3Parser struct {
	// The value of language.GenerateArgs.Config.RepoRoot.
	repoRoot string
	// The value of language.GenerateArgs.Rel.
	relPackagePath string
	// The function that determines if a dependency is ignored from a Gazelle
	// directive. It's the signature of pythonconfig.Config.IgnoresDependency.
	ignoresDependency func(dep string) bool
}

// newPython3Parser constructs a new python3Parser.
func newPython3Parser(
	repoRoot string,
	relPackagePath string,
	ignoresDependency func(dep string) bool,
) *python3Parser {
	return &python3Parser{
		repoRoot:          repoRoot,
		relPackagePath:    relPackagePath,
		ignoresDependency: ignoresDependency,
	}
}

// parseSingle parses a single Python file and returns the extracted modules
// from the import statements as well as the parsed comments.
func (p *python3Parser) parseSingle(pyFilename string) (*treeset.Set, map[string]*treeset.Set, *annotations, error) {
	pyFilenames := treeset.NewWith(godsutils.StringComparator)
	pyFilenames.Add(pyFilename)
	return p.parse(pyFilenames)
}

// parse parses multiple Python files and returns the extracted modules from
// the import statements as well as the parsed comments.
func (p *python3Parser) parse(pyFilenames *treeset.Set) (*treeset.Set, map[string]*treeset.Set, *annotations, error) {
	modules := treeset.NewWith(moduleComparator)

	g, ctx := errgroup.WithContext(context.Background())
	ch := make(chan struct{}, 6) // Limit the number of concurrent parses.
	chRes := make(chan *ParserOutput, len(pyFilenames.Values()))
	for _, v := range pyFilenames.Values() {
		ch <- struct{}{}
		g.Go(func(filename string) func() error {
			return func() error {
				defer func() {
					<-ch
				}()
				res, err := NewFileParser().ParseFile(ctx, p.repoRoot, p.relPackagePath, filename)
				if err != nil {
					return err
				}
				chRes <- res
				return nil
			}
		}(v.(string)))
	}
	if err := g.Wait(); err != nil {
		return nil, nil, nil, err
	}
	close(ch)
	close(chRes)
	mainModules := make(map[string]*treeset.Set, len(chRes))
	allAnnotations := new(annotations)
	allAnnotations.ignore = make(map[string]struct{})
	for res := range chRes {
		if res.HasMain {
			mainModules[res.FileName] = treeset.NewWith(moduleComparator)
		}
		annotations, err := annotationsFromComments(res.Comments)
		if err != nil {
			return nil, nil, nil, fmt.Errorf("failed to parse annotations: %w", err)
		}

		for _, m := range res.Modules {
			// Check for ignored dependencies set via an annotation to the Python
			// module.
			if annotations.ignores(m.Name) || annotations.ignores(m.From) {
				continue
			}

			// Check for ignored dependencies set via a Gazelle directive in a BUILD
			// file.
			if p.ignoresDependency(m.Name) || p.ignoresDependency(m.From) {
				continue
			}

			modules.Add(m)
			if res.HasMain {
				mainModules[res.FileName].Add(m)
			}
		}

		// Collect all annotations from each file into a single annotations struct.
		for k, v := range annotations.ignore {
			allAnnotations.ignore[k] = v
		}
		allAnnotations.includeDeps = append(allAnnotations.includeDeps, annotations.includeDeps...)
	}

	allAnnotations.includeDeps = removeDupesFromStringTreeSetSlice(allAnnotations.includeDeps)

	return modules, mainModules, allAnnotations, nil
}

// removeDupesFromStringTreeSetSlice takes a []string, makes a set out of the
// elements, and then returns a new []string with all duplicates removed. Order
// is preserved.
func removeDupesFromStringTreeSetSlice(array []string) []string {
	s := treeset.NewWith(godsutils.StringComparator)
	for _, v := range array {
		s.Add(v)
	}
	dedupe := make([]string, s.Size())
	for i, v := range s.Values() {
		dedupe[i] = fmt.Sprint(v)
	}
	return dedupe
}

// module represents a fully-qualified, dot-separated, Python module as seen on
// the import statement, alongside the line number where it happened.
type module struct {
	// The fully-qualified, dot-separated, Python module name as seen on import
	// statements.
	Name string `json:"name"`
	// The line number where the import happened.
	LineNumber uint32 `json:"lineno"`
	// The path to the module file relative to the Bazel workspace root.
	Filepath string `json:"filepath"`
	// If this was a from import, e.g. from foo import bar, From indicates the module
	// from which it is imported.
	From string `json:"from"`
}

// moduleComparator compares modules by name.
func moduleComparator(a, b interface{}) int {
	return godsutils.StringComparator(a.(module).Name, b.(module).Name)
}

// annotationKind represents Gazelle annotation kinds.
type annotationKind string

const (
	// The Gazelle annotation prefix.
	annotationPrefix string = "gazelle:"
	// The ignore annotation kind. E.g. '# gazelle:ignore <module_name>'.
	annotationKindIgnore     annotationKind = "ignore"
	annotationKindIncludeDep annotationKind = "include_dep"
)

// comment represents a Python comment.
type comment string

// asAnnotation returns an annotation object if the comment has the
// annotationPrefix.
func (c *comment) asAnnotation() (*annotation, error) {
	uncomment := strings.TrimLeft(string(*c), "# ")
	if !strings.HasPrefix(uncomment, annotationPrefix) {
		return nil, nil
	}
	withoutPrefix := strings.TrimPrefix(uncomment, annotationPrefix)
	annotationParts := strings.SplitN(withoutPrefix, " ", 2)
	if len(annotationParts) < 2 {
		return nil, fmt.Errorf("`%s` requires a value", *c)
	}
	return &annotation{
		kind:  annotationKind(annotationParts[0]),
		value: annotationParts[1],
	}, nil
}

// annotation represents a single Gazelle annotation parsed from a Python
// comment.
type annotation struct {
	kind  annotationKind
	value string
}

// annotations represent the collection of all Gazelle annotations parsed out of
// the comments of a Python module.
type annotations struct {
	// The parsed modules to be ignored by Gazelle.
	ignore map[string]struct{}
	// Labels that Gazelle should include as deps of the generated target.
	includeDeps []string
}

// annotationsFromComments returns all the annotations parsed out of the
// comments of a Python module.
func annotationsFromComments(comments []comment) (*annotations, error) {
	ignore := make(map[string]struct{})
	includeDeps := []string{}
	for _, comment := range comments {
		annotation, err := comment.asAnnotation()
		if err != nil {
			return nil, err
		}
		if annotation != nil {
			if annotation.kind == annotationKindIgnore {
				modules := strings.Split(annotation.value, ",")
				for _, m := range modules {
					if m == "" {
						continue
					}
					m = strings.TrimSpace(m)
					ignore[m] = struct{}{}
				}
			}
			if annotation.kind == annotationKindIncludeDep {
				targets := strings.Split(annotation.value, ",")
				for _, t := range targets {
					if t == "" {
						continue
					}
					t = strings.TrimSpace(t)
					includeDeps = append(includeDeps, t)
				}
			}
		}
	}
	return &annotations{
		ignore:      ignore,
		includeDeps: includeDeps,
	}, nil
}

// ignored returns true if the given module was ignored via the ignore
// annotation.
func (a *annotations) ignores(module string) bool {
	_, ignores := a.ignore[module]
	return ignores
}
