// Copyright 2020 Junekey Jeon
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

#include "big_uint.h"
#include "dragonbox/dragonbox.h"

#include <algorithm>
#include <iostream>

static std::int_least32_t floor_log10_pow2_precise(std::int_least32_t e) {
    using namespace jkj::dragonbox::detail::log;
    bool is_negative;
    if (e < 0) {
        is_negative = true;
        e = -e;
    }
    else {
        is_negative = false;
    }

    auto power_of_2 = jkj::big_uint::power_of_2(std::size_t(e));
    auto power_of_10 = jkj::big_uint(1);
    std::int_least32_t k = 0;
    while (power_of_10 <= power_of_2) {
        power_of_10.multiply_5();
        power_of_10.multiply_2();
        ++k;
    }

    return is_negative ? -k : k - 1;
}

static std::int_least32_t floor_log10_pow2_minus_log10_4_over_3_precise(std::int_least32_t e) {
    e -= 2;

    if (e < 0) {
        e = -e;
        auto power_of_2 = jkj::big_uint::power_of_2(std::size_t(e));
        auto power_of_10_times_3 = jkj::big_uint(3);
        std::int_least32_t k = 0;
        while (power_of_10_times_3 < power_of_2) {
            power_of_10_times_3.multiply_5();
            power_of_10_times_3.multiply_2();
            ++k;
        }
        return -k;
    }
    else {
        auto power_of_2_times_3 = jkj::big_uint::power_of_2(std::size_t(e)) * 3;
        auto power_of_10 = jkj::big_uint(1);
        std::int_least32_t k = 0;
        while (power_of_10 <= power_of_2_times_3) {
            power_of_10.multiply_5();
            power_of_10.multiply_2();
            ++k;
        }
        return k - 1;
    }
}

static std::int_least32_t floor_log2_pow10_precise(std::int_least32_t e) {
    bool is_negative;
    if (e < 0) {
        is_negative = true;
        e = -e;
    }
    else {
        is_negative = false;
    }

    auto power_of_10 = jkj::big_uint(1);
    for (std::int_least32_t i = 0; i < e; ++i) {
        power_of_10.multiply_5();
        power_of_10.multiply_2();
    }

    auto k = std::int_least32_t(log2p1(power_of_10));

    return is_negative ? -k : k - 1;
}

static std::int_least32_t floor_log5_pow2_precise(std::int_least32_t e) {
    bool is_negative;
    if (e < 0) {
        is_negative = true;
        e = -e;
    }
    else {
        is_negative = false;
    }

    auto power_of_2 = jkj::big_uint::power_of_2(std::size_t(e));
    auto power_of_5 = jkj::big_uint(1);
    std::int_least32_t k = 0;
    while (power_of_5 <= power_of_2) {
        power_of_5.multiply_5();
        ++k;
    }

    return is_negative ? -k : k - 1;
}

static std::int_least32_t floor_log5_pow2_minus_log5_3_precise(std::int_least32_t e) {
    if (e >= 0) {
        auto power_of_2 = jkj::big_uint::power_of_2(std::size_t(e));
        auto power_of_5_times_3 = jkj::big_uint(3);
        std::int_least32_t k = 0;
        while (power_of_5_times_3 <= power_of_2) {
            power_of_5_times_3.multiply_5();
            ++k;
        }
        return k - 1;
    }
    else {
        e = -e;
        auto power_of_2_times_3 = jkj::big_uint::power_of_2(std::size_t(e)) * 3;
        auto power_of_5 = jkj::big_uint(1);
        std::int_least32_t k = 0;
        while (power_of_5 < power_of_2_times_3) {
            power_of_5.multiply_5();
            ++k;
        }
        return -k;
    }
}

struct verify_result {
    std::int_least32_t min_exponent;
    std::int_least32_t max_exponent;
};

