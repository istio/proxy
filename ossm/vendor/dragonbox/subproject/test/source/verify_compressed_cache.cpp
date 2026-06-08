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

#include "dragonbox/dragonbox.h"
#include "big_uint.h"
#include "rational_continued_fractions.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

// We are trying to verify that an appropriate right-shift of phi_k * 5^a plus one
// can be used instead of phi_(a+k). (Here, phi_k and phi_(a+k) are supposed to be the "tilde" ones;
// tilde is omitted for simplicity.) Since phi_k is defined in terms of ceiling, what we get from
// phi_k * 5^a will be phi_(a+k) + (error) for some nonnegative (error).
//
// For correct multiplication, the margin for binary32 is at least
// 2^64 * 5091154818982829 / 12349290596248284087255008291061760 = 7.60...,
// so we are safe if the error is up to 7.
// The margin for binary64 is at least
// 2^128 * 723173431431821867556830303887 /
// 18550103527668669801949286474444582643081334006759269899933694558208
// = 13.26..., so we are safe if the error is up to 13.
//
// For correct integer checks, the case b > n_max is fine because the only condition on the
// recovered cache is a lower bound which must be already true for phi_(a+k).
// For the case b <= n_max, we only need to check the upper bound
// (recovered_cache) < 2^(Q-beta) * a/b + 2^(q-beta)/(floor(nmax/b) * b),
// so we check it manually for each e.

template <class CacheEntryType>
struct recovered_cache_t {
    CacheEntryType value;
    bool success;
};

