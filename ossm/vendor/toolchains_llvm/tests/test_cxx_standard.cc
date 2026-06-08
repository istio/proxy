#include <iostream>
#include <cstdlib>

int run_test(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Not enough arguments" << std::endl;
        return 1;
    }

    long expected_version = std::atol(argv[1]);

    if (expected_version == 0) {
        std::cout << "Invalid version argument, must be an integer" << std::endl;
        return 1;
    }

    if (expected_version != __cplusplus) {
        std::cout << "Expected version to be " << argv[1] << " but got " << __cplusplus << std::endl;
        return 1;
    }
    return 0;
}
