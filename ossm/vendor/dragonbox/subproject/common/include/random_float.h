// Copyright 2020-2021 Junekey Jeon
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

#ifndef JKJ_HEADER_RANDOM_FLOAT
#define JKJ_HEADER_RANDOM_FLOAT

#include "dragonbox/dragonbox.h"
#include <algorithm>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// For correct seeding
class repeating_seed_seq {
public:
    using result_type = std::uint32_t;

    repeating_seed_seq() : stored_values{0} {}
    template <class InputIterator>
    repeating_seed_seq(InputIterator first, InputIterator last) : stored_values(first, last) {}
    template <class T>
    repeating_seed_seq(std::initializer_list<T> list) : stored_values(list) {}

    repeating_seed_seq(std::random_device&& rd, std::size_t count) {
        stored_values.resize(count);
        for (auto& elem : stored_values)
            elem = rd();
    }

    template <class RandomAccessIterator>
    void generate(RandomAccessIterator first, RandomAccessIterator last) {
        auto count = last - first;
        auto q = count / stored_values.size();
        for (std::size_t i = 0; i < q; ++i) {
            std::copy_n(stored_values.cbegin(), stored_values.size(), first);
            first += stored_values.size();
        }
        count -= q * stored_values.size();
        std::copy_n(stored_values.cbegin(), count, first);
    }

    std::size_t size() const noexcept { return stored_values.size(); }

    template <class OutputIterator>
    void param(OutputIterator first) const {
        std::copy(stored_values.begin(), stored_values.end(), first);
    }

private:
    std::vector<std::uint32_t> stored_values;
};

inline std::mt19937_64 generate_correctly_seeded_mt19937_64() {
    repeating_seed_seq seed_seq{std::random_device{}, std::mt19937_64::state_size *
                                                          std::mt19937_64::word_size /
                                                          (sizeof(std::uint32_t) * 8)};
    return std::mt19937_64{seed_seq};
}

template <class Float, class RandGen>
Float uniformly_randomly_generate_finite_float(RandGen& rg) {
    using default_float_bit_carrier_conversion_traits =
        jkj::dragonbox::default_float_bit_carrier_conversion_traits<Float>;
    using format = typename default_float_bit_carrier_conversion_traits::format;
    using carrier_uint = typename default_float_bit_carrier_conversion_traits::carrier_uint;
    using format_traits = jkj::dragonbox::ieee754_binary_traits<format, carrier_uint>;
    using uniform_distribution = std::uniform_int_distribution<carrier_uint>;

    // Generate sign bit
    auto sign_bit = uniform_distribution{0, 1}(rg);

    // Generate exponent bits
    auto exponent_bits = uniform_distribution{0, (carrier_uint(1) << format::exponent_bits) - 2}(rg);

    // Generate significand bits
    auto significand_bits =
        uniform_distribution{0, (carrier_uint(1) << format_traits::significand_bits) - 1}(rg);

    auto bit_representation = (sign_bit << (format_traits::carrier_bits - 1)) |
                              (exponent_bits << (format_traits::significand_bits)) | significand_bits;

    return default_float_bit_carrier_conversion_traits::carrier_to_float(bit_representation);
}

template <class Float, class RandGen>
Float uniformly_randomly_generate_general_float(RandGen& rg) {
    using default_float_bit_carrier_conversion_traits =
        jkj::dragonbox::default_float_bit_carrier_conversion_traits<Float>;
    using carrier_uint = typename default_float_bit_carrier_conversion_traits::carrier_uint;
    using uniform_distribution = std::uniform_int_distribution<carrier_uint>;

    // Generate sign bit
    auto bit_representation = uniform_distribution{0, std::numeric_limits<carrier_uint>::max()}(rg);
    return default_float_bit_carrier_conversion_traits::carrier_to_float(bit_representation);
}

template <class Float>
struct std_string_to_float;

template <>
struct std_string_to_float<float> {
    float operator()(std::string const& str) const { return std::stof(str); }
};

template <>
struct std_string_to_float<double> {
    double operator()(std::string const& str) const { return std::stod(str); }
};

// This function tries to uniformly randomly generate a float number with the
// given number of decimal digits, and the end-result is not perfectly bias-free.
// However, I don't think there is an easy way to do it correctly.
template <class Float, class RandGen>
Float randomly_generate_float_with_given_digits(unsigned int digits, RandGen& rg) {
    using conversion_traits = jkj::dragonbox::default_float_bit_carrier_conversion_traits<Float>;
    using carrier_uint = typename conversion_traits::carrier_uint;
    using signed_int_t = std::make_signed_t<carrier_uint>;

    assert(digits >= 1);
    assert(digits <= conversion_traits::format::decimal_significand_digits);

    // Generate sign uniformly randomly
    signed_int_t sign = std::uniform_int_distribution<signed_int_t>{0, 1}(rg) == 0 ? 1 : -1;


    // Try to generate significand uniformly randomly
    Float result;
    signed_int_t from = 0, to = 9;
    if (digits > 1) {
        from = 1;
        for (unsigned int e = 1; e < digits - 1; ++e) {
            from *= 10;
        }
        to = from * 10 - 1;
    }

    while (true) {
        auto significand = std::uniform_int_distribution<signed_int_t>{from, to}(rg);
        if (digits > 1) {
            significand *= 10;
            significand += std::uniform_int_distribution<signed_int_t>{1, 9}(rg);
        }

        // Generate exponent uniformly randomly
        auto exp = std::uniform_int_distribution<int>{
            std::numeric_limits<Float>::min_exponent10 - (int(digits) - 1),
            std::numeric_limits<Float>::max_exponent10 - (int(digits) - 1)}(rg);

        // Cook up
        auto str = std::to_string(sign * significand) + 'e' + std::to_string(exp);
        try {
            result = std_string_to_float<Float>{}(str);
        }
        catch (...) {
            continue;
        }

        if (!std::isfinite(result)) {
            continue;
        }

        // Discard if a shorter representation exists
        // We don't need to care about sign and correct rounding here
        if (from != 0) {
            auto roundtrip = jkj::dragonbox::to_decimal(
                result, jkj::dragonbox::policy::sign::ignore,
                jkj::dragonbox::policy::decimal_to_binary_rounding::nearest_to_even,
                jkj::dragonbox::policy::binary_to_decimal_rounding::do_not_care);
            if (roundtrip.significand <= carrier_uint(from * 10)) {
                continue;
            }
        }
        break;
    }

    return result;
}

#endif
