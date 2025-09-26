// Copyright 2021-2025 Buf Technologies, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package buf

import (
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strings"

	"github.com/bazelbuild/bazel-gazelle/language"
	"github.com/bazelbuild/bazel-gazelle/rule"
	"github.com/bazelbuild/buildtools/build"
)

var (
	repoImportFuncs = map[string]func(language.ImportReposArgs) language.ImportReposResult{
		"buf.yaml":      bufYAMLImport,
		"buf.lock":      bufLockImport,
		"buf.work.yaml": bufWorkImport,
	}
	workspaceFiles        = []string{"WORKSPACE", "WORKSPACE.bazel"}
	defaultToolchainMacro = "rules_buf_toolchains"

	_ language.RepoImporter = (*bufLang)(nil)
)

func (*bufLang) CanImport(path string) bool {
	return repoImportFuncs[filepath.Base(path)] != nil
}

func (*bufLang) ImportRepos(args language.ImportReposArgs) language.ImportReposResult {
	result := repoImportFuncs[filepath.Base(args.Path)](args)
	if result.Error != nil {
		return result
	}
	if args.Prune {
		genRuleSet := make(map[string]struct{}, len(result.Gen))
		for _, genRule := range result.Gen {
			genRuleSet[genRule.Name()] = struct{}{}
		}
		for _, repoRule := range args.Config.Repos {
			if repoRule.Kind() != dependenciesRepoRuleKind {
				continue
			}
			if _, ok := genRuleSet[repoRule.Name()]; !ok {
				result.Empty = append(result.Empty, repoRule)
			}
		}
	}
	return result
}

func bufLockImport(args language.ImportReposArgs) language.ImportReposResult {
	data, err := os.ReadFile(args.Path)
	if err != nil {
		return language.ImportReposResult{Error: fmt.Errorf("failed to read `buf.lock`: %w", err)}
	}
	var bufLock bufLock
	if err := parseJsonOrYaml(data, &bufLock); err != nil {
		return language.ImportReposResult{Error: fmt.Errorf("failed to parse `buf.lock`: %w", err)}
	}
	if len(bufLock.Deps) == 0 {
		return language.ImportReposResult{}
	}
	relativePath, err := filepath.Rel(args.Config.RepoRoot, args.Path)
	if err != nil {
		return language.ImportReposResult{Error: fmt.Errorf("failed to read `buf.lock`: file is outside the workspace")}
	}
	// The rule name here is based on the relative location of the `buf.yaml`` file.
	// This is not based on the name in `buf.yaml` as that is not a required field.
	ruleName := getRepoNameForPath(filepath.Dir(relativePath))
	repoRule := rule.NewRule(dependenciesRepoRuleKind, ruleName)
	modules := &build.ListExpr{
		ForceMultiLine: true,
		List:           make([]build.Expr, 0, len(bufLock.Deps)),
	}
	for _, dep := range bufLock.Deps {
		var value string
		if dep.Name != "" { // v2
			value = fmt.Sprintf("%s:%s", dep.Name, dep.Commit)
		} else {
			value = fmt.Sprintf("%s/%s/%s:%s", dep.Remote, dep.Owner, dep.Repository, dep.Commit)
		}
		modules.List = append(
			modules.List,
			&build.StringExpr{
				Value: value,
			},
		)
	}
	repoRule.SetAttr("modules", modules)
	addOptionalToolchainAttribute(args, repoRule)
	return language.ImportReposResult{
		Gen: []*rule.Rule{repoRule},
	}
}

func bufYAMLImport(args language.ImportReposArgs) language.ImportReposResult {
	return bufLockImport(
		language.ImportReposArgs{
			Config: args.Config,
			Path:   filepath.Join(filepath.Dir(args.Path), "buf.lock"),
			Prune:  args.Prune,
			Cache:  args.Cache,
		},
	)
}

func bufWorkImport(args language.ImportReposArgs) language.ImportReposResult {
	data, err := os.ReadFile(args.Path)
	if err != nil {
		return language.ImportReposResult{Error: fmt.Errorf("failed to read `buf.work.yaml`: %w", err)}
	}
	var bufWork bufWork
	if err := parseJsonOrYaml(data, &bufWork); err != nil {
		return language.ImportReposResult{Error: fmt.Errorf("failed to parse `buf.work.yaml`: %w", err)}
	}
	var result language.ImportReposResult
	for _, dir := range bufWork.Directories {
		lockPath := filepath.Join(filepath.Dir(args.Path), dir, "buf.lock")
		currentResult := bufLockImport(language.ImportReposArgs{
			Config: args.Config,
			Path:   lockPath,
			Prune:  args.Prune,
			Cache:  args.Cache,
		})
		if currentResult.Error != nil {
			return currentResult
		}
		result.Gen = append(result.Gen, currentResult.Gen...)
		result.Empty = append(result.Empty, currentResult.Empty...)
	}
	return result
}

func getRepoNameForPath(path string) string {
	if path == "." {
		path = ""
	}
	return strings.TrimSuffix("buf_deps_"+strings.ReplaceAll(path, "/", "_"), "_")
}

// The buf cli is made available via the bazel toolchain, this is done using a separate repo
// in buf/internal/toolchain.bzl. The default value for the toolchain can be overriden by users.
//
// The toolchain repo is needed for the repo rule to locate the cli.
//
// This function parses the WORKSPACE file and sets the toolchain repo name if overriden.
func addOptionalToolchainAttribute(args language.ImportReposArgs, rule *rule.Rule) {
	const toolchainRepoAttrKey = "toolchain_repo"
	for _, workspaceFile := range workspaceFiles {
		data, err := os.ReadFile(filepath.Join(args.Config.RepoRoot, workspaceFile))
		if err != nil {
			log.Printf("failed to open workspace file: %q, err: %v", workspaceFile, err)
			continue
		}
		ast, err := build.ParseWorkspace(workspaceFile, data)
		if err != nil {
			log.Printf("failed to parse workspace file: %q, err: %v\n", workspaceFile, err)
			return
		}
		toolchainMacro := defaultToolchainMacro
		for _, expr := range ast.Stmt {
			switch expr := (expr).(type) {
			case *build.LoadStmt:
				if !strings.HasSuffix(expr.Module.Value, "buf:repositories.bzl") {
					continue
				}
				for i, ruleName := range expr.From {
					if ruleName.Name == defaultToolchainMacro {
						toolchainMacro = expr.To[i].Name
						break
					}
				}
			case *build.CallExpr:
				funcIdent, ok := expr.X.(*build.Ident)
				if !ok {
					continue
				}
				if funcIdent.Name != toolchainMacro {
					continue
				}
				// Found the load statement
				if len(expr.List) == 0 {
					return
				}
				if strExpr, ok := expr.List[0].(*build.StringExpr); ok {
					rule.SetAttr(toolchainRepoAttrKey, strExpr.Value)
					return
				}
				for _, arg := range expr.List {
					if assignExpr, ok := arg.(*build.AssignExpr); ok {
						if name, ok := assignExpr.LHS.(*build.Ident); ok && name.Name == "name" {
							strExpr, ok := assignExpr.RHS.(*build.StringExpr)
							if !ok {
								break
							}
							rule.SetAttr(toolchainRepoAttrKey, strExpr.Value)
							return
						}
					}
				}
			}
		}
		return
	}
}
