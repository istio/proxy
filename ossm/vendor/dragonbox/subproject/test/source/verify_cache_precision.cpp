// Copyright 2020-2022 Junekey Jeon
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

#include "best_rational_approx.h"
#include "good_rational_approx.h"
#include "big_uint.h"
#include "rational_continued_fractions.h"
#include "dragonbox/dragonbox.h"
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

static std::ostream& operator<<(std::ostream& out, jkj::big_uint const& n) {
    auto dec = n.to_decimal();
    assert(!dec.empty());

    out << dec.back();

    auto cur_fill = out.fill();
    out << std::setfill('0');

    for (std::size_t back_idx = 0; back_idx < dec.size() - 1; ++back_idx) {
        out << std::setw(19) << dec[dec.size() - back_idx - 2];
    }

    out << std::setfill(cur_fill);
    return out;
}

struct analysis_result {
    struct result_per_cache_entry {
        int sufficient_bits_for_multiplication;
        int sufficient_bits_for_integer_checks;
        jkj::unsigned_rational<jkj::big_uint> distance_to_upper_bound;
    };
    std::vector<result_per_cache_entry> results;

    struct error_case {
        int e;
        int k;
        jkj::unsigned_rational<jkj::big_uint> target;
        jkj::unsigned_rational<jkj::big_uint> unit;
        std::vector<jkj::big_uint> candidate_multipliers{};
    };
    std::vector<error_case> error_cases;
};