template <class FormatTraits, class GetCache, class ConvertToBigUInt>
bool verify_compressed_cache(GetCache&& get_cache, ConvertToBigUInt&& convert_to_big_uint,
                             std::size_t max_diff_for_multiplication) {
    using format = typename FormatTraits::format;
    using cache_holder_type = jkj::dragonbox::compressed_cache_holder<format>;
    using impl = jkj::dragonbox::detail::impl<FormatTraits>;

    jkj::unsigned_rational<jkj::big_uint> unit;
    auto n_max = jkj::big_uint::power_of_2(format::significand_bits + 2);
    for (int e = format::min_exponent - format::significand_bits;
         e <= format::max_exponent - format::significand_bits; ++e) {
        int const k = impl::kappa - jkj::dragonbox::detail::log::floor_log10_pow2(e);

        auto const real_cache = jkj::dragonbox::policy::cache::full.get_cache<format, int>(k);

        auto const recovered_cache = get_cache(k);
        if (!recovered_cache.success) {
            std::cout << " (e = " << e << ")\n";
            return false;
        }

        auto const rc = convert_to_big_uint(recovered_cache.value);
        auto const diff = rc - convert_to_big_uint(real_cache);
        if (diff != 0) {
            if (diff > max_diff_for_multiplication) {
                std::cout << "Multiplication is no longer valid. (e = " << e << ")\n";
                return false;
            }

            // For the case b <= n_max, integer check might be no longer valid.
            int const beta = e + jkj::dragonbox::detail::log::floor_log2_pow10(k);

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

            if (unit.denominator <= n_max) {
                // Check (recovered_cache) < 2^(Q-beta) * a/b + 2^(q-beta)/(floor(nmax/b) * b),
                // or equivalently,
                // b * (recovered_cache) - 2^(Q-beta) * a < 2^(q-beta) / floor(nmax/b).
                auto const left_hand_side =
                    unit.denominator * rc -
                    jkj::big_uint::power_of_2(cache_holder_type::cache_bits - beta) * unit.numerator;

                if (left_hand_side * (n_max / unit.denominator) >=
                    jkj::big_uint::power_of_2(FormatTraits::carrier_bits - beta)) {
                    std::cout << "Integer check is no longer valid. (e = " << e << ")\n";

                    // This exceptional case is carefully examined, so okay.
                    if (std::is_same<format, jkj::dragonbox::ieee754_binary32>::value && e == -10) {
                        // The exceptional case only occurs when n is exactly n_max.
                        if (left_hand_side * ((n_max - 1) / unit.denominator) >=
                            jkj::big_uint::power_of_2(FormatTraits::carrier_bits - beta)) {
                            return false;
                        }
                        std::cout << "    This case has been carefully addressed.\n\n";
                    }
                    else {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

int main() {
    bool success = true;

    std::cout << "[Verifying compressed cache for binary32...]\n";
    {
        using cache_holder_type =
            jkj::dragonbox::compressed_cache_holder<jkj::dragonbox::ieee754_binary32>;

        if (verify_compressed_cache<jkj::dragonbox::ieee754_binary_traits<
                jkj::dragonbox::ieee754_binary32, std::uint_least32_t>>(
                [](int k) {
                    return recovered_cache_t<cache_holder_type::cache_entry_type>{
                        cache_holder_type::get_cache<int>(k), true};
                },
                [](cache_holder_type::cache_entry_type value) { return jkj::big_uint{value}; }, 7)) {
            std::cout << "Verification succeeded. No error detected.\n\n";
        }
        else {
            std::cout << "\n";
            success = false;
        }
    }

    std::cout << "[Verifying compressed cache for binary64...]\n";
    {
        using cache_holder_type =
            jkj::dragonbox::compressed_cache_holder<jkj::dragonbox::ieee754_binary64>;

        if (verify_compressed_cache<jkj::dragonbox::ieee754_binary_traits<
                jkj::dragonbox::ieee754_binary64, std::uint_least64_t>>(
                [](int k) {
                    // Compute the base index.
                    auto const cache_index = int(std::uint_least32_t(k - cache_holder_type::min_k) /
                                                 cache_holder_type::compression_ratio);
                    auto const kb =
                        cache_index * cache_holder_type::compression_ratio + cache_holder_type::min_k;
                    auto const offset = k - kb;

                    // Get the base cache.
                    auto const base_cache = cache_holder_type::cache[cache_index];

                    if (offset != 0) {
                        // Obtain the corresponding power of 5.
                        auto const pow5 = cache_holder_type::pow5_table[offset];

                        // Compute the required amount of bit-shifts.
                        using jkj::dragonbox::detail::log::floor_log2_pow10;
                        auto const alpha = floor_log2_pow10(k) - floor_log2_pow10(kb) - offset;
                        assert(alpha > 0 && alpha < 64);

                        // Try to recover the real cache.
                        using jkj::dragonbox::detail::wuint::umul128;
                        auto recovered_cache = umul128(base_cache.high(), pow5);
                        auto const middle_low = umul128(base_cache.low(), pow5);

                        recovered_cache += middle_low.high();

                        auto const high_to_middle = std::uint_least64_t(
                            (recovered_cache.high() << (64 - alpha)) & UINT64_C(0xffffffffffffffff));
                        auto const middle_to_low = std::uint_least64_t(
                            (recovered_cache.low() << (64 - alpha)) & UINT64_C(0xffffffffffffffff));

                        recovered_cache = {(recovered_cache.low() >> alpha) | high_to_middle,
                                           ((middle_low.low() >> alpha) | middle_to_low)};
                        recovered_cache = {recovered_cache.high(),
                                           std::uint_least64_t(recovered_cache.low() + 1)};

                        if (recovered_cache.low() == 0) {
                            std::cout
                                << "Overflow detected - taking the ceil requires addition-with-carry";
                            return recovered_cache_t<cache_holder_type::cache_entry_type>{
                                recovered_cache, false};
                        }
                        else {
                            return recovered_cache_t<cache_holder_type::cache_entry_type>{
                                recovered_cache, true};
                        }
                    }
                    else {
                        return recovered_cache_t<cache_holder_type::cache_entry_type>{base_cache, true};
                    }
                },
                [](cache_holder_type::cache_entry_type value) {
                    return jkj::big_uint{value.low(), value.high()};
                },
                13)) {
            std::cout << "Verification succeeded. No error detected.\n\n";
        }
        else {
            std::cout << "\n";
            success = false;
        }
    }

    std::cout << "Done.\n\n\n";
    return success ? 0 : -1;
}
