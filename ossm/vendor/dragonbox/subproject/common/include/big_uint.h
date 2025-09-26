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

#ifndef JKJ_HEADER_BIG_UINT
#define JKJ_HEADER_BIG_UINT

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <vector>

namespace jkj {
    class big_uint {
    public:
        using element_type = std::uint64_t;
        static constexpr std::size_t element_number_of_bits = 64;

    private:
        // Least significant element first.
        std::vector<element_type> elements;

    public:
        // elements is empty if and only if it represents 0.
        big_uint() = default;
        big_uint(element_type n) {
            if (n != 0) {
                elements.push_back(n);
            }
        }

        // Remove leading zeros.
        explicit big_uint(std::initializer_list<element_type> list)
            : big_uint(list.begin(), list.end()) {}

        // Remove leading zeros.
        template <class Iter, class = typename std::iterator_traits<Iter>::iterator_category>
        big_uint(Iter first, Iter last) : elements(first, last) {
            auto effective_size = elements.size();
            while (effective_size > 0) {
                if (elements[effective_size - 1] != 0) {
                    break;
                }
                --effective_size;
            }
            elements.resize(effective_size);
        }

        element_type operator[](std::size_t idx) const { return elements[idx]; }

        bool is_zero() const noexcept { return elements.empty(); }
        bool is_even() const noexcept {
            if (elements.empty()) {
                return true;
            }
            else {
                return elements[0] % 2 == 0;
            }
        }

        friend std::size_t log2p1(big_uint const& n) noexcept;

        static big_uint power_of_2(std::size_t k);
        static big_uint pow(big_uint base, std::size_t k);

        // Repeat multiplying 2 until the number becomes bigger than or equal to the given number.
        // Returns the number of multiplications, which is ceil(log2(n/*this)).
        // Precondition: *this is not zero and n should be bigger than or equal to the current
        // number. Note that this function need not require &n != this.
        std::size_t multiply_2_until(big_uint const& n);

        // Repeat multiplying 2 while the number becomes less than or equal to the given number.
        // Returns the number of multiplications, which is floor(log2(n/*this)).
        // Precondition: *this is not zero and n should be bigger than or equal to the current
        // number. Note that this function need not require &n != this.
        std::size_t multiply_2_while(big_uint const& n);

        void multiply_2() &;
        void multiply_5() &;

        bool operator==(big_uint const& n) const noexcept { return elements == n.elements; }
        bool operator!=(big_uint const& n) const noexcept { return elements != n.elements; }
        bool operator==(element_type n) const noexcept {
            return (elements.size() == 0 && n == 0) || (elements.size() == 1 && elements[0] == n);
        }
        bool operator!=(element_type n) const noexcept {
            return (elements.size() == 0 && n != 0) || (elements.size() == 1 && elements[0] != n) ||
                   (elements.size() > 1);
        }

    private:
        int comparison_common(big_uint const& n) const noexcept;

    public:
        bool operator<(big_uint const& n) const noexcept { return comparison_common(n) < 0; }
        bool operator<=(big_uint const& n) const noexcept { return comparison_common(n) <= 0; }
        bool operator>(big_uint const& n) const noexcept { return comparison_common(n) > 0; }
        bool operator>=(big_uint const& n) const noexcept { return comparison_common(n) >= 0; }

        bool operator<(element_type n) const noexcept {
            return (elements.size() == 0 && n != 0) || (elements.size() == 1 && elements[0] < n);
        }
        bool operator<=(element_type n) const noexcept {
            return (elements.size() == 0 && n == 0) || (elements.size() == 1 && elements[0] <= n);
        }
        bool operator>(element_type n) const noexcept {
            return (elements.size() == 1 && elements[0] > n) || (elements.size() > 1);
        }
        bool operator>=(element_type n) const noexcept {
            return (elements.size() == 0 && n == 0) || (elements.size() == 1 && elements[0] >= n) ||
                   (elements.size() > 1);
        }

        big_uint& operator+=(big_uint const& n) &;
        big_uint& operator+=(element_type n) &;
        template <class T>
        big_uint operator+(T const& n) const {
            auto r = *this;
            return r += n;
        }
        friend big_uint operator+(element_type n, big_uint const& m) { return m + n; }
        big_uint& operator++() & {
            *this += 1;
            return *this;
        }
        big_uint operator++(int) & {
            auto temp = *this;
            *this += 1;
            return temp;
        }

        // Precondition: n should be strictly smaller than or equal to the current number
        big_uint& operator-=(big_uint const& n) &;
        big_uint operator-(big_uint const& n) const {
            auto r = *this;
            return r -= n;
        }

        // Precondition: *this should be nonzero
        big_uint& operator--() &;

        big_uint& operator*=(element_type n) &;
        big_uint operator*(element_type n) {
            auto r = *this;
            return r *= n;
        }
        friend big_uint operator*(big_uint const& x, big_uint const& y);
        big_uint& operator*=(big_uint const& y) & {
            auto result = *this * y;
            *this = result;
            return *this;
        }

        // Perform long division
        // *this becomes the remainder, returns the quotient
        // Precondition: n != 0
        big_uint long_division(big_uint const& n);

        big_uint operator/(big_uint const& n) const {
            auto temp = *this;
            return temp.long_division(n);
        }
        big_uint operator/(element_type n) const {
            auto temp = *this;
            return temp.long_division(n);
        }
        big_uint operator%(big_uint const& n) const {
            auto temp = *this;
            temp.long_division(n);
            return temp;
        }
        big_uint operator%(element_type n) const {
            auto temp = *this;
            temp.long_division(n);
            return temp;
        }

        // Convert the number into decimal, and returns the
        // array of (at most) 19 digits.
        std::vector<std::uint64_t> to_decimal() const;
    };

    std::size_t log2p1(big_uint const& n) noexcept;
    big_uint operator*(big_uint const& x, big_uint const& y);

    struct big_uint_div_t {
        big_uint quot;
        big_uint rem;
    };
    inline big_uint_div_t div(big_uint const& x, big_uint const& y) {
        big_uint_div_t ret;
        ret.rem = x;
        ret.quot = ret.rem.long_division(y);
        return ret;
    }
    inline big_uint_div_t div(big_uint&& x, big_uint const& y) {
        big_uint_div_t ret;
        ret.rem = static_cast<big_uint&&>(x);
        ret.quot = ret.rem.long_division(y);
        return ret;
    }
}

#endif