template <class FastCalculatorInfo, class PreciseCalculator>
static verify_result verify(std::string_view name, std::size_t tier,
                            PreciseCalculator&& precise_calculator) {
    // Compute the maximum possible exponent for ensuring no overflow.
    using info = FastCalculatorInfo;
    using intermediate_type = decltype(info::multiply);
    using return_type = typename info::default_return_type;

    constexpr auto max_intermediate_value = std::min(
        std::numeric_limits<intermediate_type>::max(),
        intermediate_type(
            (std::min(static_cast<intermediate_type>(std::numeric_limits<return_type>::max()),
                      intermediate_type(std::numeric_limits<intermediate_type>::max() >> info::shift)) *
             (intermediate_type(1) << info::shift)) +
            ((intermediate_type(1) << info::shift) - 1)));
    constexpr auto no_overflow_max_exponent =
        (max_intermediate_value + std::min(info::subtract, intermediate_type(0))) / info::multiply;

    constexpr auto min_intermediate_value =
        std::max(std::numeric_limits<intermediate_type>::min(),
                 intermediate_type(
                     (std::max(static_cast<intermediate_type>(std::numeric_limits<return_type>::min()),
                               intermediate_type((std::numeric_limits<intermediate_type>::min() +
                                                  (intermediate_type(1) << (info::shift + 1)) - 2) >>
                                                 info::shift)) *
                      (intermediate_type(1) << info::shift)) -
                     ((intermediate_type(1) << info::shift) - 1)));
    constexpr auto no_overflow_min_exponent =
        (min_intermediate_value + std::max(info::subtract, intermediate_type(0))) /
        info::multiply; // (negative) / (positive) computes the ceiling in C/C++.


    verify_result result{std::int_least32_t(no_overflow_min_exponent),
                         std::int_least32_t(no_overflow_max_exponent)};

    bool reach_upper_bound = false;
    bool reach_lower_bound = false;
    for (std::int_least32_t e = 0; e <= std::max(-no_overflow_min_exponent, no_overflow_max_exponent);
         ++e) {
        if (!reach_upper_bound) {
            auto true_value = precise_calculator(e);
            auto computed_value = (e * info::multiply - info::subtract) >> info::shift;
            if (computed_value != true_value) {
                std::cout << "  - error with positive e ("
                          << "e: " << e << ", true value: " << true_value
                          << ", computed value: " << computed_value << ")\n";

                reach_upper_bound = true;
                result.max_exponent = e - 1;
            }
        }

        if (!reach_lower_bound) {
            auto true_value = precise_calculator(-e);
            auto computed_value = static_cast<std::int_least32_t>(
                static_cast<return_type>((-e * info::multiply - info::subtract) >> info::shift));
            if (computed_value != true_value) {
                std::cout << "  - error with negative e ("
                          << "e: " << -e << ", true value: " << true_value
                          << ", computed value: " << computed_value << ")\n";

                reach_lower_bound = true;
                result.min_exponent = -e + 1;
            }
        }

        if (reach_upper_bound && reach_lower_bound) {
            break;
        }
    }

    std::cout << name << " (tier: " << tier << ") is correct for e in [" << result.min_exponent << ", "
              << result.max_exponent << "]\n\n";

    return result;
}

template <template <std::size_t> class FastCalculatorInfo, std::size_t current_tier = 0>
struct verify_all_tiers {
    template <class PreciseCalculator, std::size_t dummy1 = current_tier,
              std::int_least32_t dummy2 = FastCalculatorInfo<dummy1>::min_exponent>
    bool operator()(std::string_view name, PreciseCalculator&& precise_calculator) {
        if (current_tier == 0) {
            std::cout << "Verifying " << name << "...\n\n";
        }
        auto const result =
            verify<FastCalculatorInfo<current_tier>>(name, current_tier, precise_calculator);

        bool success = result.min_exponent <= FastCalculatorInfo<current_tier>::min_exponent &&
                       result.max_exponent >= FastCalculatorInfo<current_tier>::max_exponent;

        return verify_all_tiers<FastCalculatorInfo, current_tier + 1>{}(name, precise_calculator) &&
               success;
    }

    bool operator()(...) {
        std::cout << "\n\n";
        return true;
    }
};

int main() {
    using namespace jkj::dragonbox::detail::log;

    bool success = true;
    std::cout << "[Verifying log computation...]\n";

    success &= verify_all_tiers<floor_log10_pow2_info>{}("floor_log10_pow2", floor_log10_pow2_precise);
    success &= verify_all_tiers<floor_log2_pow10_info>{}("floor_log2_pow10", floor_log2_pow10_precise);
    success &= verify_all_tiers<floor_log10_pow2_minus_log10_4_over_3_info>{}(
        "floor_log10_pow2_minus_log10_4_over_3", floor_log10_pow2_minus_log10_4_over_3_precise);
    success &= verify_all_tiers<floor_log5_pow2_info>{}("floor_log5_pow2", floor_log5_pow2_precise);
    success &= verify_all_tiers<floor_log5_pow2_minus_log5_3_info>{}(
        "floor_log5_pow2_minus_log5_3", floor_log5_pow2_minus_log5_3_precise);

    if (success) {
        std::cout << "Done. No error detected.\n\n\n";
    }
    else {
        std::cout << "Error detected.\n\n\n";
        return -1;
    }
}
