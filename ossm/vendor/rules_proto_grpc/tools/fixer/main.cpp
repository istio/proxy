#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>


bool file_exists(const std::string& path) {
    return std::ifstream{path}.good();
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << path << "\n";
        std::exit(3);
    };

    std::stringstream file_stream;
    file_stream << file.rdbuf();
    file.close();

    return std::string(file_stream.str());
}

std::vector<std::string> read_file_lines(const std::string& path) {
    std::ifstream file{path};
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << path << "\n";
        std::exit(2);
    };

    std::stringstream file_stream;
    file_stream << file.rdbuf();
    file.close();
    std::vector<std::string> line_list;
    std::string line;

    while (std::getline(file_stream, line)) {
        line_list.push_back(line);
    }

    return line_list;
}

int main(int argc, char **argv, char **envp) {
    if (argc != 5) {
        std::cerr << "Expected 4 arguments in order: <path list file> <template file> <source dir> <target dir>\n";
        std::exit(1);
    }

    // Load in expected path list and template file
    const auto path_list = read_file_lines(argv[1]);
    const std::string template_str = read_file(argv[2]);

    // Load source and target dirs
    const std::string source_dir = argv[3];
    const std::string target_dir = argv[4];

    // Build template vars
    std::map<std::string, std::string> common_template_vars;
    common_template_vars["go_package"] = "";

    // Attempt to find Go package
    // When the fixer is applied to Go generated sources, it needs to generate files that have a package that match the
    // existing files. This assumes that at least one file has been generated, which is typically safe
    bool found_go_files = false;
    for (const auto& path : path_list) {
        // Check file is a go file
        const auto extension = path.substr(path.rfind(".") + 1);
        if (extension != "go") continue;
        found_go_files = true;

        // Check file exists
        std::string full_path = source_dir + "/" + path;
        if (!file_exists(full_path)) continue;

        // Attempt to grab package
        // Doing this with iterating over lines vs std::regex::multiline, to support C++11 as minimum standard
        const auto file_lines = read_file_lines(full_path);
        std::smatch package_match;
        for (const auto& line : file_lines) {
            if (std::regex_match(line, package_match, std::regex("^package ([a-zA-Z0-9_-]+)$"))) {
                common_template_vars["go_package"] = package_match[1];
                break;
            }
        }
    }

    if (found_go_files && common_template_vars["go_package"] == "") {
        std::cerr << "Warning: failed to find go package for templating go files, falling back to parent dir name";
    }

    // Copy or create each file in the target directory
    for (const auto& path : path_list) {
        // Skip blank lines
        if (path.length() == 0) continue;

        // Open target file
        const std::string target_path = target_dir + "/" + path;
        std::ofstream target_file{target_path};
        if (!target_file.good()) {
            std::cerr << "Failed to open target file: " << target_path << "\n";
            std::exit(4);
        };

        const std::string source_path = source_dir + "/" + path;
        if (file_exists(source_path)) {
            // Source file exists, copy
            std::ifstream source_file{source_path};
            if (!source_file.is_open()) {
                std::cerr << "Failed to open source file\n";
                std::exit(5);
            };

            target_file << source_file.rdbuf();
            source_file.close();

        } else {
            // Source file does not exist, write target file with template
            //std::cout << "Fixing missing plugin output file: " << path << "\n";

            // Build file specific template vars
            std::map<std::string, std::string> file_template_vars{common_template_vars};

            std::string parent_path = target_path.substr(0, target_path.rfind("/"));
            file_template_vars["parent_directory_name"] = parent_path.substr(parent_path.rfind("/") + 1);
            if (file_template_vars["go_package"] == "") {
                file_template_vars["go_package"] = file_template_vars["parent_directory_name"];
            }

            // Replace template variables
            std::string file_template_str = std::string(template_str);
            for (const auto& tup : file_template_vars) {
                file_template_str = std::regex_replace(
                    file_template_str, std::regex("\\{" + tup.first + "\\}"), tup.second
                );
            }

            // Write filled template
            target_file << file_template_str;
        }

        target_file.close();
    }

    return 0;
}
