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
	"log"
	"os"
	"path"
	"regexp"
	"strings"

	"github.com/emirpasic/gods/lists/singlylinkedlist"

	"github.com/bazel-contrib/rules_python/gazelle/manifest"
	"github.com/bazelbuild/bazel-gazelle/label"
)

// Directives
const (
	// PythonExtensionDirective represents the directive that controls whether
	// this Python extension is enabled or not. Sub-packages inherit this value.
	// Can be either "enabled" or "disabled". Defaults to "enabled".
	PythonExtensionDirective = "python_extension"
	// PythonRootDirective represents the directive that sets a Bazel package as
	// a Python root. This is used on monorepos with multiple Python projects
	// that don't share the top-level of the workspace as the root.
	PythonRootDirective = "python_root"
	// PythonManifestFileNameDirective represents the directive that overrides
	// the default gazelle_python.yaml manifest file name.
	PythonManifestFileNameDirective = "python_manifest_file_name"
	// IgnoreFilesDirective represents the directive that controls the ignored
	// files from the generated targets.
	IgnoreFilesDirective = "python_ignore_files"
	// IgnoreDependenciesDirective represents the directive that controls the
	// ignored dependencies from the generated targets.
	IgnoreDependenciesDirective = "python_ignore_dependencies"
	// ValidateImportStatementsDirective represents the directive that controls
	// whether the Python import statements should be validated.
	ValidateImportStatementsDirective = "python_validate_import_statements"
	// GenerationMode represents the directive that controls the target generation
	// mode. See below for the GenerationModeType constants.
	GenerationMode = "python_generation_mode"
	// GenerationModePerFileIncludeInit represents the directive that augments
	// the "per_file" GenerationMode by including the package's __init__.py file.
	// This is a boolean directive.
	GenerationModePerFileIncludeInit = "python_generation_mode_per_file_include_init"
	// GenerationModePerPackageRequireTestEntryPoint represents the directive that
	// requires a test entry point to generate test targets in "package" GenerationMode.
	// This is a boolean directive.
	GenerationModePerPackageRequireTestEntryPoint = "python_generation_mode_per_package_require_test_entry_point"
	// LibraryNamingConvention represents the directive that controls the
	// py_library naming convention. It interpolates $package_name$ with the
	// Bazel package name. E.g. if the Bazel package name is `foo`, setting this
	// to `$package_name$_my_lib` would render to `foo_my_lib`.
	LibraryNamingConvention = "python_library_naming_convention"
	// BinaryNamingConvention represents the directive that controls the
	// py_binary naming convention. See python_library_naming_convention for
	// more info on the package name interpolation.
	BinaryNamingConvention = "python_binary_naming_convention"
	// TestNamingConvention represents the directive that controls the py_test
	// naming convention. See python_library_naming_convention for more info on
	// the package name interpolation.
	TestNamingConvention = "python_test_naming_convention"
	// ProtoNamingConvention represents the directive that controls the
	// py_proto_library naming convention. It interpolates $proto_name$ with
	// the proto_library rule name, minus any trailing _proto. E.g. if the
	// proto_library name is `foo_proto`, setting this to `$proto_name$_my_lib`
	// would render to `foo_my_lib`.
	ProtoNamingConvention = "python_proto_naming_convention"
	// DefaultVisibilty represents the directive that controls what visibility
	// labels are added to generated python targets.
	DefaultVisibilty = "python_default_visibility"
	// Visibility represents the directive that controls what additional
	// visibility labels are added to generated targets. It mimics the behavior
	// of the `go_visibility` directive.
	Visibility = "python_visibility"
	// TestFilePattern represents the directive that controls which python
	// files are mapped to `py_test` targets.
	TestFilePattern = "python_test_file_pattern"
	// LabelConvention represents the directive that defines the format of the
	// labels to third-party dependencies.
	LabelConvention = "python_label_convention"
	// LabelNormalization represents the directive that controls how distribution
	// names of labels to third-party dependencies are normalized. Supported values
	// are 'none', 'pep503' and 'snake_case' (default). See LabelNormalizationType.
	LabelNormalization = "python_label_normalization"
	// ExperimentalAllowRelativeImports represents the directive that controls
	// whether relative imports are allowed.
	ExperimentalAllowRelativeImports = "python_experimental_allow_relative_imports"
	// GeneratePyiDeps represents the directive that controls whether to generate
	// separate pyi_deps attribute or merge type-checking dependencies into deps.
	// Defaults to false for backward compatibility.
	GeneratePyiDeps = "python_generate_pyi_deps"
	// GenerateProto represents the directive that controls whether to generate
	// python_generate_proto targets.
	GenerateProto = "python_generate_proto"
	// PythonResolveSiblingImports represents the directive that controls whether
	// absolute imports can be solved to sibling modules. When enabled, imports
	// like "import a" can be resolved to sibling modules. When disabled, they
	// can only be resolved as an absolute import.
	PythonResolveSiblingImports = "python_resolve_sibling_imports"
)

