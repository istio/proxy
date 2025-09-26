// Copyright 2022 Junekey Jeon
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

#ifndef JKJ_HEADER_GOOD_RATIONAL_APPROX
#define JKJ_HEADER_GOOD_RATIONAL_APPROX

#include "continued_fractions.h"
#include <cassert>
#include <cstdlib>
#include <vector>

namespace jkj {
    // Find all denominators of rational approximations from below of denominators no more than
    // denominator_upper_bound for the given number x whose error in the strong sense is at most
    // error_threshold. The returned list might contain several different denominators of the same
    // rational number.
    template <class ContinuedFractionsImpl, class UInt, class PositiveNumberX,
              class PositiveNumberEpsilon>
    std::vector<typename ContinuedFractionsImpl::uint_type>
    find_all_good_rational_approx_from_below_denoms(PositiveNumberX const& x,
                                         UInt const& denominator_upper_bound,
                                         PositiveNumberEpsilon const& error_threshold) {
        assert(denominator_upper_bound > 0);

        using uint_type = typename ContinuedFractionsImpl::uint_type;
        std::vector<uint_type> results;

        if (error_threshold >= PositiveNumberEpsilon(1)) {
            for (UInt i = 1; i <= denominator_upper_bound; ++i) {
                results.push_back(i);
            }
            return results;
        }

        // Initialize a continued fractions calculator.
        ContinuedFractionsImpl cf{x};

        // We will find the first even semiconvergent whose error is at most the threshold.
        // If such semiconvergent has the denominator strictly bigger than the
        // denominator_upper_bound, then there is no n such that nx - floor(nx) <= epsilon.
        unsigned_rational<uint_type> previous_convergent; // even
        unsigned_rational<uint_type> current_convergent;  // odd
        uint_type semiconvergent_coeff = 0;
        unsigned_rational<uint_type> semiconvergent;

        auto perfect_approx_possible = [&] {
            // If there is no more convergent, we already obtained the perfect approximation.
            // This means that x = p/q is rational with q <= denominator_upper_bound.
            // In this case, just return q, 2q, 3q, ... up to denominator_upper_bound.

            auto denominator = cf.current_denominator();
            while (denominator <= denominator_upper_bound) {
                results.push_back(denominator);
                denominator += cf.current_denominator();
            }
        };

        auto update_semiconvergent = [&] {
            semiconvergent.numerator =
                previous_convergent.numerator + semiconvergent_coeff * current_convergent.numerator;
            semiconvergent.denominator = previous_convergent.denominator +
                                         semiconvergent_coeff * current_convergent.denominator;
        };

        auto gcd = [&](auto a, auto b) {
            decltype(a) t;
            while (b != 0) {
                t = b;
                b = a % b;
                a = static_cast<decltype(a)&&>(t);
            }
            return a;
        };

        auto subroutine_for_each_semiconvergent = [&] {
            auto push_all_multiples = [&](auto const& candidate) {
                auto error = candidate.denominator * x - candidate.numerator;
                auto error_multiple = error;
                auto denominator_multiple = candidate.denominator;
                while (error_multiple < error_threshold &&
                       denominator_multiple <= denominator_upper_bound) {
                    results.push_back(denominator_multiple);
                    error_multiple += error;
                    denominator_multiple += candidate.denominator;
                }
            };
            push_all_multiples(semiconvergent);

            auto b_max = uint_type(denominator_upper_bound) / semiconvergent.denominator;

            for (decltype(b_max) b = 2; b <= b_max; ++b) {
                unsigned_rational<uint_type> candidate{b * semiconvergent.numerator,
                                                       b * semiconvergent.denominator};

                auto a_max = (uint_type(denominator_upper_bound) - candidate.denominator) /
                             current_convergent.denominator;
                for (decltype(a_max) a = 1; a <= a_max && a < b; ++a) {
                    candidate.numerator += current_convergent.numerator;
                    candidate.denominator += current_convergent.denominator;

                    // If a and b are not coprime, skip.
                    if (gcd(a, b) != 1) {
                        continue;
                    }

                    push_all_multiples(candidate);
                }
            }
        };

        while (true) {
            // Currently, cf.current_index() is odd.
            // Obtain the next convergent.
            if (!cf.update()) {
                perfect_approx_possible();

                // The last convergent is of odd index, and we are looking for semiconvergents after
                // the last even convergent. These semiconvergents should have the same error
                // with the last even convergent, and since the last even convergent has been
                // confirmed to have error worse than the threshold, there is nothing left to do.
                return results;
            }

            // Now, cf.current_index() is even.
            if (cf.current_denominator() * x - cf.current_numerator() < error_threshold) {
                // In this case, we should look at semiconvergents.
                if (cf.current_index() == 0) {
                    // In this case, the semiconvergent we are looking for is the 0th convergent.
                    break;
                }
                else {
                    while (true) {
                        ++semiconvergent_coeff;
                        update_semiconvergent();

                        if (semiconvergent.denominator > denominator_upper_bound) {
                            // This means there is no semiconvergent satisfying the condition.
                            return results;
                        }

                        if (semiconvergent_coeff == cf.current_coefficient()) {
                            // Found the first semiconvergent, which is the next convergent.
                            break;
                        }

                        if (semiconvergent.denominator * x - semiconvergent.numerator <
                            error_threshold) {
                            // Found the first semiconvergent.
                            // Iterate until reaching the next convergent.
                            while (true) {
                                subroutine_for_each_semiconvergent();
                                ++semiconvergent_coeff;
                                update_semiconvergent();

                                if (semiconvergent.denominator > denominator_upper_bound) {
                                    return results;
                                }

                                if (semiconvergent_coeff == cf.current_coefficient()) {
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
                break;
            }

            if (cf.current_denominator() > denominator_upper_bound) {
                // In this case, we haven't found any convergent that has better error than the
                // threshold, which means there is no n satisfying nx - floor(nx) <= epsilon.
                return results;
            }

            // Currently, cf.current_index() is even.
            // Obtain the next convergent.
            if (!cf.update()) {
                // In this case, there is no n satisfying nx - floor(nx) <= epsilon
                // other than those making x an integer.
                perfect_approx_possible();
                return results;
            }

            // Now, cf.current_index() is odd.
            if (cf.current_denominator() > denominator_upper_bound) {
                // In this case, we haven't found any convergent that has better error than the
                // threshold, which means there is no n satisfying nx - floor(nx) <= epsilon.
                return results;
            }

            previous_convergent = cf.previous_convergent();
            current_convergent = cf.current_convergent();
        }

        // We have found the first even semiconvergent with a good enough error.
        assert(cf.current_index() % 2 == 0);

        // Now, iterate over all subsequent convergents.
        while (true) {
            if (!cf.update()) {
                perfect_approx_possible();
                break;
            }

            // Now, cf.current_index() is odd.
            previous_convergent = cf.previous_convergent();
            current_convergent = cf.current_convergent();
            semiconvergent_coeff = 0;
            update_semiconvergent();
            subroutine_for_each_semiconvergent();

            if (cf.current_denominator() > denominator_upper_bound) {
                break;
            }

            // Obtain the next even convergent.
            if (!cf.update()) {
                perfect_approx_possible();

                // The last convergent is of odd index, and we are looking for semiconvergents
                // after the last even convergent.
                while (true) {
                    ++semiconvergent_coeff;
                    update_semiconvergent();

                    if (semiconvergent.denominator > denominator_upper_bound) {
                        break;
                    }

                    subroutine_for_each_semiconvergent();
                }
                return results;
            }

            // Iterate over all semiconvergents.
            for (semiconvergent_coeff = 1; semiconvergent_coeff < cf.current_coefficient();
                 ++semiconvergent_coeff) {
                update_semiconvergent();

                if (semiconvergent.denominator > denominator_upper_bound) {
                    return results;
                }

                subroutine_for_each_semiconvergent();
            }
        }

        return results;
    }
}

#endif