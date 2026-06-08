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

#ifndef JKJ_HEADER_CONTINUED_FRACTIONS
#define JKJ_HEADER_CONTINUED_FRACTIONS

namespace jkj {
    // Continued fractions calculator for positive numbers.

    template <class UInt>
    struct unsigned_rational {
        UInt numerator = 0;
        UInt denominator = 0;

        unsigned_rational() = default;
        unsigned_rational(UInt const& numerator) : numerator{numerator}, denominator{1} {}
        unsigned_rational(UInt&& numerator)
            : numerator{static_cast<UInt&&>(numerator)}, denominator{1} {}
        unsigned_rational(UInt const& numerator, UInt const& denominator)
            : numerator{numerator}, denominator{denominator} {}
        unsigned_rational(UInt&& numerator, UInt const& denominator)
            : numerator{static_cast<UInt&&>(numerator)}, denominator{denominator} {}
        unsigned_rational(UInt const& numerator, UInt&& denominator)
            : numerator{numerator}, denominator{static_cast<UInt&&>(denominator)} {}
        unsigned_rational(UInt&& numerator, UInt&& denominator)
            : numerator{static_cast<UInt&&>(numerator)}, denominator{
                                                             static_cast<UInt&&>(denominator)} {}

        friend bool operator<(unsigned_rational const& x, unsigned_rational const& y) {
            return x.numerator * y.denominator < y.numerator * x.denominator;
        }
        friend bool operator<=(unsigned_rational const& x, unsigned_rational const& y) {
            return x.numerator * y.denominator <= y.numerator * x.denominator;
        }
        friend bool operator>(unsigned_rational const& x, unsigned_rational const& y) {
            return x.numerator * y.denominator > y.numerator * x.denominator;
        }
        friend bool operator>=(unsigned_rational const& x, unsigned_rational const& y) {
            return x.numerator * y.denominator >= y.numerator * x.denominator;
        }
        friend bool operator==(unsigned_rational const& x, unsigned_rational const& y) {
            return x.numerator * y.denominator == y.numerator * x.denominator;
        }
        friend bool operator!=(unsigned_rational const& x, unsigned_rational const& y) {
            return x.numerator * y.denominator != y.numerator * x.denominator;
        }

        // Performs no reduction.
        friend unsigned_rational operator+(unsigned_rational const& x, unsigned_rational const& y) {
            return {x.numerator * y.denominator + y.numerator * x.denominator,
                    x.denominator * y.denominator};
        }
        // Performs no reduction.
        unsigned_rational& operator+=(unsigned_rational const& y) & {
            numerator *= y.denominator;
            numerator += y.numerator * denominator;
            denominator *= y.denominator;
            return *this;
        }
        // Performs no reduction.
        friend unsigned_rational operator-(unsigned_rational const& x, unsigned_rational const& y) {
            return {x.numerator * y.denominator - y.numerator * x.denominator,
                    x.denominator * y.denominator};
        }
        // Performs no reduction.
        unsigned_rational& operator-=(unsigned_rational const& y) & {
            numerator *= y.denominator;
            numerator -= y.numerator * denominator;
            denominator *= y.denominator;
            return *this;
        }
        // Performs no reduction.
        friend unsigned_rational operator*(unsigned_rational const& x, unsigned_rational const& y) {
            return {x.numerator * y.numerator, x.denominator * y.denominator};
        }
        // Performs no reduction.
        unsigned_rational& operator*=(unsigned_rational const& y) & {
            numerator *= y.numerator;
            denominator *= y.denominator;
            return *this;
        }
        // Performs no reduction.
        friend unsigned_rational operator/(unsigned_rational const& x, unsigned_rational const& y) {
            return {x.numerator * y.denominator, x.denominator * y.numerator};
        }
        // Performs no reduction.
        unsigned_rational& operator/=(unsigned_rational const& y) & {
            numerator *= y.denominator;
            denominator *= y.numerator;
            return *this;
        }
    };

    template <class Impl, class UInt>
    class continued_fractions {
        // The (-1)st coefficient is assumed to be 0.
        UInt current_coefficient_{0};
        unsigned_rational<UInt> current_convergent_{1, 0};
        unsigned_rational<UInt> previous_convergent_{0, 1};
        int current_index_ = -1;
        bool terminated_ = false;

    protected:
        void set_terminate_flag() noexcept { terminated_ = true; }

    public:
        using uint_type = UInt;

        int current_index() const noexcept { return current_index_; }

        UInt const& current_coefficient() const noexcept { return current_coefficient_; }

        unsigned_rational<UInt> const& current_convergent() const noexcept {
            return current_convergent_;
        }

        UInt const& current_numerator() const noexcept { return current_convergent().numerator; }

        UInt const& current_denominator() const noexcept {
            return current_convergent().denominator;
        }

        unsigned_rational<UInt> const& previous_convergent() const noexcept {
            return previous_convergent_;
        }

        UInt const& previous_numerator() const noexcept { return previous_convergent().numerator; }

        UInt const& previous_denominator() const noexcept {
            return previous_convergent().denominator;
        }

        bool is_terminated() const noexcept { return terminated_; }

        // Do nothing if the procedure is terminated.
        // Returns true if the update is done,
        // and returns false if the procedure is already terminated.
        bool update() {
            if (!is_terminated()) {
                unsigned_rational<UInt> new_output;
                current_coefficient_ = static_cast<Impl&>(*this).compute_next_coefficient();

                unsigned_rational<UInt> new_convergent{
                    previous_numerator() + current_coefficient_ * current_numerator(),
                    previous_denominator() + current_coefficient_ * current_denominator()};
                previous_convergent_ = static_cast<unsigned_rational<UInt>&&>(current_convergent_);
                current_convergent_ = static_cast<unsigned_rational<UInt>&&>(new_convergent);

                ++current_index_;

                return true;
            }
            else {
                return false;
            }
        }
    };
}

#endif