// GenerationModeType represents one of the generation modes for the Python
// extension.
type GenerationModeType string

// Generation modes
const (
	// GenerationModePackage defines the mode in which targets will be generated
	// for each __init__.py, or when an existing BUILD or BUILD.bazel file already
	// determines a Bazel package.
	GenerationModePackage GenerationModeType = "package"
	// GenerationModeProject defines the mode in which a coarse-grained target will
	// be generated englobing sub-directories containing Python files.
	GenerationModeProject GenerationModeType = "project"
	GenerationModeFile    GenerationModeType = "file"
)

const (
	packageNameNamingConventionSubstitution     = "$package_name$"
	protoNameNamingConventionSubstitution       = "$proto_name$"
	distributionNameLabelConventionSubstitution = "$distribution_name$"
)

const (
	// The default visibility label, including a format placeholder for `python_root`.
	DefaultVisibilityFmtString = "//%s:__subpackages__"
	// The default globs used to determine pt_test targets.
	DefaultTestFilePatternString = "*_test.py,test_*.py"
	// The default convention of label of third-party dependencies.
	DefaultLabelConvention = "$distribution_name$"
	// The default normalization applied to distribution names of third-party dependency labels.
	DefaultLabelNormalizationType = SnakeCaseLabelNormalizationType
)

// defaultIgnoreFiles is the list of default values used in the
// python_ignore_files option.
var defaultIgnoreFiles = map[string]struct{}{}

// Configs is an extension of map[string]*Config. It provides finding methods
// on top of the mapping.
type Configs map[string]*Config

// ParentForPackage returns the parent Config for the given Bazel package.
func (c Configs) ParentForPackage(pkg string) *Config {
	for {
		dir := path.Dir(pkg)
		if dir == "." {
			dir = ""
		}
		parent := (map[string]*Config)(c)[dir]
		if parent != nil {
			return parent
		}
		if dir == "" {
			return nil
		}
		pkg = dir
	}
}

// Config represents a config extension for a specific Bazel package.
type Config struct {
	parent *Config

	extensionEnabled    bool
	repoRoot            string
	pythonProjectRoot   string
	gazelleManifestPath string
	gazelleManifest     *manifest.Manifest

	excludedPatterns                          *singlylinkedlist.List
	ignoreFiles                               map[string]struct{}
	ignoreDependencies                        map[string]struct{}
	validateImportStatements                  bool
	coarseGrainedGeneration                   bool
	perFileGeneration                         bool
	perFileGenerationIncludeInit              bool
	perPackageGenerationRequireTestEntryPoint bool
	libraryNamingConvention                   string
	binaryNamingConvention                    string
	testNamingConvention                      string
	protoNamingConvention                     string
	defaultVisibility                         []string
	visibility                                []string
	testFilePattern                           []string
	labelConvention                           string
	labelNormalization                        LabelNormalizationType
	experimentalAllowRelativeImports          bool
	generatePyiDeps                           bool
	generateProto                             bool
	resolveSiblingImports                     bool
}

type LabelNormalizationType int

const (
	NoLabelNormalizationType LabelNormalizationType = iota
	Pep503LabelNormalizationType
	SnakeCaseLabelNormalizationType
)

