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
#include "ryu/ryu.h"

#include <iostream>
#include <iomanip>
#include <string_view>
#include <utility>

static void reference_implementation(float x, char* buffer) { f2s_buffered(x, buffer); }
static void reference_implementation(double x, char* buffer) { d2s_buffered(x, buffer); }

template <class Float, class TestTarget>
static bool test_all_shorter_interval_cases_impl(TestTarget&& test_target) {
    using conversion_traits = jkj::dragonbox::default_float_bit_carrier_conversion_traits<Float>;
    using ieee754_format_info = typename conversion_traits::format;
    using carrier_uint = typename conversion_traits::carrier_uint;

    char buffer1[64];
    char buffer2[64];

    bool success = true;
    for (int e = ieee754_format_info::min_exponent; e <= ieee754_format_info::max_exponent; ++e) {
        // Compose a floating-point number
        carrier_uint br = carrier_uint(e - ieee754_format_info::exponent_bias)
                          << ieee754_format_info::significand_bits;
        auto x = conversion_traits::carrier_to_float(br);

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
        std::cout << "All cases are verified.\n";
    }
    else {
        std::cout << "Error detected.\n";
    }
    return success;
}

int main() {
    bool success = true;

    std::cout << "[Testing all shorter interval cases for binary32...]\n";
    success &= test_all_shorter_interval_cases_impl<float>(
        [](auto x, char* buffer) { jkj::dragonbox::to_chars(x, buffer); });
    std::cout << "Done.\n\n\n";

    std::cout << "[Testing all shorter interval cases for binary32 (compact cache)...]\n";
    success &= test_all_shorter_interval_cases_impl<float>([](auto x, char* buffer) {
        jkj::dragonbox::to_chars(x, buffer, jkj::dragonbox::policy::cache::compact);
    });
    std::cout << "Done.\n\n\n";

    std::cout << "[Testing all shorter interval cases for binary32 (simplified impl)...]\n";
    success &= test_all_shorter_interval_cases_impl<float>(
        [](auto x, char* buffer) { jkj::simple_dragonbox::to_chars(x, buffer); });
    std::cout << "Done.\n\n\n";

    std::cout
        << "[Testing all shorter interval cases for binary32 (simplified impl, compact cache)...]\n";
    success &= test_all_shorter_interval_cases_impl<float>([](auto x, char* buffer) {
        jkj::simple_dragonbox::to_chars(x, buffer, jkj::simple_dragonbox::policy::cache::compact);
    });
    std::cout << "Done.\n\n\n";

    std::cout << "[Testing all shorter interval cases for binary64...]\n";
    success &= test_all_shorter_interval_cases_impl<double>(
        [](auto x, char* buffer) { jkj::dragonbox::to_chars(x, buffer); });
    std::cout << "Done.\n\n\n";

    std::cout << "[Testing all shorter interval cases for binary64 (compact cache)...]\n";
    success &= test_all_shorter_interval_cases_impl<double>([](auto x, char* buffer) {
        jkj::dragonbox::to_chars(x, buffer, jkj::dragonbox::policy::cache::compact);
    });
    std::cout << "Done.\n\n\n";

    std::cout << "[Testing all shorter interval cases for binary64 (simplified impl)...]\n";
    success &= test_all_shorter_interval_cases_impl<double>(
        [](auto x, char* buffer) { jkj::simple_dragonbox::to_chars(x, buffer); });
    std::cout << "Done.\n\n\n";

    std::cout
        << "[Testing all shorter interval cases for binary64 (simplified impl, compact cache)...]\n";
    success &= test_all_shorter_interval_cases_impl<double>([](auto x, char* buffer) {
        jkj::simple_dragonbox::to_chars(x, buffer, jkj::simple_dragonbox::policy::cache::compact);
    });
    std::cout << "Done.\n\n\n";

    if (!success) {
        return -1;
    }
}
