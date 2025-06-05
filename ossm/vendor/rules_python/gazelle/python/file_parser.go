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
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strings"

	sitter "github.com/dougthor42/go-tree-sitter"
	"github.com/dougthor42/go-tree-sitter/python"
)

const (
	sitterNodeTypeString              = "string"
	sitterNodeTypeComment             = "comment"
	sitterNodeTypeIdentifier          = "identifier"
	sitterNodeTypeDottedName          = "dotted_name"
	sitterNodeTypeIfStatement         = "if_statement"
	sitterNodeTypeAliasedImport       = "aliased_import"
	sitterNodeTypeWildcardImport      = "wildcard_import"
	sitterNodeTypeImportStatement     = "import_statement"
	sitterNodeTypeComparisonOperator  = "comparison_operator"
	sitterNodeTypeImportFromStatement = "import_from_statement"
)

type ParserOutput struct {
	FileName string
	Modules  []module
	Comments []comment
	HasMain  bool
}

type FileParser struct {
	code        []byte
	relFilepath string
	output      ParserOutput
}

func NewFileParser() *FileParser {
	return &FileParser{}
}

// ParseCode instantiates a new tree-sitter Parser and parses the python code, returning
// the tree-sitter RootNode.
// It prints a warning if parsing fails.
func ParseCode(code []byte, path string) (*sitter.Node, error) {
	parser := sitter.NewParser()
	parser.SetLanguage(python.GetLanguage())

	tree, err := parser.ParseCtx(context.Background(), nil, code)
	if err != nil {
		return nil, err
	}

	root := tree.RootNode()
	if !root.HasError() {
		return root, nil
	}

	log.Printf("WARNING: failed to parse %q. The resulting BUILD target may be incorrect.", path)

	// Note: we intentionally do not return an error even when root.HasError because the parse
	// failure may be in some part of the code that Gazelle doesn't care about.
	verbose, envExists := os.LookupEnv("RULES_PYTHON_GAZELLE_VERBOSE")
	if !envExists || verbose != "1" {
		return root, nil
	}

	for i := 0; i < int(root.ChildCount()); i++ {
		child := root.Child(i)
		if child.IsError() {
			// Example logs:
			// gazelle: Parse error at {Row:1 Column:0}:
			// def search_one_more_level[T]():
			log.Printf("Parse error at %+v:\n%+v", child.StartPoint(), child.Content(code))
			// Log the internal tree-sitter representation of what was parsed. Eg:
			// gazelle: The above was parsed as: (ERROR (identifier) (call function: (list (identifier)) arguments: (argument_list)))
			log.Printf("The above was parsed as: %v", child.String())
		}
	}

	return root, nil
}

// parseMain returns true if the python file has an `if __name__ == "__main__":` block,
// which is a common idiom for python scripts/binaries.
func (p *FileParser) parseMain(ctx context.Context, node *sitter.Node) bool {
	for i := 0; i < int(node.ChildCount()); i++ {
		if err := ctx.Err(); err != nil {
			return false
		}
		child := node.Child(i)
		if child.Type() == sitterNodeTypeIfStatement &&
			child.Child(1).Type() == sitterNodeTypeComparisonOperator && child.Child(1).Child(1).Type() == "==" {
			statement := child.Child(1)
			a, b := statement.Child(0), statement.Child(2)
			// convert "'__main__' == __name__" to "__name__ == '__main__'"
			if b.Type() == sitterNodeTypeIdentifier {
				a, b = b, a
			}
			if a.Type() == sitterNodeTypeIdentifier && a.Content(p.code) == "__name__" &&
				// at github.com/dougthor42/go-tree-sitter@latest (after v0.0.0-20240422154435-0628b34cbf9c we used)
				// "__main__" is the second child of b. But now, it isn't.
				// we cannot use the latest go-tree-sitter because of the top level reference in scanner.c.
				// https://github.com/dougthor42/go-tree-sitter/blob/04d6b33fe138a98075210f5b770482ded024dc0f/python/scanner.c#L1
				b.Type() == sitterNodeTypeString && string(p.code[b.StartByte()+1:b.EndByte()-1]) == "__main__" {
				return true
			}
		}
	}
	return false
}

