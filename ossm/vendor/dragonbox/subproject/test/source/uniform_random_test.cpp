// Copyright 2020-2024 Junekey Jeon
//
// The contents of this file may be used under the terms of
// the Apache License v2.0 with LLVM Exceptions.
//
//    (See accompanying file LICENSE-Apache or copy at
//     https://llvm.org/foundation/relicensing/LICENSE.txt)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

#include "dragonbox/dragonbox_to_chars.h"
#include "simple_dragonbox.h"
#include "random_float.h"
#include "ryu/ryu.h"

#include <iostream>
#include <string_view>
#include <utility>

static void reference_implementation(float x, char* buffer) { f2s_buffered(x, buffer); }
static void reference_implementation(double x, char* buffer) { d2s_buffered(x, buffer); }

template <class Float, class TestTarget>
static bool uniform_random_test(std::size_t number_of_tests, TestTarget&& test_target) {
    char buffer1[64];
    char buffer2[64];
    auto rg = generate_correctly_seeded_mt19937_64();
    bool success = true;
    for (std::size_t test_idx = 0; test_idx < number_of_tests; ++test_idx) {
        auto x = uniformly_randomly_generate_general_float<Float>(rg);

        // Check if the output is identical to the reference implementation (Ryu).
        test_target(x, buffer1);
        reference_implementation(x, buffer2);

        std::string_view view1(buffer1);
        std::string_view view2(buffer2);

        if (view1 != view2) {
            std::cout << "Error detected! [Reference = " << buffer2 << ", Dragonbox = " << buffer1
                      << "]\n";
            success = false;
        }
    }

    if (success) {
        std::cout << "Uniform random test with " << number_of_tests << " examples succeeded.\n";
    }
    else {
        std::cout << "Error detected.\n";
    }

    return success;
}

int main() {
    constexpr std::size_t number_of_uniform_random_tests_float = 10000000;
    constexpr bool run_float = true;
    constexpr bool run_float_with_compact_cache = true;
    constexpr bool run_simple_float = true;
    constexpr bool run_simpl_float_with_compact_cache = true;

    constexpr std::size_t number_of_uniform_random_tests_double = 10000000;
    constexpr bool run_double = true;
    constexpr bool run_double_with_compact_cache = true;
    constexpr bool run_simple_double = true;
    constexpr bool run_simple_double_with_compact_cache = true;

    bool success = true;

    if (run_float) {
        std::cout << "[Testing uniformly randomly generated binary32 inputs...]\n";
        success &=
            uniform_random_test<float>(number_of_uniform_random_tests_float, [](auto x, char* buffer) {
                jkj::dragonbox::to_chars(x, buffer);
            });
        std::cout << "Done.\n\n\n";
    }
    if (run_float_with_compact_cache) {
        std::cout << "[Testing uniformly randomly generated binary32 inputs (compact cache)...]\n";
        success &=
            uniform_random_test<float>(number_of_uniform_random_tests_float, [](auto x, char* buffer) {
                jkj::dragonbox::to_chars(x, buffer, jkj::dragonbox::policy::cache::compact);
            });
        std::cout << "Done.\n\n\n";
    }
    if (run_simple_float) {
        std::cout << "[Testing uniformly randomly generated binary32 inputs (simplified impl)...]\n";
        success &=
            uniform_random_test<float>(number_of_uniform_random_tests_float, [](auto x, char* buffer) {
                jkj::simple_dragonbox::to_chars(x, buffer);
            });
        std::cout << "Done.\n\n\n";
    }
    if (run_simpl_float_with_compact_cache) {
        std::cout << "[Testing uniformly randomly generated binary32 inputs (simplified impl, compact "
                     "cache)...]\n";
        success &= uniform_random_test<float>(number_of_uniform_random_tests_float, [](auto x,
                                                                                       char* buffer) {
            jkj::simple_dragonbox::to_chars(x, buffer, jkj::simple_dragonbox::policy::cache::compact);
        });
        std::cout << "Done.\n\n\n";
    }
    if (run_double) {
        std::cout << "[Testing uniformly randomly generated binary64 inputs...]\n";
        success &= uniform_random_test<double>(
            number_of_uniform_random_tests_double,
            [](auto x, char* buffer) { jkj::dragonbox::to_chars(x, buffer); });
        std::cout << "Done.\n\n\n";
    }
    if (run_double_with_compact_cache) {
        std::cout << "[Testing uniformly randomly generated binary64 inputs (compact cache)...]\n";
        success &= uniform_random_test<double>(
            number_of_uniform_random_tests_double, [](auto x, char* buffer) {
                jkj::dragonbox::to_chars(x, buffer, jkj::dragonbox::policy::cache::compact);
            });
        std::cout << "Done.\n\n\n";
    }
    if (run_simple_double) {
        std::cout << "[Testing uniformly randomly generated binary64 inputs (simplified impl)...]\n";
        success &= uniform_random_test<double>(
            number_of_uniform_random_tests_double,
            [](auto x, char* buffer) { jkj::simple_dragonbox::to_chars(x, buffer); });
        std::cout << "Done.\n\n\n";
    }
    if (run_simple_double_with_compact_cache) {
        std::cout << "[Testing uniformly randomly generated binary64 inputs with (simplified impl, "
                     "compact cache)...]\n";
        success &= uniform_random_test<double>(number_of_uniform_random_tests_double, [](auto x,
                                                                                         char* buffer) {
            jkj::simple_dragonbox::to_chars(x, buffer, jkj::simple_dragonbox::policy::cache::compact);
        });
        std::cout << "Done.\n\n\n";
    }

    if (!success) {
        return -1;
    }
}
