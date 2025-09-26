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

#include "big_uint.h"
#include "dragonbox/dragonbox.h"
#include <cassert>
#include <limits>

namespace jkj {
    static constexpr std::size_t log2p1(std::uint64_t x) noexcept {
        // C++20 std::log2p1 is not yet supported
        // return std::log2p1(x);

        std::size_t ret = 0;
        auto inspect = [&x, &ret](int shft) {
            if ((x >> shft) != 0) {
                x >>= shft;
                ret += shft;
            }
        };

        inspect(32);
        inspect(16);
        inspect(8);
        inspect(4);
        inspect(2);
        inspect(1);

        return ret + x;
    }

    std::size_t log2p1(big_uint const& n) noexcept {
        if (n.is_zero()) {
            return 0;
        }
        return (n.elements.size() - 1) * big_uint::element_number_of_bits +
               log2p1(n.elements.back());
    }

    big_uint big_uint::power_of_2(std::size_t k) {
        auto number_of_elmts = k / element_number_of_bits + 1;
        big_uint ret_val;
        ret_val.elements.resize(number_of_elmts, 0);
        ret_val.elements.back() = (element_type(1) << (k % element_number_of_bits));
        return ret_val;
    }

    big_uint big_uint::pow(big_uint base, std::size_t k) {
        big_uint result{1};
        while (true) {
            if (k % 2 != 0) {
                result *= base;
            }
            k /= 2;
            if (k == 0) {
                break;
            }
            base *= base;
        }

        return result;
    }

    std::size_t big_uint::multiply_2_until(big_uint const& n) {
        assert(elements.size() != 0);

        std::size_t number_of_multiplications = 0;

        // Perform left-shift to match the leading-1 position.
        // Perform element-wise shift first.
        assert(elements.size() <= n.elements.size());
        auto element_pos_offset = n.elements.size() - elements.size();
        if (element_pos_offset > 0) {
            number_of_multiplications = element_pos_offset * element_number_of_bits;
            auto old_size = elements.size();
            elements.resize(n.elements.size());

            std::move_backward(std::begin(elements), std::begin(elements) + old_size,
                               std::begin(elements) + elements.size());

            std::fill_n(std::begin(elements), element_pos_offset, 0);
        }
        // And then perform bit-wise shift.
        auto new_bit_pos = log2p1(elements.back());
        auto bit_pos_offset =
            std::ptrdiff_t(log2p1(n.elements.back())) - std::ptrdiff_t(new_bit_pos);
        assert(element_pos_offset > 0 || (element_pos_offset == 0 && bit_pos_offset >= 0));
        number_of_multiplications += bit_pos_offset;
        if (bit_pos_offset > 0) {
            // Left-shfit
            auto shft = std::size_t(bit_pos_offset);
            auto remaining_bits = element_number_of_bits - shft;
            for (auto idx = elements.size() - 1; idx > 0; --idx) {
                auto bits_to_transfer = elements[idx - 1] >> remaining_bits;

                elements[idx] <<= shft;
                elements[idx] |= bits_to_transfer;
            }
            elements[0] <<= shft;
        }
        else if (bit_pos_offset < 0) {
            // Right-shift
            auto shft = std::size_t(-bit_pos_offset);
            auto remaining_bits = element_number_of_bits - shft;
            elements[0] >>= shft;
            for (std::size_t idx = 1; idx < elements.size(); ++idx) {
                auto bits_to_transfer = elements[idx] << remaining_bits;

                elements[idx - 1] |= bits_to_transfer;
                elements[idx] >>= shft;
            }
        }

        // Compare the shifted number with the given number.
        bool is_bigger_than_or_equal_to = true;
        for (auto idx = std::ptrdiff_t(elements.size()) - 1; idx >= 0; --idx) {
            if (elements[idx] > n.elements[idx])
                break;
            else if (elements[idx] < n.elements[idx]) {
                is_bigger_than_or_equal_to = false;
                break;
            }
        }

        // If our number is still less,
        if (!is_bigger_than_or_equal_to) {
            // Shift one more bit.
            ++number_of_multiplications;
            if (new_bit_pos == element_number_of_bits) {
                elements.resize(elements.size() + 1);
            }

            constexpr auto remaining_bits = element_number_of_bits - 1;
            for (auto idx = elements.size() - 1; idx > 0; --idx) {
                elements[idx] <<= 1;
                elements[idx] |= (elements[idx - 1] >> remaining_bits);
            }
            elements[0] <<= 1;
        }

        return number_of_multiplications;
    }

