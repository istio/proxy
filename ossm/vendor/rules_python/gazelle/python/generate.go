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
	"fmt"
	"io/fs"
	"log"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"github.com/bazelbuild/bazel-gazelle/config"
	"github.com/bazelbuild/bazel-gazelle/label"
	"github.com/bazelbuild/bazel-gazelle/language"
	"github.com/bazelbuild/bazel-gazelle/rule"
	"github.com/bmatcuk/doublestar/v4"
	"github.com/emirpasic/gods/lists/singlylinkedlist"
	"github.com/emirpasic/gods/sets/treeset"
	godsutils "github.com/emirpasic/gods/utils"

	"github.com/bazelbuild/rules_python/gazelle/pythonconfig"
)

const (
	pyLibraryEntrypointFilename = "__init__.py"
	pyBinaryEntrypointFilename  = "__main__.py"
	pyTestEntrypointFilename    = "__test__.py"
	pyTestEntrypointTargetname  = "__test__"
	conftestFilename            = "conftest.py"
	conftestTargetname          = "conftest"
)

var (
	buildFilenames = []string{"BUILD", "BUILD.bazel"}
)

func GetActualKindName(kind string, args language.GenerateArgs) string {
	if kindOverride, ok := args.Config.KindMap[kind]; ok {
		return kindOverride.KindName
	}
	return kind
}

func matchesAnyGlob(s string, globs []string) bool {
	// This function assumes that the globs have already been validated. If a glob is
	// invalid, it's considered a non-match and we move on to the next pattern.
	for _, g := range globs {
		if ok, _ := doublestar.Match(g, s); ok {
			return true
		}
	}
	return false
}

