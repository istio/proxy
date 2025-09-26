#!/usr/bin/env python3

from subprocess import check_output
import glob
import os
import shutil
from language_config import GoConfig, PythonConfig, LanguageConfig

workspace = check_output(["bazel", "info", "workspace"]).decode().strip()
LANG_CONFIGS = {config.language: config for config in [GoConfig(), PythonConfig()]}

def generate_lang_files_from_protos(language_config: LanguageConfig):
    language = language_config.language
    print(f"Generating proto code in language {language}")
    output = os.path.join(workspace, language)
    bazel_bin = check_output(["bazel", "info", "bazel-bin"]).decode().strip()

    protos = check_output(
        [
            "bazel",
            "query",
            language_config.bazel_query_kind,
        ]
    ).split()
    output_dir = f"github.com/cncf/xds/{language}"
    check_output(["bazel", "build", "-c", "fastbuild"] + protos)
    print(f"Found {len(protos)} bazel rules to generate code")
    for rule in protos:
        rule_dir = rule.decode()[2:].rsplit(":")[0]
        input_dir = language_config.get_input_dir(bazel_bin, rule_dir)
        input_files = glob.glob(os.path.join(input_dir, language_config.generated_file_pattern))    
        output_dir = os.path.join(output, rule_dir)
        print(f"Moving {len(input_files)} generated files from {input_dir} to output_dir {output_dir}")
        # Ensure the output directory exists
        os.makedirs(output_dir, 0o755, exist_ok=True)
        for generated_file in input_files:
            output_file = shutil.copy(generated_file, output_dir)
            os.chmod(output_file, 0o644)


if __name__ == "__main__":
    for config in LANG_CONFIGS.values():
        generate_lang_files_from_protos(language_config=config)