    std::size_t big_uint::multiply_2_while(big_uint const& n) {
        assert(elements.size() != 0);

        std::size_t number_of_multiplications = 0;

        // Perform left-shift to match the leading-1 position.
        // Perform element-wise shift first.
        assert(elements.size() <= n.elements.size());
        auto element_pos_offset = n.elements.size() - elements.size();
        if (element_pos_offset > 0) {
            number_of_multiplications = element_pos_offset * element_number_of_bits;
            auto old_size = elements.size();
            elements.resize(n.elements.size());

            std::move_backward(std::begin(elements), std::begin(elements) + old_size,
                               std::begin(elements) + elements.size());

            std::fill_n(std::begin(elements), element_pos_offset, 0);
        }
        // And then perform bit-wise shift.
        auto new_bit_pos = log2p1(elements.back());
        auto bit_pos_offset =
            std::ptrdiff_t(log2p1(n.elements.back())) - std::ptrdiff_t(new_bit_pos);
        assert(element_pos_offset > 0 || (element_pos_offset == 0 && bit_pos_offset >= 0));
        number_of_multiplications += bit_pos_offset;
        if (bit_pos_offset > 0) {
            // Left-shfit
            auto shft = std::size_t(bit_pos_offset);
            auto remaining_bits = element_number_of_bits - shft;
            for (auto idx = elements.size() - 1; idx > 0; --idx) {
                auto bits_to_transfer = elements[idx - 1] >> remaining_bits;

                elements[idx] <<= shft;
                elements[idx] |= bits_to_transfer;
            }
            elements[0] <<= shft;
        }
        else if (bit_pos_offset < 0) {
            // Right-shift
            auto shft = std::size_t(-bit_pos_offset);
            auto remaining_bits = element_number_of_bits - shft;
            elements[0] >>= shft;
            for (std::size_t idx = 1; idx < elements.size(); ++idx) {
                auto bits_to_transfer = elements[idx] << remaining_bits;

                elements[idx - 1] |= bits_to_transfer;
                elements[idx] >>= shft;
            }
        }

        // Compare the shifted number with the given number.
        bool is_strictly_bigger = true;
        for (auto idx = std::ptrdiff_t(elements.size()) - 1; idx >= 0; --idx) {
            if (elements[idx] > n.elements[idx])
                break;
            else if (elements[idx] <= n.elements[idx]) {
                is_strictly_bigger = false;
                break;
            }
        }

        // If our number is strictly bigger,
        if (is_strictly_bigger) {
            // Shift one bit back.
            assert(number_of_multiplications > 0);
            --number_of_multiplications;

            constexpr auto remaining_bits = element_number_of_bits - 1;
            for (auto idx = 0; idx < elements.size() - 1; ++idx) {
                elements[idx] >>= 1;
                elements[idx] |= (elements[idx + 1] << remaining_bits);
            }
            elements.back() >>= 1;

            if (elements.back() == 0) {
                elements.pop_back();
            }
        }

        return number_of_multiplications;
    }

    void big_uint::multiply_2() & {
        // Shift to left by 1.
        element_type carry = 0;
        for (std::size_t idx = 0; idx < elements.size(); ++idx) {
            auto new_element = (elements[idx] << 1) | carry;

            // Keep the carry.
            carry = (elements[idx] >> (element_number_of_bits - 1));

            elements[idx] = new_element;
        }

        if (carry != 0) {
            elements.push_back(1);
        }
    }

    void big_uint::multiply_5() & {
        if (elements.size() == 0) {
            return;
        }

        element_type upper_2_bits = 0;
        element_type carry = 0;
        for (std::size_t idx = 0; idx < elements.size(); ++idx) {
            auto times_4 = (elements[idx] << 2) | upper_2_bits;
            upper_2_bits = (elements[idx] >> (element_number_of_bits - 2));

            element_type new_carry = 0;
            // Add *this with *this * 4.
            elements[idx] += times_4;
            // If carry happens,
            if (elements[idx] < times_4) {
                new_carry = 1;
                // Add the carry from the previous element.
                elements[idx] += carry;
            }
            // If no carry happens,
            else {
                // Add the carry from the previous element.
                elements[idx] += carry;
                // If carry happens,
                if (elements[idx] < carry) {
                    new_carry = 1;
                }
            }
            carry = new_carry;
        }

        upper_2_bits += carry;
        if (upper_2_bits != 0) {
            elements.push_back(upper_2_bits);
        }
    }

    int big_uint::comparison_common(big_uint const& n) const noexcept {
        if (elements.size() < n.elements.size()) {
            return -1;
        }
        else if (elements.size() > n.elements.size()) {
            return +1;
        }
        else {
            for (auto idx_p1 = elements.size(); idx_p1 > 0; --idx_p1) {
                if (elements[idx_p1 - 1] < n.elements[idx_p1 - 1]) {
                    return -1;
                }
                else if (elements[idx_p1 - 1] > n.elements[idx_p1 - 1]) {
                    return +1;
                }
            }
            return 0;
        }
    }