// GenerateRules extracts build metadata from source files in a directory.
// GenerateRules is called in each directory where an update is requested
// in depth-first post-order.
func (py *Python) GenerateRules(args language.GenerateArgs) language.GenerateResult {
	cfgs := args.Config.Exts[languageName].(pythonconfig.Configs)
	cfg := cfgs[args.Rel]

	if !cfg.ExtensionEnabled() {
		return language.GenerateResult{}
	}

	if !isBazelPackage(args.Dir) {
		if cfg.CoarseGrainedGeneration() {
			// Determine if the current directory is the root of the coarse-grained
			// generation. If not, return without generating anything.
			parent := cfg.Parent()
			if parent != nil && parent.CoarseGrainedGeneration() {
				return language.GenerateResult{}
			}
		} else if !hasEntrypointFile(args.Dir) {
			return language.GenerateResult{}
		}
	}

	actualPyBinaryKind := GetActualKindName(pyBinaryKind, args)
	actualPyLibraryKind := GetActualKindName(pyLibraryKind, args)
	actualPyTestKind := GetActualKindName(pyTestKind, args)

	pythonProjectRoot := cfg.PythonProjectRoot()

	packageName := filepath.Base(args.Dir)

	pyLibraryFilenames := treeset.NewWith(godsutils.StringComparator)
	pyTestFilenames := treeset.NewWith(godsutils.StringComparator)
	pyFileNames := treeset.NewWith(godsutils.StringComparator)

	// hasPyBinaryEntryPointFile controls whether a single py_binary target should be generated for
	// this package or not.
	hasPyBinaryEntryPointFile := false

	// hasPyTestEntryPointFile and hasPyTestEntryPointTarget control whether a py_test target should
	// be generated for this package or not.
	hasPyTestEntryPointFile := false
	hasPyTestEntryPointTarget := false
	hasConftestFile := false

	testFileGlobs := cfg.TestFilePattern()

	for _, f := range args.RegularFiles {
		if cfg.IgnoresFile(filepath.Base(f)) {
			continue
		}
		ext := filepath.Ext(f)
		if ext == ".py" {
			pyFileNames.Add(f)
			if !hasPyBinaryEntryPointFile && f == pyBinaryEntrypointFilename {
				hasPyBinaryEntryPointFile = true
			} else if !hasPyTestEntryPointFile && f == pyTestEntrypointFilename {
				hasPyTestEntryPointFile = true
			} else if f == conftestFilename {
				hasConftestFile = true
			} else if matchesAnyGlob(f, testFileGlobs) {
				pyTestFilenames.Add(f)
			} else {
				pyLibraryFilenames.Add(f)
			}
		}
	}

	// If a __test__.py file was not found on disk, search for targets that are
	// named __test__.
	if !hasPyTestEntryPointFile && args.File != nil {
		for _, rule := range args.File.Rules {
			if rule.Name() == pyTestEntrypointTargetname {
				hasPyTestEntryPointTarget = true
				break
			}
		}
	}

	// Add files from subdirectories if they meet the criteria.
	for _, d := range args.Subdirs {
		// boundaryPackages represents child Bazel packages that are used as a
		// boundary to stop processing under that tree.
		boundaryPackages := make(map[string]struct{})
		err := filepath.WalkDir(
			filepath.Join(args.Dir, d),
			func(path string, entry fs.DirEntry, err error) error {
				if err != nil {
					return err
				}
				// Ignore the path if it crosses any boundary package. Walking
				// the tree is still important because subsequent paths can
				// represent files that have not crossed any boundaries.
				for bp := range boundaryPackages {
					if strings.HasPrefix(path, bp) {
						return nil
					}
				}
				if entry.IsDir() {
					// If we are visiting a directory, we determine if we should
					// halt digging the tree based on a few criterias:
					//   1. We are using per-file generation.
					//   2. The directory has a BUILD or BUILD.bazel files. Then
					//       it doesn't matter at all what it has since it's a
					//       separate Bazel package.
					//   3. (only for package generation) The directory has an
					//       __init__.py, __main__.py or __test__.py, meaning a
					//       BUILD file will be generated.
					if cfg.PerFileGeneration() {
						return fs.SkipDir
					}

					if isBazelPackage(path) {
						boundaryPackages[path] = struct{}{}
						return nil
					}

					if !cfg.CoarseGrainedGeneration() && hasEntrypointFile(path) {
						return fs.SkipDir
					}

					return nil
				}
				if filepath.Ext(path) == ".py" {
					if cfg.CoarseGrainedGeneration() || !isEntrypointFile(path) {
						srcPath, _ := filepath.Rel(args.Dir, path)
						repoPath := filepath.Join(args.Rel, srcPath)
						excludedPatterns := cfg.ExcludedPatterns()
						if excludedPatterns != nil {
							it := excludedPatterns.Iterator()
							for it.Next() {
								excludedPattern := it.Value().(string)
								isExcluded, err := doublestar.Match(excludedPattern, repoPath)
								if err != nil {
									return err
								}
								if isExcluded {
									return nil
								}
							}
						}
						baseName := filepath.Base(path)
						if matchesAnyGlob(baseName, testFileGlobs) {
							pyTestFilenames.Add(srcPath)
						} else {
							pyLibraryFilenames.Add(srcPath)
						}
					}
				}
				return nil
			},
		)
		if err != nil {
			log.Printf("ERROR: %v\n", err)
			return language.GenerateResult{}
		}
	}

	parser := newPython3Parser(args.Config.RepoRoot, args.Rel, cfg.IgnoresDependency)
	visibility := cfg.Visibility()

	var result language.GenerateResult
	result.Gen = make([]*rule.Rule, 0)

	collisionErrors := singlylinkedlist.New()

	appendPyLibrary := func(srcs *treeset.Set, pyLibraryTargetName string) {
		allDeps, mainModules, annotations, err := parser.parse(srcs)
		if err != nil {
			log.Fatalf("ERROR: %v\n", err)
		}

		if !hasPyBinaryEntryPointFile {
			// Creating one py_binary target per main module when __main__.py doesn't exist.
			mainFileNames := make([]string, 0, len(mainModules))
			for name := range mainModules {
				mainFileNames = append(mainFileNames, name)

				// Remove the file from srcs if we're doing per-file library generation so
				// that we don't also generate a py_library target for it.
				if cfg.PerFileGeneration() {
					srcs.Remove(name)
				}
			}
			sort.Strings(mainFileNames)
			for _, filename := range mainFileNames {
				pyBinaryTargetName := strings.TrimSuffix(filepath.Base(filename), ".py")
				if err := ensureNoCollision(args.File, pyBinaryTargetName, actualPyBinaryKind); err != nil {
					fqTarget := label.New("", args.Rel, pyBinaryTargetName)
					log.Printf("failed to generate target %q of kind %q: %v",
						fqTarget.String(), actualPyBinaryKind, err)
					continue
				}
				pyBinary := newTargetBuilder(pyBinaryKind, pyBinaryTargetName, pythonProjectRoot, args.Rel, pyFileNames).
					addVisibility(visibility).
					addSrc(filename).
					addModuleDependencies(mainModules[filename]).
					addResolvedDependencies(annotations.includeDeps).
					generateImportsAttribute().build()
				result.Gen = append(result.Gen, pyBinary)
				result.Imports = append(result.Imports, pyBinary.PrivateAttr(config.GazelleImportsKey))
			}
		}

		// If we're doing per-file generation, srcs could be empty at this point, meaning we shouldn't make a py_library.
		// If there is already a package named py_library target before, we should generate an empty py_library.
		if srcs.Empty() {
			if args.File == nil {
				return
			}
			generateEmptyLibrary := false
			for _, r := range args.File.Rules {
				if r.Kind() == actualPyLibraryKind && r.Name() == pyLibraryTargetName {
					generateEmptyLibrary = true
				}
			}
			if !generateEmptyLibrary {
				return
			}
		}

		// Check if a target with the same name we are generating already
		// exists, and if it is of a different kind from the one we are
		// generating. If so, we have to throw an error since Gazelle won't
		// generate it correctly.
		if err := ensureNoCollision(args.File, pyLibraryTargetName, actualPyLibraryKind); err != nil {
			fqTarget := label.New("", args.Rel, pyLibraryTargetName)
			err := fmt.Errorf("failed to generate target %q of kind %q: %w. "+
				"Use the '# gazelle:%s' directive to change the naming convention.",
				fqTarget.String(), actualPyLibraryKind, err, pythonconfig.LibraryNamingConvention)
			collisionErrors.Add(err)
		}

		pyLibrary := newTargetBuilder(pyLibraryKind, pyLibraryTargetName, pythonProjectRoot, args.Rel, pyFileNames).
			addVisibility(visibility).
			addSrcs(srcs).
			addModuleDependencies(allDeps).
			addResolvedDependencies(annotations.includeDeps).
			generateImportsAttribute().
			build()

		if pyLibrary.IsEmpty(py.Kinds()[pyLibrary.Kind()]) {
			result.Empty = append(result.Empty, pyLibrary)
		} else {
			result.Gen = append(result.Gen, pyLibrary)
			result.Imports = append(result.Imports, pyLibrary.PrivateAttr(config.GazelleImportsKey))
		}
	}
	if cfg.PerFileGeneration() {
		hasInit, nonEmptyInit := hasLibraryEntrypointFile(args.Dir)
		pyLibraryFilenames.Each(func(index int, filename interface{}) {
			pyLibraryTargetName := strings.TrimSuffix(filepath.Base(filename.(string)), ".py")
			if filename == pyLibraryEntrypointFilename && !nonEmptyInit {
				return // ignore empty __init__.py.
			}
			srcs := treeset.NewWith(godsutils.StringComparator, filename)
			if cfg.PerFileGenerationIncludeInit() && hasInit && nonEmptyInit {
				srcs.Add(pyLibraryEntrypointFilename)
			}
			appendPyLibrary(srcs, pyLibraryTargetName)
		})
	} else {
		appendPyLibrary(pyLibraryFilenames, cfg.RenderLibraryName(packageName))
	}

	if hasPyBinaryEntryPointFile {
		deps, _, annotations, err := parser.parseSingle(pyBinaryEntrypointFilename)
		if err != nil {
			log.Fatalf("ERROR: %v\n", err)
		}

		pyBinaryTargetName := cfg.RenderBinaryName(packageName)

		// Check if a target with the same name we are generating already
		// exists, and if it is of a different kind from the one we are
		// generating. If so, we have to throw an error since Gazelle won't
		// generate it correctly.
		if err := ensureNoCollision(args.File, pyBinaryTargetName, actualPyBinaryKind); err != nil {
			fqTarget := label.New("", args.Rel, pyBinaryTargetName)
			err := fmt.Errorf("failed to generate target %q of kind %q: %w. "+
				"Use the '# gazelle:%s' directive to change the naming convention.",
				fqTarget.String(), actualPyBinaryKind, err, pythonconfig.BinaryNamingConvention)
			collisionErrors.Add(err)
		}

		pyBinaryTarget := newTargetBuilder(pyBinaryKind, pyBinaryTargetName, pythonProjectRoot, args.Rel, pyFileNames).
			setMain(pyBinaryEntrypointFilename).
			addVisibility(visibility).
			addSrc(pyBinaryEntrypointFilename).
			addModuleDependencies(deps).
			addResolvedDependencies(annotations.includeDeps).
			generateImportsAttribute()

		pyBinary := pyBinaryTarget.build()

		result.Gen = append(result.Gen, pyBinary)
		result.Imports = append(result.Imports, pyBinary.PrivateAttr(config.GazelleImportsKey))
	}

	var conftest *rule.Rule
	if hasConftestFile {
		deps, _, annotations, err := parser.parseSingle(conftestFilename)
		if err != nil {
			log.Fatalf("ERROR: %v\n", err)
		}

		// Check if a target with the same name we are generating already
		// exists, and if it is of a different kind from the one we are
		// generating. If so, we have to throw an error since Gazelle won't
		// generate it correctly.
		if err := ensureNoCollision(args.File, conftestTargetname, actualPyLibraryKind); err != nil {
			fqTarget := label.New("", args.Rel, conftestTargetname)
			err := fmt.Errorf("failed to generate target %q of kind %q: %w. ",
				fqTarget.String(), actualPyLibraryKind, err)
			collisionErrors.Add(err)
		}

		conftestTarget := newTargetBuilder(pyLibraryKind, conftestTargetname, pythonProjectRoot, args.Rel, pyFileNames).
			addSrc(conftestFilename).
			addModuleDependencies(deps).
			addResolvedDependencies(annotations.includeDeps).
			addVisibility(visibility).
			setTestonly().
			generateImportsAttribute()

		conftest = conftestTarget.build()

		result.Gen = append(result.Gen, conftest)
		result.Imports = append(result.Imports, conftest.PrivateAttr(config.GazelleImportsKey))
	}

	var pyTestTargets []*targetBuilder
	newPyTestTargetBuilder := func(srcs *treeset.Set, pyTestTargetName string) *targetBuilder {
		deps, _, annotations, err := parser.parse(srcs)
		if err != nil {
			log.Fatalf("ERROR: %v\n", err)
		}
		// Check if a target with the same name we are generating already
		// exists, and if it is of a different kind from the one we are
		// generating. If so, we have to throw an error since Gazelle won't
		// generate it correctly.
		if err := ensureNoCollision(args.File, pyTestTargetName, actualPyTestKind); err != nil {
			fqTarget := label.New("", args.Rel, pyTestTargetName)
			err := fmt.Errorf("failed to generate target %q of kind %q: %w. "+
				"Use the '# gazelle:%s' directive to change the naming convention.",
				fqTarget.String(), actualPyTestKind, err, pythonconfig.TestNamingConvention)
			collisionErrors.Add(err)
		}
		return newTargetBuilder(pyTestKind, pyTestTargetName, pythonProjectRoot, args.Rel, pyFileNames).
			addSrcs(srcs).
			addModuleDependencies(deps).
			addResolvedDependencies(annotations.includeDeps).
			generateImportsAttribute()
	}
	if (!cfg.PerPackageGenerationRequireTestEntryPoint() || hasPyTestEntryPointFile || hasPyTestEntryPointTarget || cfg.CoarseGrainedGeneration()) && !cfg.PerFileGeneration() {
		// Create one py_test target per package
		if hasPyTestEntryPointFile {
			// Only add the pyTestEntrypointFilename to the pyTestFilenames if
			// the file exists on disk.
			pyTestFilenames.Add(pyTestEntrypointFilename)
		}
		if hasPyTestEntryPointTarget || !pyTestFilenames.Empty() {
			pyTestTargetName := cfg.RenderTestName(packageName)
			pyTestTarget := newPyTestTargetBuilder(pyTestFilenames, pyTestTargetName)

			if hasPyTestEntryPointTarget {
				entrypointTarget := fmt.Sprintf(":%s", pyTestEntrypointTargetname)
				main := fmt.Sprintf(":%s", pyTestEntrypointFilename)
				pyTestTarget.
					addSrc(entrypointTarget).
					addResolvedDependency(entrypointTarget).
					setMain(main)
			} else if hasPyTestEntryPointFile {
				pyTestTarget.setMain(pyTestEntrypointFilename)
			} /* else:
			main is not set, assuming there is a test file with the same name
			as the target name, or there is a macro wrapping py_test and setting its main attribute.
			*/
			pyTestTargets = append(pyTestTargets, pyTestTarget)
		}
	} else {
		// Create one py_test target per file
		pyTestFilenames.Each(func(index int, testFile interface{}) {
			srcs := treeset.NewWith(godsutils.StringComparator, testFile)
			pyTestTargetName := strings.TrimSuffix(filepath.Base(testFile.(string)), ".py")
			pyTestTarget := newPyTestTargetBuilder(srcs, pyTestTargetName)

			if hasPyTestEntryPointTarget {
				entrypointTarget := fmt.Sprintf(":%s", pyTestEntrypointTargetname)
				main := fmt.Sprintf(":%s", pyTestEntrypointFilename)
				pyTestTarget.
					addSrc(entrypointTarget).
					addResolvedDependency(entrypointTarget).
					setMain(main)
			} else if hasPyTestEntryPointFile {
				pyTestTarget.addSrc(pyTestEntrypointFilename)
				pyTestTarget.setMain(pyTestEntrypointFilename)
			}
			pyTestTargets = append(pyTestTargets, pyTestTarget)
		})
	}

	for _, pyTestTarget := range pyTestTargets {
		if conftest != nil {
			pyTestTarget.addModuleDependency(module{Name: strings.TrimSuffix(conftestFilename, ".py")})
		}
		pyTest := pyTestTarget.build()

		result.Gen = append(result.Gen, pyTest)
		result.Imports = append(result.Imports, pyTest.PrivateAttr(config.GazelleImportsKey))
	}

	if !collisionErrors.Empty() {
		it := collisionErrors.Iterator()
		for it.Next() {
			log.Printf("ERROR: %v\n", it.Value())
		}
		os.Exit(1)
	}

	return result
}