// New creates a new Config.
func New(
	repoRoot string,
	pythonProjectRoot string,
) *Config {
	return &Config{
		extensionEnabled:                          true,
		repoRoot:                                  repoRoot,
		pythonProjectRoot:                         pythonProjectRoot,
		excludedPatterns:                          singlylinkedlist.New(),
		ignoreFiles:                               make(map[string]struct{}),
		ignoreDependencies:                        make(map[string]struct{}),
		validateImportStatements:                  true,
		coarseGrainedGeneration:                   false,
		perFileGeneration:                         false,
		perFileGenerationIncludeInit:              false,
		perPackageGenerationRequireTestEntryPoint: true,
		libraryNamingConvention:                   packageNameNamingConventionSubstitution,
		binaryNamingConvention:                    fmt.Sprintf("%s_bin", packageNameNamingConventionSubstitution),
		testNamingConvention:                      fmt.Sprintf("%s_test", packageNameNamingConventionSubstitution),
		protoNamingConvention:                     fmt.Sprintf("%s_py_pb2", protoNameNamingConventionSubstitution),
		defaultVisibility:                         []string{fmt.Sprintf(DefaultVisibilityFmtString, "")},
		visibility:                                []string{},
		testFilePattern:                           strings.Split(DefaultTestFilePatternString, ","),
		labelConvention:                           DefaultLabelConvention,
		labelNormalization:                        DefaultLabelNormalizationType,
		experimentalAllowRelativeImports:          false,
		generatePyiDeps:                           false,
		generateProto:                             false,
		resolveSiblingImports:                     false,
	}
}

// Parent returns the parent config.
func (c *Config) Parent() *Config {
	return c.parent
}

// NewChild creates a new child Config. It inherits desired values from the
// current Config and sets itself as the parent to the child.
func (c *Config) NewChild() *Config {
	return &Config{
		parent:                       c,
		extensionEnabled:             c.extensionEnabled,
		repoRoot:                     c.repoRoot,
		pythonProjectRoot:            c.pythonProjectRoot,
		excludedPatterns:             c.excludedPatterns,
		ignoreFiles:                  make(map[string]struct{}),
		ignoreDependencies:           make(map[string]struct{}),
		validateImportStatements:     c.validateImportStatements,
		coarseGrainedGeneration:      c.coarseGrainedGeneration,
		perFileGeneration:            c.perFileGeneration,
		perFileGenerationIncludeInit: c.perFileGenerationIncludeInit,
		perPackageGenerationRequireTestEntryPoint: c.perPackageGenerationRequireTestEntryPoint,
		libraryNamingConvention:                   c.libraryNamingConvention,
		binaryNamingConvention:                    c.binaryNamingConvention,
		testNamingConvention:                      c.testNamingConvention,
		protoNamingConvention:                     c.protoNamingConvention,
		defaultVisibility:                         c.defaultVisibility,
		visibility:                                c.visibility,
		testFilePattern:                           c.testFilePattern,
		labelConvention:                           c.labelConvention,
		labelNormalization:                        c.labelNormalization,
		experimentalAllowRelativeImports:          c.experimentalAllowRelativeImports,
		generatePyiDeps:                           c.generatePyiDeps,
		generateProto:                             c.generateProto,
		resolveSiblingImports:                     c.resolveSiblingImports,
	}
}

// AddExcludedPattern adds a glob pattern parsed from the standard
// gazelle:exclude directive.
func (c *Config) AddExcludedPattern(pattern string) {
	c.excludedPatterns.Add(pattern)
}

// ExcludedPatterns returns the excluded patterns list.
func (c *Config) ExcludedPatterns() *singlylinkedlist.List {
	return c.excludedPatterns
}

// SetExtensionEnabled sets whether the extension is enabled or not.
func (c *Config) SetExtensionEnabled(enabled bool) {
	c.extensionEnabled = enabled
}

// ExtensionEnabled returns whether the extension is enabled or not.
func (c *Config) ExtensionEnabled() bool {
	return c.extensionEnabled
}

// SetPythonProjectRoot sets the Python project root.
func (c *Config) SetPythonProjectRoot(pythonProjectRoot string) {
	c.pythonProjectRoot = pythonProjectRoot
}

// PythonProjectRoot returns the Python project root.
func (c *Config) PythonProjectRoot() string {
	return c.pythonProjectRoot
}

// SetGazelleManifest sets the Gazelle manifest parsed from the
// gazelle_python.yaml file.
func (c *Config) SetGazelleManifest(gazelleManifest *manifest.Manifest) {
	c.gazelleManifest = gazelleManifest
}