    big_uint& big_uint::operator+=(big_uint const& n) & {
        std::size_t min_size;
        if (elements.size() >= n.elements.size()) {
            min_size = n.elements.size();
        }
        else {
            min_size = elements.size();
            elements.insert(elements.cend(), n.elements.cbegin() + min_size, n.elements.cend());
        }

        unsigned int carry = 0;
        for (std::size_t idx = 0; idx < min_size; ++idx) {
            auto with_carry = elements[idx] + carry;
            unsigned int first_carry = (with_carry < elements[idx]) ? 1 : 0;

            auto n_element = n.elements[idx];
            elements[idx] = with_carry + n_element;
            carry = first_carry | ((elements[idx] < n_element) ? 1 : 0);
        }

        if (carry != 0) {
            for (std::size_t idx = min_size; idx < elements.size(); ++idx) {
                ++elements[idx];
                if (elements[idx] != 0) {
                    return *this;
                }
            }
            elements.push_back(1);
        }

        return *this;
    }

    big_uint& big_uint::operator+=(element_type n) & {
        if (is_zero()) {
            elements.push_back(n);
            return *this;
        }

        elements[0] += n;

        // If carry happens,
        if (elements[0] < n) {
            // Propagate carry.
            for (std::size_t idx = 1; idx < elements.size(); ++idx) {
                ++elements[idx];
                if (elements[idx] != 0) {
                    return *this;
                }
            }
            elements.push_back(1);
        }
        return *this;
    }

    big_uint& big_uint::operator-=(big_uint const& n) & {
        // Underflow!
        assert(elements.size() >= n.elements.size());

        unsigned int borrow = 0;
        for (std::size_t idx = 0; idx < n.elements.size(); ++idx) {
            auto with_borrow = elements[idx] - borrow;
            unsigned int first_borrow = (with_borrow > elements[idx]) ? 1 : 0;

            auto n_element = n.elements[idx];
            elements[idx] = with_borrow - n_element;
            borrow = first_borrow | ((elements[idx] > with_borrow) ? 1 : 0);
        }

        if (borrow != 0) {
            for (std::size_t idx = n.elements.size(); idx < elements.size(); ++idx) {
                --elements[idx];
                if (elements[idx] != std::numeric_limits<element_type>::max()) {
                    goto remove_leading_zeros;
                }
            }
            // Underflow!
            assert(elements.back() != std::numeric_limits<element_type>::max());
        }

    remove_leading_zeros:
        auto itr = elements.end();
        for (; itr != elements.begin(); --itr) {
            if (*(itr - 1) != 0) {
                break;
            }
        }
        elements.erase(itr, elements.end());

        return *this;
    }

    big_uint& big_uint::operator--() & {
        // Underflow!
        assert(!is_zero());

        for (std::size_t idx = 0; idx < elements.size(); ++idx) {
            --elements[idx];
            if (elements[idx] != std::numeric_limits<element_type>::max()) {
                break;
            }
        }

        // Remove leading zeros
        auto itr = elements.end();
        for (; itr != elements.begin(); --itr) {
            if (*(itr - 1) != 0) {
                break;
            }
        }
        elements.erase(itr, elements.end());

        return *this;
    }

    big_uint& big_uint::operator*=(element_type n) & {
        if (n == 0) {
            elements.clear();
            return *this;
        }

        element_type carry = 0;
        for (std::size_t idx = 0; idx < elements.size(); ++idx) {
            auto mul = jkj::dragonbox::detail::wuint::umul128(elements[idx], n);
            elements[idx] = mul.low() + carry;
            carry = mul.high() + (elements[idx] < mul.low() ? 1 : 0);
        }
        if (carry != 0) {
            elements.push_back(carry);
        }

        return *this;
    }

