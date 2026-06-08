#!/usr/bin/env python3

from subprocess import check_output

import os

from abc import ABC, abstractmethod

workspace = check_output(["bazel", "info", "workspace"]).decode().strip()


class LanguageConfig(ABC):
    """Abstract base class for language-specific configurations."""

    @property
    @abstractmethod
    def language(self) -> str:
        """Returns the language name."""
        pass
    
    @property
    @abstractmethod
    def bazel_query_kind(self) -> str:
        """Returns the Bazel query kind for this language."""
        pass

    @property
    @abstractmethod
    def generated_file_pattern(self) -> str:
        """Returns the file pattern for generated files."""
        pass

    @abstractmethod
    def get_input_dir(self, bazel_bin: str, rule_dir: str) -> str:
        """Constructs the input directory path for generated files."""
        pass


class GoConfig(LanguageConfig):
    """Configuration for Go."""

    @property
    def language(self) -> str:
        return "go"

    @property
    def bazel_query_kind(self) -> str:
        return 'kind("go_proto_library", ...)'

    @property
    def generated_file_pattern(self) -> str:
        return "*.go"

    def get_input_dir(self, bazel_bin: str, rule_dir: str) -> str:
        return os.path.join(
            bazel_bin, rule_dir, "pkg_go_proto_", "github.com/cncf/xds/go", rule_dir
        )


class PythonConfig(LanguageConfig):
    """Configuration for Python."""

    @property
    def language(self) -> str:
        return "python"

    @property
    def bazel_query_kind(self) -> str:
        return 'kind("py_proto_library", ...)'

    @property
    def generated_file_pattern(self) -> str:
        return "*_pb2.py"

    def get_input_dir(self, bazel_bin: str, rule_dir: str) -> str:
        return os.path.join(bazel_bin, rule_dir)


