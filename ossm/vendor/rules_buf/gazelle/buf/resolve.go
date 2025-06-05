// Copyright 2021-2023 Buf Technologies, Inc.
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
	"log"

	"github.com/bazelbuild/bazel-gazelle/config"
	"github.com/bazelbuild/bazel-gazelle/label"
	"github.com/bazelbuild/bazel-gazelle/repo"
	"github.com/bazelbuild/bazel-gazelle/resolve"
	"github.com/bazelbuild/bazel-gazelle/rule"
)

func (*bufLang) Imports(*config.Config, *rule.Rule, *rule.File) []resolve.ImportSpec { return nil }
func (*bufLang) Embeds(*rule.Rule, label.Label) []label.Label                        { return nil }

func (l *bufLang) Resolve(
	gazelleConfig *config.Config,
	ruleIndex *resolve.RuleIndex,
	remoteCache *repo.RemoteCache,
	ruleToResolve *rule.Rule,
	importsRaw interface{},
	fromLabel label.Label,
) {
	// Only breaking rule requires resolution
	switch ruleToResolve.Kind() {
	case breakingRuleKind:
		config := GetConfigForGazelleConfig(gazelleConfig)
		if config.BreakingMode != BreakingModeModule {
			return
		}
		resolveProtoTargetsForRule(
			gazelleConfig,
			ruleIndex,
			remoteCache,
			ruleToResolve,
			importsRaw,
			fromLabel,
		)
	}
}

// resolveProtoTargetsForRule resolves targets of buf_breaking_test in Module mode
func resolveProtoTargetsForRule(
	gazelleConfig *config.Config,
	ruleIndex *resolve.RuleIndex,
	remoteCache *repo.RemoteCache,
	ruleToResolve *rule.Rule,
	importsRaw interface{},
	fromLabel label.Label,
) {
	// importsRaw will be `[]string` for module mode
	imports, ok := importsRaw.([]string)
	if !ok {
		return
	}
	targetSet := make(map[string]struct{})
	for _, imp := range imports {
		results := ruleIndex.FindRulesByImportWithConfig(
			gazelleConfig,
			resolve.ImportSpec{
				Lang: "proto",
				Imp:  imp,
			},
			"proto",
		)
		if len(results) == 0 {
			log.Printf("unable to resolve proto dependency: %s", imp)
		}
		for _, res := range results {
			targetSet[res.Label.Rel(fromLabel.Repo, fromLabel.Pkg).String()] = struct{}{}
		}
	}
	targets := make([]string, 0, len(targetSet))
	for target := range targetSet {
		targets = append(targets, target)
	}
	ruleToResolve.SetAttr("targets", targets)
}