    big_uint operator*(big_uint const& x, big_uint const& y) {
        if (x.is_zero() || y.is_zero()) {
            return big_uint();
        }

        big_uint result;
        result.elements.resize(x.elements.size() + y.elements.size(), 0);
        decltype(x.elements) temp(x.elements.size());

        for (std::size_t y_idx = 0; y_idx < y.elements.size(); ++y_idx) {
            // Compute y.elements[y_idx] * x and accumulate it into the result
            for (std::size_t x_idx = 0; x_idx < x.elements.size(); ++x_idx) {
                auto mul = jkj::dragonbox::detail::wuint::umul128(x.elements[x_idx], y.elements[y_idx]);

                // Add the first half
                result.elements[x_idx + y_idx] += mul.low();
                unsigned int carry = result.elements[x_idx + y_idx] < mul.low() ? 1 : 0;

                // Add the second half
                auto with_carry = mul.high() + carry;
                carry = with_carry < mul.high() ? 1 : 0;
                result.elements[x_idx + y_idx + 1] += with_carry;

                // If there is carry,
                if (result.elements[x_idx + y_idx + 1] < with_carry) {
                    // Propagate.
                    assert(x_idx + y_idx + 2 < result.elements.size());
                    for (auto idx = x_idx + y_idx + 2; idx < result.elements.size(); ++idx) {
                        ++result.elements[idx];
                        if (result.elements[idx] != 0) {
                            break;
                        }
                    }
                }
            }
        }

        // Remove the last element if it is zero.
        if (result.elements.back() == 0) {
            result.elements.pop_back();
        }

        return result;
    }

    big_uint big_uint::long_division(big_uint const& n) {
        assert(!n.is_zero());

        if (this == &n) {
            elements.clear();
            return big_uint(1);
        }

        big_uint quotient;
        big_uint n_shifted;

        while (true) {
            // Break if *this is smaller than n.
            if (elements.size() < n.elements.size()) {
                break;
            }

            // Shift n element-wise first.
            n_shifted.elements.resize(elements.size());
            std::fill_n(n_shifted.elements.begin(), (elements.size() - n.elements.size()), 0);
            std::copy(n.elements.cbegin(), n.elements.cend(),
                      n_shifted.elements.begin() + (elements.size() - n.elements.size()));

            std::size_t total_shift_amount =
                element_number_of_bits * (elements.size() - n.elements.size());

            // Shift n bit-wise to match the leading 1 position.
            auto leading_1_pos = log2p1(elements.back());
            auto n_leading_1_pos = log2p1(n_shifted.elements.back());

            if (leading_1_pos > n_leading_1_pos) {
                // Shift left.
                auto shift_amount = leading_1_pos - n_leading_1_pos;
                total_shift_amount += shift_amount;

                element_type carry = 0;
                for (std::size_t idx = elements.size() - n.elements.size(); idx < elements.size();
                     ++idx) {
                    auto new_element = (n_shifted.elements[idx] << shift_amount) | carry;
                    carry = (n_shifted.elements[idx] >> (element_number_of_bits - shift_amount));
                    n_shifted.elements[idx] = new_element;
                }
                assert(carry == 0);
            }
            else if (leading_1_pos < n_leading_1_pos) {
                // Break if *this is smaller than n.
                if (total_shift_amount == 0) {
                    break;
                }

                // Shift right.
                auto shift_amount = n_leading_1_pos - leading_1_pos;
                assert(total_shift_amount > shift_amount);
                total_shift_amount -= shift_amount;
                for (std::size_t idx = elements.size() - n.elements.size(); idx < elements.size();
                     ++idx) {
                    n_shifted.elements[idx - 1] |=
                        (n_shifted.elements[idx] << (element_number_of_bits - shift_amount));
                    n_shifted.elements[idx] >>= shift_amount;
                }
            }

            // Check if n_shifted is bigger than *this; if that's the case, shift one bit to right.
            if (n_shifted > *this) {
                // If we actually didn't shift anything,
                if (total_shift_amount == 0) {
                    // *this is smaller than n.
                    break;
                }

                // Shift right.
                --total_shift_amount;
                n_shifted.elements[0] >>= 1;
                for (std::size_t idx = 1; idx < elements.size(); ++idx) {
                    n_shifted.elements[idx - 1] |=
                        (n_shifted.elements[idx] << (element_number_of_bits - 1));
                    n_shifted.elements[idx] >>= 1;
                }
            }

            // Subtract n_shifted from *this.
            *this -= n_shifted;

            // Update the quotient.
            auto quotient_element_idx = total_shift_amount / element_number_of_bits;
            auto quotient_bit_idx = total_shift_amount % element_number_of_bits;
            if (quotient.elements.size() <= quotient_element_idx) {
                quotient.elements.resize(quotient_element_idx + 1);
            }
            quotient.elements[quotient_element_idx] |= (element_type(1) << quotient_bit_idx);
        }

        return quotient;
    }

    std::vector<std::uint64_t> big_uint::to_decimal() const {
        std::vector<std::uint64_t> ret;
        auto n = *this;
        auto divisor = big_uint{1000'0000'0000'0000'0000ull};

        do {
            auto quotient = n.long_division(divisor);
            ret.push_back(n.is_zero() ? 0 : n[0]);
            n = quotient;
        } while (!n.is_zero());

        return ret;
    }
}