template <class FormatTraits>
static bool analyze(std::ostream& out, std::size_t cache_bits) {
    out << "e,bits_for_multiplication,bits_for_integer_check\n";

    using impl = jkj::dragonbox::detail::impl<FormatTraits>;
    using namespace jkj::dragonbox::detail::log;

    auto n_max = jkj::big_uint::power_of_2(impl::significand_bits + 2);

    analysis_result result;
    result.results.resize(impl::max_k - impl::min_k + 1);

    jkj::unsigned_rational<jkj::big_uint> target{1, 1}, unit;
    int prev_k = impl::max_k + 1;
    for (int e = impl::min_exponent - impl::significand_bits;
         e <= impl::max_exponent - impl::significand_bits; ++e) {
        int k = impl::kappa - floor_log10_pow2(e);
        auto exp_2 = k - floor_log2_pow10(k) - 1;
        int beta = e + floor_log2_pow10(k);

        auto& results_for_k = result.results[k - impl::min_k];

        // target = 2^(k - klog2(10) - 1) * 5^k = phi_k / 2^Q in [1/2, 1).
        if (k != prev_k) {
            target.numerator = 1;
            target.denominator = 1;
            if (k >= 0) {
                target.numerator = jkj::big_uint::pow(5, k);
            }
            else {
                target.denominator = jkj::big_uint::pow(5, -k);
            }
            if (exp_2 >= 0) {
                target.numerator *= jkj::big_uint::power_of_2(exp_2);
            }
            else {
                target.denominator *= jkj::big_uint::power_of_2(-exp_2);
            }
        }

        // unit = 2^(e + k - 1) * 5^k = a/b.
        unit.numerator = 1;
        unit.denominator = 1;
        if (k >= 0) {
            unit.numerator = jkj::big_uint::pow(5, k);
        }
        else {
            unit.denominator = jkj::big_uint::pow(5, -k);
        }
        if (e + k - 1 >= 0) {
            unit.numerator *= jkj::big_uint::power_of_2(e + k - 1);
        }
        else {
            unit.denominator *= jkj::big_uint::power_of_2(-e - k + 1);
        }


        jkj::unsigned_rational<jkj::big_uint> upper_bound;
        int sufficient_bits_for_integer_checks;
        if (unit.denominator <= n_max) {
            if (unit.denominator == 1) {
                upper_bound = {unit.numerator * n_max + 1, n_max * jkj::big_uint::power_of_2(beta)};
            }
            else {
                // We want to find the largest v <= n_max such that va == -1 (mod b).
                // To obtain such v, we first find the smallest positive v0 such that
                // v0 * a == -1 (mod b). Then v = v0 + floor((n_max - v0)/b) * b.
                auto v0 =
                    jkj::find_best_rational_approx<jkj::rational_continued_fractions<jkj::big_uint>>(
                        unit, unit.denominator - 1)
                        .above.denominator;
                auto v = v0 + ((n_max - v0) / unit.denominator) * unit.denominator;

                auto div_result = div(v * unit.numerator + 1, unit.denominator);
                assert(div_result.rem.is_zero());
                upper_bound = jkj::unsigned_rational<jkj::big_uint>{
                    div_result.quot, v * jkj::big_uint::power_of_2(beta)};
            }

            sufficient_bits_for_integer_checks =
                impl::carrier_bits + int(jkj::big_uint(1).multiply_2_until(unit.denominator));
        }
        else {
            auto [below, above] =
                jkj::find_best_rational_approx<jkj::rational_continued_fractions<jkj::big_uint>>(unit,
                                                                                                 n_max);

            upper_bound = std::move(above);
            upper_bound.denominator *= jkj::big_uint::power_of_2(beta);

            sufficient_bits_for_integer_checks =
                impl::carrier_bits +
                int((unit.numerator * below.denominator - below.numerator * unit.denominator)
                        .multiply_2_until(unit.denominator));

            // Collect all cases where cache_bits seems insufficient.
            if (sufficient_bits_for_integer_checks > cache_bits) {
                result.error_cases.push_back({e, k, target, unit});
            }
        }

        // Compute the required number of bits for successful multiplication.
        // The following is an upper bound.
        auto div_result = div(upper_bound.denominator * target.denominator,
                              upper_bound.numerator * target.denominator -
                                  upper_bound.denominator * target.numerator);
        if (!div_result.rem.is_zero()) {
            div_result.quot += 1;
        }
        auto sufficient_bits_for_multiplication =
            int(jkj::big_uint(1).multiply_2_until(div_result.quot));

        // Tentatively decrease the above result to find the minimum admissible value.
        while (sufficient_bits_for_multiplication > 0) {
            auto r =
                (jkj::big_uint::power_of_2(sufficient_bits_for_multiplication - 1) * target.numerator) %
                target.denominator;
            if (!r.is_zero()) {
                r = target.denominator - r;
            }

            if (r * upper_bound.denominator >=
                jkj::big_uint::power_of_2(sufficient_bits_for_multiplication - 1) *
                    (upper_bound.numerator * target.denominator -
                     upper_bound.denominator * target.numerator)) {
                break;
            }

            --sufficient_bits_for_multiplication;
        }

        out << e << "," << sufficient_bits_for_multiplication << ","
            << sufficient_bits_for_integer_checks << "\n";

        // Update.
        if (results_for_k.sufficient_bits_for_multiplication < sufficient_bits_for_multiplication) {
            results_for_k.sufficient_bits_for_multiplication = sufficient_bits_for_multiplication;
        }
        if (results_for_k.sufficient_bits_for_integer_checks < sufficient_bits_for_integer_checks) {
            results_for_k.sufficient_bits_for_integer_checks = sufficient_bits_for_integer_checks;
        }
        auto distance = jkj::unsigned_rational<jkj::big_uint>{
            upper_bound.numerator * target.denominator - upper_bound.denominator * target.numerator,
            upper_bound.denominator * target.denominator};
        if (results_for_k.distance_to_upper_bound.denominator.is_zero()) {
            results_for_k.distance_to_upper_bound = std::move(distance);
        }
        else if (results_for_k.distance_to_upper_bound.numerator * distance.denominator >
                 distance.numerator * results_for_k.distance_to_upper_bound.denominator) {
            results_for_k.distance_to_upper_bound = distance;
        }
    }

    // Analyze all error cases.
    auto reciprocal_error_threshold = jkj::big_uint::power_of_2(cache_bits - impl::carrier_bits);
    for (auto& ec : result.error_cases) {
        // We want to find all n such that
        // d:= na/b - floor(na/b) < 2^(q-Q).

        ec.candidate_multipliers = jkj::find_all_good_rational_approx_from_below_denoms<
            jkj::rational_continued_fractions<jkj::big_uint>>(
            ec.unit, n_max, jkj::unsigned_rational<jkj::big_uint>{1, reciprocal_error_threshold});
    }

    auto sufficient_bits_for_multiplication =
        std::max_element(result.results.cbegin(), result.results.cend(),
                         [](auto const& a, auto const& b) {
                             return a.sufficient_bits_for_multiplication <
                                    b.sufficient_bits_for_multiplication;
                         })
            ->sufficient_bits_for_multiplication;
    auto sufficient_bits_for_integer_checks =
        std::max_element(result.results.cbegin(), result.results.cend(),
                         [](auto const& a, auto const& b) {
                             return a.sufficient_bits_for_integer_checks <
                                    b.sufficient_bits_for_integer_checks;
                         })
            ->sufficient_bits_for_integer_checks;
    auto larger = std::max(sufficient_bits_for_multiplication, sufficient_bits_for_integer_checks);

    auto distance_to_upper_bound =
        std::min_element(
            result.results.cbegin(), result.results.cend(),
            [](auto const& a, auto const& b) {
                if (a.distance_to_upper_bound.denominator == 0) {
                    return false;
                }
                else if (b.distance_to_upper_bound.denominator == 0) {
                    return true;
                }
                return a.distance_to_upper_bound.numerator * b.distance_to_upper_bound.denominator <
                       b.distance_to_upper_bound.numerator * a.distance_to_upper_bound.denominator;
            })
            ->distance_to_upper_bound;

    // Reduce the fraction.
    distance_to_upper_bound =
        jkj::find_best_rational_approx<jkj::rational_continued_fractions<jkj::big_uint>>(
            distance_to_upper_bound, distance_to_upper_bound.denominator)
            .below;

    std::cout << "An upper bound on the minimum required bits for successful multiplication is "
              << sufficient_bits_for_multiplication
              << "-bits.\nAn upper bound on the minimum required bits for successful integer "
                 "checks is "
              << sufficient_bits_for_integer_checks << "-bits.\n";
    std::cout << "A lower bound on the margin is " << distance_to_upper_bound.numerator << " / "
              << distance_to_upper_bound.denominator << ".\n";

    if (cache_bits < larger) {
        auto success = true;
        std::cout << "Error cases:\n";
        auto threshold = jkj::big_uint::power_of_2(impl::significand_bits + 1) - 1;
        for (auto const& ec : result.error_cases) {
            for (auto const& n : ec.candidate_multipliers) {
                std::cout << "  e: " << ec.e << "  k: " << ec.k << "  n: " << n;

                // When e != min_e and n != 1, 2, then
                // n must be at least 2^(p+1)-2, otherwise this is a false
                // positive.

                if (ec.e != impl::min_exponent - impl::significand_bits && n != 1 && n != 2 &&
                    n < threshold) {
                    std::cout << "\n    n is smaller than " << threshold
                              << ", so this case is a false positive.";
                }
                else if ((ec.e == -81 && n == 29711844) || (ec.e == -80 && n == 29711844)) {
                    std::cout << "\n    This case has been carefully addressed.";
                }
                else {
                    success = false;
                }

                std::cout << "\n\n";
            }
        }

        if (!success) {
            std::cout << "Verification failed. " << cache_bits << "-bits are not sufficient.\n\n";
            return false;
        }
    }

    std::cout << "Verified. " << cache_bits << "-bits are sufficient.\n\n";
    return true;
}



int main() {
    bool success = true;
    std::ofstream out;

    std::cout << "[Verifying sufficiency of cache precision for binary32...]\n";
    out.open("results/binary32.csv");
    if (!analyze<jkj::dragonbox::ieee754_binary_traits<jkj::dragonbox::ieee754_binary32,
                                                       std::uint_least32_t>>(out, 64)) {
        success = false;
    }
    out.close();

    std::cout << "[Verifying sufficiency of cache precision for binary64...]\n";
    out.open("results/binary64.csv");
    if (!analyze<jkj::dragonbox::ieee754_binary_traits<jkj::dragonbox::ieee754_binary64,
                                                        std::uint_least64_t>>(out, 128)) {
        success = false;
    }
    out.close();

    return success ? 0 : -1;
}