// SetGazelleManifestPath sets the path to the gazelle_python.yaml file
// for the current configuration.
func (c *Config) SetGazelleManifestPath(gazelleManifestPath string) {
	c.gazelleManifestPath = gazelleManifestPath
}

// FindThirdPartyDependency scans the gazelle manifests for the current config
// and the parent configs up to the root finding if it can resolve the module
// name.
func (c *Config) FindThirdPartyDependency(modName string) (string, string, bool) {
	for currentCfg := c; currentCfg != nil; currentCfg = currentCfg.parent {
		// Attempt to load the manifest if needed.
		if currentCfg.gazelleManifestPath != "" && currentCfg.gazelleManifest == nil {
			currentCfgManifest, err := loadGazelleManifest(currentCfg.gazelleManifestPath)
			if err != nil {
				log.Fatal(err)
			}
			currentCfg.SetGazelleManifest(currentCfgManifest)
		}

		if currentCfg.gazelleManifest != nil {
			gazelleManifest := currentCfg.gazelleManifest
			if distributionName, ok := gazelleManifest.ModulesMapping[modName]; ok {
				var distributionRepositoryName string
				if gazelleManifest.PipDepsRepositoryName != "" {
					distributionRepositoryName = gazelleManifest.PipDepsRepositoryName
				} else if gazelleManifest.PipRepository != nil {
					distributionRepositoryName = gazelleManifest.PipRepository.Name
				}

				lbl := currentCfg.FormatThirdPartyDependency(distributionRepositoryName, distributionName)
				return lbl.String(), distributionName, true
			}
		}
	}
	return "", "", false
}

// AddIgnoreFile adds a file to the list of ignored files for a given package.
// Adding an ignored file to a package also makes it ignored on a subpackage.
func (c *Config) AddIgnoreFile(file string) {
	c.ignoreFiles[strings.TrimSpace(file)] = struct{}{}
}

// IgnoresFile checks if a file is ignored in the given package or in one of the
// parent packages up to the workspace root.
func (c *Config) IgnoresFile(file string) bool {
	trimmedFile := strings.TrimSpace(file)

	if _, ignores := defaultIgnoreFiles[trimmedFile]; ignores {
		return true
	}

	if _, ignores := c.ignoreFiles[trimmedFile]; ignores {
		return true
	}

	parent := c.parent
	for parent != nil {
		if _, ignores := parent.ignoreFiles[trimmedFile]; ignores {
			return true
		}
		parent = parent.parent
	}

	return false
}

// AddIgnoreDependency adds a dependency to the list of ignored dependencies for
// a given package. Adding an ignored dependency to a package also makes it
// ignored on a subpackage.
func (c *Config) AddIgnoreDependency(dep string) {
	c.ignoreDependencies[strings.TrimSpace(dep)] = struct{}{}
}

// IgnoresDependency checks if a dependency is ignored in the given package or
// in one of the parent packages up to the workspace root.
func (c *Config) IgnoresDependency(dep string) bool {
	trimmedDep := strings.TrimSpace(dep)

	if _, ignores := c.ignoreDependencies[trimmedDep]; ignores {
		return true
	}

	parent := c.parent
	for parent != nil {
		if _, ignores := parent.ignoreDependencies[trimmedDep]; ignores {
			return true
		}
		parent = parent.parent
	}

	return false
}

// SetValidateImportStatements sets whether Python import statements should be
// validated or not. It throws an error if this is set multiple times, i.e. if
// the directive is specified multiple times in the Bazel workspace.
func (c *Config) SetValidateImportStatements(validate bool) {
	c.validateImportStatements = validate
}

// ValidateImportStatements returns whether the Python import statements should
// be validated or not. If this option was not explicitly specified by the user,
// it defaults to true.
func (c *Config) ValidateImportStatements() bool {
	return c.validateImportStatements
}

// SetCoarseGrainedGeneration sets whether coarse-grained targets should be
// generated or not.
func (c *Config) SetCoarseGrainedGeneration(coarseGrained bool) {
	c.coarseGrainedGeneration = coarseGrained
}

// CoarseGrainedGeneration returns whether coarse-grained targets should be
// generated or not.
func (c *Config) CoarseGrainedGeneration() bool {
	return c.coarseGrainedGeneration
}

