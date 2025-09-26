// Copyright 2020 The Bazel Authors. All rights reserved.
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

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

void basic_part1_test(std::string current_dir_arg) {
    if (current_dir_arg != "--current-dir") {
        std::cerr << "error: argument \"--current-dir\" not found.\n";
        std::exit(1);
    }
}

void basic_part2_test(std::string current_dir, const char* envp[]) {
    if (current_dir != "${pwd}") {
        std::cerr << "error: unsubsituted ${pwd} not found.\n";
        std::exit(1);
    }
    const std::string current_dir_env = "CURRENT_DIR=${pwd}/test_path";
    bool found = false;
    for (int i = 0; envp[i] != nullptr; ++i) {
        if (current_dir_env == envp[i]) {
            found = true;
            break;
        }
    }
    if (!found) {
        std::cerr << "unsubsituted CURRENT_DIR not found.\n";
        std::exit(1);
    }
}

void subst_pwd_test(int argc, const char* argv[], const char* envp[]) {
    std::string current_dir = argv[3];
    if (current_dir.find("${pwd}") != std::string::npos) {
        std::cerr << "error: argument ${pwd} substitution failed.\n";
        std::exit(1);
    }
    // find the param file using its "@" prefix
    std::string param_file;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '@') {
            param_file = std::string(argv[i] + 1);
            break;
        }
    }
    if (param_file.empty()) {
        std::cerr << "error: no param file.\n";
        std::exit(1);
    }
    std::string param_file_line;
    getline(std::ifstream(param_file), param_file_line);
    if (param_file_line != current_dir) {
        std::cerr << "error: param file " << param_file << " should contain "
                  << current_dir << ", found " << param_file_line << ".\n";
        std::exit(1);
    }
    bool found = false;
    for (int i = 0; envp[i] != nullptr; ++i) {
        const std::string env = envp[i];
        if (env.rfind("CURRENT_DIR", 0) == 0) {
            found = true;
            if (env.find("${pwd}") != std::string::npos) {
                std::cerr << "error: environment variable ${pwd} substitution "
                             "failed.\n";
                std::exit(1);
            }
            break;
        }
    }
    if (!found) {
        std::cerr << "CURRENT_DIR not found.\n";
        std::exit(1);
    }
}

void env_files_test(const char* envp[]) {
    const std::string must_exist[] = {
        "FOO=BAR",
        "FOOBAR=BARFOO",
        "BAR=FOO",
        "ENV_ESCAPE=with\nnew line",
        "ENV_NO_ESCAPE=with no new line\\",
        "ENV_ESCAPE_WITH_BACKSLASH=new line\\\nhere",
    };
    for (const std::string& env : must_exist) {
        bool found = false;
        for (int i = 0; envp[i] != nullptr; ++i) {
            if (env == envp[i]) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "error: environment variable \"" << env
                      << "\" not found.\n";
            std::exit(1);
        }
    }
}

void arg_files_test(int argc, const char* argv[]) {
    const std::string must_exist[] = {
        "--arg1=foo",
        "--arg2",
        "foo bar",
        "--arg2=bar",
        "--arg3",
        "foobar",
        "arg with\nnew line",
        "arg with\\",
        "no new line",
        "arg with\\\nnew line and a trailing backslash",
    };

    for (const std::string& arg : must_exist) {
        bool found = false;
        for (int i = 0; i < argc; ++i) {
            if (arg == argv[i]) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "error: argument \"" << arg << "\" not found.\n";
            std::exit(1);
        }
    }
}

void test_stdout() {
    for (int i = 0; i < 10000; ++i) {
// On windows writing LF to any stream in text mode gets changed to CRLF
// Since the test file is saved using CRLF, we are forcing the same on
// non windows systems
#if defined(_WIN32)
        std::cout << "Child process to stdout : " << i << "\n";
#else
        std::cout << "Child process to stdout : " << i << "\r\n";
#endif  // defined(_WIN32)
    }
}

void test_stderr() { std::cerr << "This is the stderr output"; }

int main(int argc, const char* argv[], const char* envp[]) {
    if (argc < 4) {
        std::cerr << "error: Invalid number of args exected at least 4 got "
                  << argc << ".\n";
        return 1;
    }
    std::string test_config = argv[1];
    bool combined = test_config == "combined";
    if (combined || test_config == "basic") {
        basic_part1_test(argv[2]);
    }

    if (combined || test_config == "subst-pwd") {
        subst_pwd_test(argc, argv, envp);
    } else if (test_config == "basic") {
        basic_part2_test(argv[3], envp);
    }

    if (combined || test_config == "env-files") {
        env_files_test(envp);
    }

    if (combined || test_config == "arg-files") {
        arg_files_test(argc, argv);
    }

    if (combined || test_config == "stdout") {
        test_stdout();
    }

    if (combined || test_config == "stderr") {
        test_stderr();
    }
}