// isBazelPackage determines if the directory is a Bazel package by probing for
// the existence of a known BUILD file name.
func isBazelPackage(dir string) bool {
	for _, buildFilename := range buildFilenames {
		path := filepath.Join(dir, buildFilename)
		if _, err := os.Stat(path); err == nil {
			return true
		}
	}
	return false
}

// hasEntrypointFile determines if the directory has any of the established
// entrypoint filenames.
func hasEntrypointFile(dir string) bool {
	for _, entrypointFilename := range []string{
		pyLibraryEntrypointFilename,
		pyBinaryEntrypointFilename,
		pyTestEntrypointFilename,
	} {
		path := filepath.Join(dir, entrypointFilename)
		if _, err := os.Stat(path); err == nil {
			return true
		}
	}
	return false
}

// hasLibraryEntrypointFile returns if the given directory has the library
// entrypoint file, and if it is non-empty.
func hasLibraryEntrypointFile(dir string) (bool, bool) {
	stat, err := os.Stat(filepath.Join(dir, pyLibraryEntrypointFilename))
	if os.IsNotExist(err) {
		return false, false
	}
	if err != nil {
		log.Fatalf("ERROR: %v\n", err)
	}
	return true, stat.Size() != 0
}

// isEntrypointFile returns whether the given path is an entrypoint file. The
// given path can be absolute or relative.
func isEntrypointFile(path string) bool {
	basePath := filepath.Base(path)
	switch basePath {
	case pyLibraryEntrypointFilename,
		pyBinaryEntrypointFilename,
		pyTestEntrypointFilename:
		return true
	default:
		return false
	}
}

func ensureNoCollision(file *rule.File, targetName, kind string) error {
	if file == nil {
		return nil
	}
	for _, t := range file.Rules {
		if t.Name() == targetName && t.Kind() != kind {
			return fmt.Errorf("a target of kind %q with the same name already exists", t.Kind())
		}
	}
	return nil
}