// SetPerFileGneration sets whether a separate py_library target should be
// generated for each file.
func (c *Config) SetPerFileGeneration(perFile bool) {
	c.perFileGeneration = perFile
}

// PerFileGeneration returns whether a separate py_library target should be
// generated for each file.
func (c *Config) PerFileGeneration() bool {
	return c.perFileGeneration
}

// SetPerFileGenerationIncludeInit sets whether py_library targets should
// include __init__.py files when PerFileGeneration() is true.
func (c *Config) SetPerFileGenerationIncludeInit(includeInit bool) {
	c.perFileGenerationIncludeInit = includeInit
}

// PerFileGenerationIncludeInit returns whether py_library targets should
// include __init__.py files when PerFileGeneration() is true.
func (c *Config) PerFileGenerationIncludeInit() bool {
	return c.perFileGenerationIncludeInit
}

func (c *Config) SetPerPackageGenerationRequireTestEntryPoint(perPackageGenerationRequireTestEntryPoint bool) {
	c.perPackageGenerationRequireTestEntryPoint = perPackageGenerationRequireTestEntryPoint
}

func (c *Config) PerPackageGenerationRequireTestEntryPoint() bool {
	return c.perPackageGenerationRequireTestEntryPoint
}

// SetLibraryNamingConvention sets the py_library target naming convention.
func (c *Config) SetLibraryNamingConvention(libraryNamingConvention string) {
	c.libraryNamingConvention = libraryNamingConvention
}

// RenderLibraryName returns the py_library target name by performing all
// substitutions.
func (c *Config) RenderLibraryName(packageName string) string {
	return strings.ReplaceAll(c.libraryNamingConvention, packageNameNamingConventionSubstitution, packageName)
}

// SetBinaryNamingConvention sets the py_binary target naming convention.
func (c *Config) SetBinaryNamingConvention(binaryNamingConvention string) {
	c.binaryNamingConvention = binaryNamingConvention
}

// RenderBinaryName returns the py_binary target name by performing all
// substitutions.
func (c *Config) RenderBinaryName(packageName string) string {
	return strings.ReplaceAll(c.binaryNamingConvention, packageNameNamingConventionSubstitution, packageName)
}

// SetTestNamingConvention sets the py_test target naming convention.
func (c *Config) SetTestNamingConvention(testNamingConvention string) {
	c.testNamingConvention = testNamingConvention
}

// RenderTestName returns the py_test target name by performing all
// substitutions.
func (c *Config) RenderTestName(packageName string) string {
	return strings.ReplaceAll(c.testNamingConvention, packageNameNamingConventionSubstitution, packageName)
}

// SetProtoNamingConvention sets the py_proto_library target naming convention.
func (c *Config) SetProtoNamingConvention(protoNamingConvention string) {
	c.protoNamingConvention = protoNamingConvention
}

// RenderProtoName returns the py_proto_library target name by performing all
// substitutions.
func (c *Config) RenderProtoName(protoName string) string {
	return strings.ReplaceAll(c.protoNamingConvention, protoNameNamingConventionSubstitution, strings.TrimSuffix(protoName, "_proto"))
}

// AppendVisibility adds additional items to the target's visibility.
func (c *Config) AppendVisibility(visibility string) {
	c.visibility = append(c.visibility, visibility)
}

// Visibility returns the target's visibility.
func (c *Config) Visibility() []string {
	return append(c.defaultVisibility, c.visibility...)
}

// SetDefaultVisibility sets the default visibility of the target.
func (c *Config) SetDefaultVisibility(visibility []string) {
	c.defaultVisibility = visibility
}

// DefaultVisibilty returns the target's default visibility.
func (c *Config) DefaultVisibilty() []string {
	return c.defaultVisibility
}

// SetTestFilePattern sets the file patterns that should be mapped to 'py_test' rules.
func (c *Config) SetTestFilePattern(patterns []string) {
	c.testFilePattern = patterns
}

// TestFilePattern returns the patterns that should be mapped to 'py_test' rules.
func (c *Config) TestFilePattern() []string {
	return c.testFilePattern
}

// SetLabelConvention sets the label convention used for third-party dependencies.
func (c *Config) SetLabelConvention(convention string) {
	c.labelConvention = convention
}

// LabelConvention returns the label convention used for third-party dependencies.
func (c *Config) LabelConvention() string {
	return c.labelConvention
}

