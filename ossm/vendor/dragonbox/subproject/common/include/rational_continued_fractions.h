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

#ifndef JKJ_HEADER_RATIONAL_CONTINUED_FRACTIONS
#define JKJ_HEADER_RATIONAL_CONTINUED_FRACTIONS

#include "continued_fractions.h"
#include <cassert>
#include <cstdlib>

namespace jkj {
    template <class UInt>
    class rational_continued_fractions
        : public continued_fractions<rational_continued_fractions<UInt>, UInt> {
        using crtp_base = continued_fractions<rational_continued_fractions<UInt>, UInt>;
        friend crtp_base;

        UInt prev_error_;
        UInt curr_error_;

        UInt compute_next_coefficient() {
            using std::div;
            auto div_result = div(prev_error_, curr_error_);
            prev_error_ = static_cast<UInt&&>(curr_error_);
            curr_error_ = static_cast<UInt&&>(div_result.rem);

            if (curr_error_ == 0) {
                crtp_base::set_terminate_flag();
            }

            return static_cast<UInt&&>(div_result.quot);
        }

    public:
        rational_continued_fractions(unsigned_rational<UInt> r)
            : prev_error_{static_cast<UInt&&>(r.numerator)}, curr_error_{static_cast<UInt&&>(
                                                                 r.denominator)} {
            assert(curr_error_ != 0);
        }
    };
}

#endif