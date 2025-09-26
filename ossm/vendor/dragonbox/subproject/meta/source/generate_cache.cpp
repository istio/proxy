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

#include "big_uint.h"
#include "continued_fractions.h"
#include "dragonbox/dragonbox.h"
#include <algorithm>
#include <utility>
#include <stdexcept>
#include <vector>

template <class FormatTraits>
auto generate_cache(std::size_t cache_bits) {
    using impl = jkj::dragonbox::detail::impl<FormatTraits>;

    std::vector<jkj::big_uint> results;
    jkj::unsigned_rational<jkj::big_uint> target_number;
    for (int k = impl::min_k; k <= impl::max_k; ++k) {
        // (2f_c +- 1) * 2^beta * (2^(k - e_k - Q) * 5^k)
        // e_k = floor(k log2(10)) - Q + 1, so
        // k - e_k - Q = k - floor(k log2(10)) - 1.
        int exp_2 = k - jkj::dragonbox::detail::log::floor_log2_pow10(k) - 1;

        target_number.numerator = 1;
        target_number.denominator = 1;
        if (k >= 0) {
            target_number.numerator = jkj::big_uint::pow(5, k);
        }
        else {
            target_number.denominator = jkj::big_uint::pow(5, -k);
        }
        if (exp_2 >= 0) {
            target_number.numerator *= jkj::big_uint::power_of_2(exp_2);
        }
        else {
            target_number.denominator *= jkj::big_uint::power_of_2(-exp_2);
        }

        auto div_res = div(jkj::big_uint::power_of_2(cache_bits) * target_number.numerator,
                           target_number.denominator);
        auto m = std::move(div_res.quot);
        if (!div_res.rem.is_zero()) {
            m += 1;
        }

        // Recheck that m is in the correct range.
        if (m < jkj::big_uint::power_of_2(cache_bits - 1) ||
            m >= jkj::big_uint::power_of_2(cache_bits)) {
            throw std::logic_error{"Generated cache entry is not in the correct range"};
        }

        results.push_back(std::move(m));
    }

    return results;
}

#include <fstream>
#include <iomanip>
#include <iostream>

int main() {
    std::cout << "[Generating cache...]\n";

    auto write_file = [](std::ofstream& out, std::size_t cache_bits, auto type_tag,
                         auto&& ieee_754_type_name_string, auto&& element_printer) {
        using impl_type = jkj::dragonbox::detail::impl<decltype(type_tag)>;
        auto const cache_array = generate_cache<decltype(type_tag)>(cache_bits);

        out << "static constexpr int min_k = " << std::dec << impl_type::min_k << ";\n";
        out << "static constexpr int max_k = " << std::dec << impl_type::max_k << ";\n";
        out << "static constexpr detail::array<cache_entry_type, detail::stdr::size_t(max_k - min_k + "
               "1)> cache JKJ_STATIC_DATA_SECTION = { {";
        for (int k = impl_type::min_k; k < impl_type::max_k; ++k) {
            auto idx = std::size_t(k - impl_type::min_k);
            out << "\n\t";
            element_printer(out, cache_array[idx]);
            out << ",";
        }
        out << "\n\t";
        element_printer(out, cache_array.back());
        out << "\n} };";
    };

    std::ofstream out;

    try {
        out.open("results/binary32_generated_cache.txt");
        write_file(out, 64,
                   jkj::dragonbox::ieee754_binary_traits<jkj::dragonbox::ieee754_binary32,
                                                         std::uint_least32_t>{},
                   "binary32", [](std::ofstream& out, jkj::big_uint const& value) {
                       out << "UINT64_C(0x" << std::hex << std::setw(16) << std::setfill('0')
                           << value[0] << ")";
                   });
        out.close();

        out.open("results/binary64_generated_cache.txt");
        write_file(out, 128,
                   jkj::dragonbox::ieee754_binary_traits<jkj::dragonbox::ieee754_binary64,
                                                         std::uint_least64_t>{},
                   "binary64", [](std::ofstream& out, jkj::big_uint const& value) {
                       out << "{UINT64_C(0x" << std::hex << std::setw(16) << std::setfill('0')
                           << value[1] << "), UINT64_C(0x" << std::hex << std::setw(16)
                           << std::setfill('0') << value[0] << ")}";
                   });
        out.close();
    }
    catch (std::logic_error const& ex) {
        std::cout << ex.what() << "\n";
        return -1;
    }

    std::cout << "Done.\n\n\n";
}