// SetLabelConvention sets the label normalization applied to distribution names of third-party dependencies.
func (c *Config) SetLabelNormalization(normalizationType LabelNormalizationType) {
	c.labelNormalization = normalizationType
}

// LabelConvention returns the label normalization applied to distribution names of third-party dependencies.
func (c *Config) LabelNormalization() LabelNormalizationType {
	return c.labelNormalization
}

// SetExperimentalAllowRelativeImports sets whether relative imports are allowed.
func (c *Config) SetExperimentalAllowRelativeImports(allowRelativeImports bool) {
	c.experimentalAllowRelativeImports = allowRelativeImports
}

// ExperimentalAllowRelativeImports returns whether relative imports are allowed.
func (c *Config) ExperimentalAllowRelativeImports() bool {
	return c.experimentalAllowRelativeImports
}

// SetGeneratePyiDeps sets whether pyi_deps attribute should be generated separately
// or type-checking dependencies should be merged into the regular deps attribute.
func (c *Config) SetGeneratePyiDeps(generatePyiDeps bool) {
	c.generatePyiDeps = generatePyiDeps
}

// GeneratePyiDeps returns whether pyi_deps attribute should be generated separately
// or type-checking dependencies should be merged into the regular deps attribute.
func (c *Config) GeneratePyiDeps() bool {
	return c.generatePyiDeps
}

// SetGenerateProto sets whether py_proto_library should be generated for proto_library.
func (c *Config) SetGenerateProto(generateProto bool) {
	c.generateProto = generateProto
}

// GenerateProto returns whether py_proto_library should be generated for proto_library.
func (c *Config) GenerateProto() bool {
	return c.generateProto
}

// SetResolveSiblingImports sets whether absolute imports can be resolved to sibling modules.
func (c *Config) SetResolveSiblingImports(resolveSiblingImports bool) {
	c.resolveSiblingImports = resolveSiblingImports
}

// ResolveSiblingImports returns whether absolute imports can be resolved to sibling modules.
func (c *Config) ResolveSiblingImports() bool {
	return c.resolveSiblingImports
}

// FormatThirdPartyDependency returns a label to a third-party dependency performing all formating and normalization.
func (c *Config) FormatThirdPartyDependency(repositoryName string, distributionName string) label.Label {
	conventionalDistributionName := strings.ReplaceAll(c.labelConvention, distributionNameLabelConventionSubstitution, distributionName)

	var normConventionalDistributionName string
	switch norm := c.LabelNormalization(); norm {
	case SnakeCaseLabelNormalizationType:
		// See /python/private/normalize_name.bzl
		normConventionalDistributionName = strings.ToLower(conventionalDistributionName)
		normConventionalDistributionName = regexp.MustCompile(`[-_.]+`).ReplaceAllString(normConventionalDistributionName, "_")
		normConventionalDistributionName = strings.Trim(normConventionalDistributionName, "_")
	case Pep503LabelNormalizationType:
		// See https://packaging.python.org/en/latest/specifications/name-normalization/#name-format
		normConventionalDistributionName = strings.ToLower(conventionalDistributionName)                                        // ... "should be lowercased"
		normConventionalDistributionName = regexp.MustCompile(`[-_.]+`).ReplaceAllString(normConventionalDistributionName, "-") // ... "all runs of the characters ., -, or _ replaced with a single -"
		normConventionalDistributionName = strings.Trim(normConventionalDistributionName, "-")                                  // ... "must start and end with a letter or number"
	default:
		fallthrough
	case NoLabelNormalizationType:
		normConventionalDistributionName = conventionalDistributionName
	}

	return label.New(repositoryName, normConventionalDistributionName, normConventionalDistributionName)
}

func loadGazelleManifest(gazelleManifestPath string) (*manifest.Manifest, error) {
	if _, err := os.Stat(gazelleManifestPath); err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		}
		return nil, fmt.Errorf("failed to load Gazelle manifest at %q: %w", gazelleManifestPath, err)
	}
	manifestFile := new(manifest.File)
	if err := manifestFile.Decode(gazelleManifestPath); err != nil {
		return nil, fmt.Errorf("failed to load Gazelle manifest at %q: %w", gazelleManifestPath, err)
	}
	return manifestFile.Manifest, nil
}
