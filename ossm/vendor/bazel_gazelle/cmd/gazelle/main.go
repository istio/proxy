/* Copyright 2016 The Bazel Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

// Command gazelle is a BUILD file generator for Go projects.
// See "gazelle --help" for more details.
package main

import (
	"flag"
	"fmt"
	"log"
	"os"

	"github.com/bazelbuild/bazel-gazelle/config"
	"github.com/bazelbuild/bazel-gazelle/language"
)

type command int

const (
	updateCmd command = iota
	fixCmd
	updateReposCmd
	helpCmd
)

var commandFromName = map[string]command{
	"fix":          fixCmd,
	"help":         helpCmd,
	"update":       updateCmd,
	"update-repos": updateReposCmd,
}

var nameFromCommand = []string{
	// keep in sync with definition above
	"update",
	"fix",
	"update-repos",
	"help",
}

func (cmd command) String() string {
	return nameFromCommand[cmd]
}

func main() {
	log.SetPrefix("gazelle: ")
	log.SetFlags(0) // don't print timestamps

	var wd string
	if wsDir := os.Getenv("BUILD_WORKSPACE_DIRECTORY"); wsDir != "" {
		wd = wsDir
	} else {
		var err error
		if wd, err = os.Getwd(); err != nil {
			log.Fatal(err)
		}
	}

	if err := run(wd, os.Args[1:]); err != nil && err != flag.ErrHelp {
		if err == errExit {
			os.Exit(1)
		} else {
			log.Fatal(err)
		}
	}
}

func run(wd string, args []string) error {
	cmd := updateCmd
	if len(args) == 1 && (args[0] == "-h" || args[0] == "-help" || args[0] == "--help") {
		cmd = helpCmd
	} else if len(args) > 0 {
		c, ok := commandFromName[args[0]]
		if ok {
			cmd = c
			args = args[1:]
		}
	}

	switch cmd {
	case fixCmd, updateCmd:
		return runFixUpdate(wd, cmd, args)
	case helpCmd:
		return help()
	case updateReposCmd:
		return updateRepos(wd, args)
	default:
		log.Panicf("unknown command: %v", cmd)
	}
	return nil
}

func help() error {
	fmt.Fprint(os.Stderr, `usage: gazelle <command> [args...]

Gazelle is a BUILD file generator for Go projects. It can create new BUILD files
for a project that follows "go build" conventions, and it can update BUILD files
if they already exist. It can be invoked directly in a project workspace, or
it can be run on an external dependency during the build as part of the
go_repository rule.

Gazelle may be run with one of the commands below. If no command is given,
Gazelle defaults to "update".

  update - Gazelle will create new BUILD files or update existing BUILD files
      if needed.
  fix - in addition to the changes made in update, Gazelle will make potentially
      breaking changes. For example, it may delete obsolete rules or rename
      existing rules.
  update-repos - updates repository rules in the WORKSPACE file. Run with
      -h for details.
  help - show this message.

For usage information for a specific command, run the command with the -h flag.
For example:

  gazelle update -h

Gazelle is under active development, and its interface may change
without notice.

`)
	return flag.ErrHelp
}

// filterLanguages returns the subset of input languages that pass the config's
// filter, if any. Gazelle should not generate rules for languages not returned.
func filterLanguages(c *config.Config, langs []language.Language) []language.Language {
	if len(c.Langs) == 0 {
		return langs
	}

	var result []language.Language
	for _, inputLang := range langs {
		if containsLang(c.Langs, inputLang) {
			result = append(result, inputLang)
		}
	}
	return result
}

func containsLang(langNames []string, lang language.Language) bool {
	for _, langName := range langNames {
		if langName == lang.Name() {
			return true
		}
	}
	return false
}