// parseImportStatement parses a node for an import statement, returning a `module` and a boolean
// representing if the parse was OK or not.
func parseImportStatement(node *sitter.Node, code []byte) (module, bool) {
	switch node.Type() {
	case sitterNodeTypeDottedName:
		return module{
			Name:       node.Content(code),
			LineNumber: node.StartPoint().Row + 1,
		}, true
	case sitterNodeTypeAliasedImport:
		return parseImportStatement(node.Child(0), code)
	case sitterNodeTypeWildcardImport:
		return module{
			Name:       "*",
			LineNumber: node.StartPoint().Row + 1,
		}, true
	}
	return module{}, false
}

// parseImportStatements parses a node for import statements, returning true if the node is
// an import statement. It updates FileParser.output.Modules with the `module` that the
// import represents.
func (p *FileParser) parseImportStatements(node *sitter.Node) bool {
	if node.Type() == sitterNodeTypeImportStatement {
		for j := 1; j < int(node.ChildCount()); j++ {
			m, ok := parseImportStatement(node.Child(j), p.code)
			if !ok {
				continue
			}
			m.Filepath = p.relFilepath
			if strings.HasPrefix(m.Name, ".") {
				continue
			}
			p.output.Modules = append(p.output.Modules, m)
		}
	} else if node.Type() == sitterNodeTypeImportFromStatement {
		from := node.Child(1).Content(p.code)
		if strings.HasPrefix(from, ".") {
			return true
		}
		for j := 3; j < int(node.ChildCount()); j++ {
			m, ok := parseImportStatement(node.Child(j), p.code)
			if !ok {
				continue
			}
			m.Filepath = p.relFilepath
			m.From = from
			m.Name = fmt.Sprintf("%s.%s", from, m.Name)
			p.output.Modules = append(p.output.Modules, m)
		}
	} else {
		return false
	}
	return true
}

// parseComments parses a node for comments, returning true if the node is a comment.
// It updates FileParser.output.Comments with the parsed comment.
func (p *FileParser) parseComments(node *sitter.Node) bool {
	if node.Type() == sitterNodeTypeComment {
		p.output.Comments = append(p.output.Comments, comment(node.Content(p.code)))
		return true
	}
	return false
}

func (p *FileParser) SetCodeAndFile(code []byte, relPackagePath, filename string) {
	p.code = code
	p.relFilepath = filepath.Join(relPackagePath, filename)
	p.output.FileName = filename
}

func (p *FileParser) parse(ctx context.Context, node *sitter.Node) {
	if node == nil {
		return
	}
	for i := 0; i < int(node.ChildCount()); i++ {
		if err := ctx.Err(); err != nil {
			return
		}
		child := node.Child(i)
		if p.parseImportStatements(child) {
			continue
		}
		if p.parseComments(child) {
			continue
		}
		p.parse(ctx, child)
	}
}

func (p *FileParser) Parse(ctx context.Context) (*ParserOutput, error) {
	rootNode, err := ParseCode(p.code, p.relFilepath)
	if err != nil {
		return nil, err
	}

	p.output.HasMain = p.parseMain(ctx, rootNode)

	p.parse(ctx, rootNode)
	return &p.output, nil
}

func (p *FileParser) ParseFile(ctx context.Context, repoRoot, relPackagePath, filename string) (*ParserOutput, error) {
	code, err := os.ReadFile(filepath.Join(repoRoot, relPackagePath, filename))
	if err != nil {
		return nil, err
	}
	p.SetCodeAndFile(code, relPackagePath, filename)
	return p.Parse(ctx)
}
