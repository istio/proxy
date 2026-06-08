// Copyright 2020 Alexander Bolz
//
// Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#include "schubfach_32.h"

//--------------------------------------------------------------------------------------------------
// This file contains an implementation of the Schubfach algorithm as described in
//
// [1] Raffaello Giulietti, "The Schubfach way to render doubles",
//     https://drive.google.com/open?id=1luHhyQF9zKlM8yJ1nebU0OgVYhfC6CBN
//--------------------------------------------------------------------------------------------------

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#if _MSC_VER
#include <intrin.h>
#endif

#ifndef SF_ASSERT
#define SF_ASSERT(X) assert(X)
#endif

//==================================================================================================
//
//==================================================================================================

template <typename Dest, typename Source>
static inline Dest ReinterpretBits(Source source)
{
    static_assert(sizeof(Dest) == sizeof(Source), "size mismatch");

    Dest dest;
    std::memcpy(&dest, &source, sizeof(Source));
    return dest;
}

namespace {
struct Single
{
    static_assert(std::numeric_limits<float>::is_iec559
               && std::numeric_limits<float>::digits == 24
               && std::numeric_limits<float>::max_exponent == 128,
        "IEEE-754 single-precision implementation required");

    using value_type = float;
    using bits_type = uint32_t;

//  static constexpr int32_t   MaxDigits10     = std::numeric_limits<value_type>::max_digits10;
    static constexpr int32_t   SignificandSize = std::numeric_limits<value_type>::digits; // = p   (includes the hidden bit)
    static constexpr int32_t   ExponentBias    = std::numeric_limits<value_type>::max_exponent - 1 + (SignificandSize - 1);
//  static constexpr int32_t   MaxExponent     = std::numeric_limits<value_type>::max_exponent - 1 - (SignificandSize - 1);
//  static constexpr int32_t   MinExponent     = std::numeric_limits<value_type>::min_exponent - 1 - (SignificandSize - 1);
    static constexpr bits_type MaxIeeeExponent = bits_type{2 * std::numeric_limits<value_type>::max_exponent - 1};
    static constexpr bits_type HiddenBit       = bits_type{1} << (SignificandSize - 1);   // = 2^(p-1)
    static constexpr bits_type SignificandMask = HiddenBit - 1;                           // = 2^(p-1) - 1
    static constexpr bits_type ExponentMask    = MaxIeeeExponent << (SignificandSize - 1);
    static constexpr bits_type SignMask        = ~(~bits_type{0} >> 1);

    bits_type bits;

    explicit Single(bits_type bits_) : bits(bits_) {}
    explicit Single(value_type value) : bits(ReinterpretBits<bits_type>(value)) {}

    bits_type PhysicalSignificand() const {
        return bits & SignificandMask;
    }

    bits_type PhysicalExponent() const {
        return (bits & ExponentMask) >> (SignificandSize - 1);
    }

    bool IsFinite() const {
        return (bits & ExponentMask) != ExponentMask;
    }

    bool IsInf() const {
        return (bits & ExponentMask) == ExponentMask && (bits & SignificandMask) == 0;
    }

    bool IsNaN() const {
        return (bits & ExponentMask) == ExponentMask && (bits & SignificandMask) != 0;
    }

    bool IsZero() const {
        return (bits & ~SignMask) == 0;
    }

    bool SignBit() const {
        return (bits & SignMask) != 0;
    }
};
} // namespace

//==================================================================================================
//
//==================================================================================================

// Returns floor(x / 2^n).
//
// Technically, right-shift of negative integers is implementation defined...
// Should easily be optimized into SAR (or equivalent) instruction.
static inline int32_t FloorDivPow2(int32_t x, int32_t n)
{
#if 0
    return x < 0 ? ~(~x >> n) : (x >> n);
#else
    return x >> n;
#endif
}

// Returns floor(log_10(2^e))
// static inline int32_t FloorLog10Pow2(int32_t e)
// {
//     SF_ASSERT(e >= -1500);
//     SF_ASSERT(e <=  1500);
//     return FloorDivPow2(e * 1262611, 22);
// }

// Returns floor(log_10(3/4 2^e))
// static inline int32_t FloorLog10ThreeQuartersPow2(int32_t e)
// {
//     SF_ASSERT(e >= -1500);
//     SF_ASSERT(e <=  1500);
//     return FloorDivPow2(e * 1262611 - 524031, 22);
// }

// Returns floor(log_2(10^e))
static inline int32_t FloorLog2Pow10(int32_t e)
{
    SF_ASSERT(e >= -1233);
    SF_ASSERT(e <=  1233);
    return FloorDivPow2(e * 1741647, 19);
}

//==================================================================================================
//
//==================================================================================================

static inline uint64_t ComputePow10(int32_t k)
{
    // There are unique beta and r such that 10^k = beta 2^r and
    // 2^63 <= beta < 2^64, namely r = floor(log_2 10^k) - 63 and
    // beta = 2^-r 10^k.
    // Let g = ceil(beta), so (g-1) 2^r < 10^k <= g 2^r, with the latter
    // value being a pretty good overestimate for 10^k.

    // NB: Since for all the required exponents k, we have g < 2^64,
    //     all constants can be stored in 128-bit integers.

    static constexpr int32_t kMin = -31;
    static constexpr int32_t kMax =  45;
    static constexpr uint64_t g[kMax - kMin + 1] = {
        0x81CEB32C4B43FCF5, // -31
        0xA2425FF75E14FC32, // -30
        0xCAD2F7F5359A3B3F, // -29
        0xFD87B5F28300CA0E, // -28
        0x9E74D1B791E07E49, // -27
        0xC612062576589DDB, // -26
        0xF79687AED3EEC552, // -25
        0x9ABE14CD44753B53, // -24
        0xC16D9A0095928A28, // -23
        0xF1C90080BAF72CB2, // -22
        0x971DA05074DA7BEF, // -21
        0xBCE5086492111AEB, // -20
        0xEC1E4A7DB69561A6, // -19
        0x9392EE8E921D5D08, // -18
        0xB877AA3236A4B44A, // -17
        0xE69594BEC44DE15C, // -16
        0x901D7CF73AB0ACDA, // -15
        0xB424DC35095CD810, // -14
        0xE12E13424BB40E14, // -13
        0x8CBCCC096F5088CC, // -12
        0xAFEBFF0BCB24AAFF, // -11
        0xDBE6FECEBDEDD5BF, // -10
        0x89705F4136B4A598, //  -9
        0xABCC77118461CEFD, //  -8
        0xD6BF94D5E57A42BD, //  -7
        0x8637BD05AF6C69B6, //  -6
        0xA7C5AC471B478424, //  -5
        0xD1B71758E219652C, //  -4
        0x83126E978D4FDF3C, //  -3
        0xA3D70A3D70A3D70B, //  -2
        0xCCCCCCCCCCCCCCCD, //  -1
        0x8000000000000000, //   0
        0xA000000000000000, //   1
        0xC800000000000000, //   2
        0xFA00000000000000, //   3
        0x9C40000000000000, //   4
        0xC350000000000000, //   5
        0xF424000000000000, //   6
        0x9896800000000000, //   7
        0xBEBC200000000000, //   8
        0xEE6B280000000000, //   9
        0x9502F90000000000, //  10
        0xBA43B74000000000, //  11
        0xE8D4A51000000000, //  12
        0x9184E72A00000000, //  13
        0xB5E620F480000000, //  14
        0xE35FA931A0000000, //  15
        0x8E1BC9BF04000000, //  16
        0xB1A2BC2EC5000000, //  17
        0xDE0B6B3A76400000, //  18
        0x8AC7230489E80000, //  19
        0xAD78EBC5AC620000, //  20
        0xD8D726B7177A8000, //  21
        0x878678326EAC9000, //  22
        0xA968163F0A57B400, //  23
        0xD3C21BCECCEDA100, //  24
        0x84595161401484A0, //  25
        0xA56FA5B99019A5C8, //  26
        0xCECB8F27F4200F3A, //  27
        0x813F3978F8940985, //  28
        0xA18F07D736B90BE6, //  29
        0xC9F2C9CD04674EDF, //  30
        0xFC6F7C4045812297, //  31
        0x9DC5ADA82B70B59E, //  32
        0xC5371912364CE306, //  33
        0xF684DF56C3E01BC7, //  34
        0x9A130B963A6C115D, //  35
        0xC097CE7BC90715B4, //  36
        0xF0BDC21ABB48DB21, //  37
        0x96769950B50D88F5, //  38
        0xBC143FA4E250EB32, //  39
        0xEB194F8E1AE525FE, //  40
        0x92EFD1B8D0CF37BF, //  41
        0xB7ABC627050305AE, //  42
        0xE596B7B0C643C71A, //  43
        0x8F7E32CE7BEA5C70, //  44
        0xB35DBF821AE4F38C, //  45
    };

    SF_ASSERT(k >= kMin);
    SF_ASSERT(k <= kMax);
    return g[static_cast<uint32_t>(k - kMin)];
}

static inline uint32_t Lo32(uint64_t x)
{
    return static_cast<uint32_t>(x);
}

static inline uint32_t Hi32(uint64_t x)
{
    return static_cast<uint32_t>(x >> 32);
}

#if defined(__SIZEOF_INT128__)

static inline uint32_t RoundToOdd(uint64_t g, uint32_t cp)
{
    __extension__ using uint128_t = unsigned __int128;

    const uint128_t p = uint128_t{g} * cp;

    const uint32_t y1 = Lo32(static_cast<uint64_t>(p >> 64));
    const uint32_t y0 = Hi32(static_cast<uint64_t>(p));

    return y1 | (y0 > 1);
}

#elif defined(_MSC_VER) && defined(_M_X64)

static inline uint32_t RoundToOdd(uint64_t g, uint32_t cpHi)
{
    uint64_t p1 = 0;
    uint64_t p0 = _umul128(g, cpHi, &p1);

    const uint32_t y1 = Lo32(p1);
    const uint32_t y0 = Hi32(p0);

    return y1 | (y0 > 1);
}

#else

static inline uint32_t RoundToOdd(uint64_t g, uint32_t cp)
{
    const uint64_t b01 = uint64_t{Lo32(g)} * cp;
    const uint64_t b11 = uint64_t{Hi32(g)} * cp;
    const uint64_t hi = b11 + Hi32(b01);

    const uint32_t y1 = Hi32(hi);
    const uint32_t y0 = Lo32(hi);

    return y1 | (y0 > 1);
}

#endif

// Returns whether value is divisible by 2^e2
static inline bool MultipleOfPow2(uint32_t value, int32_t e2)
{
    SF_ASSERT(e2 >= 0);
    SF_ASSERT(e2 <= 31);
    return (value & ((uint32_t{1} << e2) - 1)) == 0;
}

namespace {
struct FloatingDecimal32 {
    uint32_t digits; // num_digits <= 9
    int32_t exponent;
};
}

static inline FloatingDecimal32 ToDecimal32(uint32_t ieee_significand, uint32_t ieee_exponent)
{
    uint32_t c;
    int32_t q;
    if (ieee_exponent != 0)
    {
        c = Single::HiddenBit | ieee_significand;
        q = static_cast<int32_t>(ieee_exponent) - Single::ExponentBias;

        if (0 <= -q && -q < Single::SignificandSize && MultipleOfPow2(c, -q))
        {
            return {c >> -q, 0};
        }
    }
    else
    {
        c = ieee_significand;
        q = 1 - Single::ExponentBias;
    }

    const bool is_even = (c % 2 == 0);
    const bool accept_lower = is_even;
    const bool accept_upper = is_even;

    const bool lower_boundary_is_closer = (ieee_significand == 0 && ieee_exponent > 1);

//  const int32_t qb = q - 2;
    const uint32_t cbl = 4 * c - 2 + lower_boundary_is_closer;
    const uint32_t cb  = 4 * c;
    const uint32_t cbr = 4 * c + 2;

    // (q * 1262611         ) >> 22 == floor(log_10(    2^q))
    // (q * 1262611 - 524031) >> 22 == floor(log_10(3/4 2^q))
    SF_ASSERT(q >= -1500);
    SF_ASSERT(q <=  1500);
    const int32_t k = FloorDivPow2(q * 1262611 - (lower_boundary_is_closer ? 524031 : 0), 22);

    const int32_t h = q + FloorLog2Pow10(-k) + 1;
    SF_ASSERT(h >= 1);
    SF_ASSERT(h <= 4);

    const uint64_t pow10 = ComputePow10(-k);
    const uint32_t vbl = RoundToOdd(pow10, cbl << h);
    const uint32_t vb  = RoundToOdd(pow10, cb  << h);
    const uint32_t vbr = RoundToOdd(pow10, cbr << h);

    const uint32_t lower = vbl + !accept_lower;
    const uint32_t upper = vbr - !accept_upper;

    // See Figure 4 in [1].
    // And the modifications in Figure 6.

    const uint32_t s = vb / 4; // NB: 4 * s == vb & ~3 == vb & -4

    if (s >= 10) // vb >= 40
    {
        const uint32_t sp = s / 10; // = vb / 40
        const bool up_inside = lower <= 40 * sp;
        const bool wp_inside =          40 * sp + 40 <= upper;
//      if (up_inside || wp_inside) // NB: At most one of u' and w' is in R_v.
        if (up_inside != wp_inside)
        {
            return {sp + wp_inside, k + 1};
        }
    }

    const bool u_inside = lower <= 4 * s;
    const bool w_inside =          4 * s + 4 <= upper;
    if (u_inside != w_inside)
    {
        return {s + w_inside, k};
    }

    // NB: s & 1 == vb & 0x4
    const uint32_t mid = 4 * s + 2; // = 2(s + t)
    const bool round_up = vb > mid || (vb == mid && (s & 1) != 0);

    return {s + round_up, k};
}

//==================================================================================================
// ToChars
//==================================================================================================

static inline void Utoa_2Digits(char* buf, uint32_t digits)
{
    static constexpr char Digits100[200] = {
        '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
        '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
        '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
        '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
        '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
        '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
        '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
        '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
        '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
        '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9',
    };

    SF_ASSERT(digits <= 99);
    std::memcpy(buf, &Digits100[2 * digits], 2 * sizeof(char));
}

static inline int TrailingZeros_2Digits(uint32_t digits)
{
    static constexpr int8_t TrailingZeros100[100] = {
        2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };

    SF_ASSERT(digits <= 99);
    return TrailingZeros100[digits];
}

static inline int PrintDecimalDigitsBackwards(char* buf, uint32_t output)
{
    int tz = 0; // number of trailing zeros removed.
    int nd = 0; // number of decimal digits processed.

    // At most 9 digits remaining

    if (output >= 10000)
    {
        const uint32_t q = output / 10000;
        const uint32_t r = output % 10000;
        output = q;
        buf -= 4;
        if (r != 0)
        {
            const uint32_t rH = r / 100;
            const uint32_t rL = r % 100;
            Utoa_2Digits(buf + 0, rH);
            Utoa_2Digits(buf + 2, rL);

            tz = TrailingZeros_2Digits(rL == 0 ? rH : rL) + (rL == 0 ? 2 : 0);
        }
        else
        {
            tz = 4;
        }
        nd = 4;
    }

    // At most 5 digits remaining.

    if (output >= 100)
    {
        const uint32_t q = output / 100;
        const uint32_t r = output % 100;
        output = q;
        buf -= 2;
        Utoa_2Digits(buf, r);
        if (tz == nd)
        {
            tz += TrailingZeros_2Digits(r);
        }
        nd += 2;

        if (output >= 100)
        {
            const uint32_t q2 = output / 100;
            const uint32_t r2 = output % 100;
            output = q2;
            buf -= 2;
            Utoa_2Digits(buf, r2);
            if (tz == nd)
            {
                tz += TrailingZeros_2Digits(r2);
            }
            nd += 2;
        }
    }

    // At most 2 digits remaining.

    SF_ASSERT(output >= 1);
    SF_ASSERT(output <= 99);

    if (output >= 10)
    {
        const uint32_t q = output;
        buf -= 2;
        Utoa_2Digits(buf, q);
        if (tz == nd)
        {
            tz += TrailingZeros_2Digits(q);
        }
//      nd += 2;
    }
    else
    {
        const uint32_t q = output;
        SF_ASSERT(q >= 1);
        SF_ASSERT(q <= 9);
        *--buf = static_cast<char>('0' + q);
    }

    return tz;
}

static inline int32_t DecimalLength(uint32_t v)
{
    SF_ASSERT(v >= 1);
    SF_ASSERT(v <= 999999999u);

    if (v >= 100000000u) { return 9; }
    if (v >= 10000000u) { return 8; }
    if (v >= 1000000u) { return 7; }
    if (v >= 100000u) { return 6; }
    if (v >= 10000u) { return 5; }
    if (v >= 1000u) { return 4; }
    if (v >= 100u) { return 3; }
    if (v >= 10u) { return 2; }
    return 1;
}

static inline char* FormatDigits(char* buffer, uint32_t digits, int32_t decimal_exponent, bool force_trailing_dot_zero = false)
{
    static constexpr int32_t MinFixedDecimalPoint = -4;
    static constexpr int32_t MaxFixedDecimalPoint =  9;
    static_assert(MinFixedDecimalPoint <= -1, "internal error");
    static_assert(MaxFixedDecimalPoint >=  9, "internal error");

    SF_ASSERT(digits >= 1);
    SF_ASSERT(digits <= 999999999u);
    SF_ASSERT(decimal_exponent >= -99);
    SF_ASSERT(decimal_exponent <=  99);

    int32_t num_digits = DecimalLength(digits);
    const int32_t decimal_point = num_digits + decimal_exponent;

    const bool use_fixed = MinFixedDecimalPoint <= decimal_point && decimal_point <= MaxFixedDecimalPoint;

    // Prepare the buffer.
    // Avoid calling memset/memcpy with variable arguments below...

    std::memset(buffer, '0', 16);
    static_assert(MinFixedDecimalPoint >= -14, "internal error");
    static_assert(MaxFixedDecimalPoint <=  16, "internal error");

    int32_t decimal_digits_position;
    if (use_fixed)
    {
        if (decimal_point <= 0)
        {
            // 0.[000]digits
            decimal_digits_position = 2 - decimal_point;
        }
        else
        {
            // dig.its
            // digits[000]
            decimal_digits_position = 0;
        }
    }
    else
    {
        // dE+123 or d.igitsE+123
        decimal_digits_position = 1;
    }

    char* digits_end = buffer + decimal_digits_position + num_digits;

    const int tz = PrintDecimalDigitsBackwards(digits_end, digits);
    digits_end -= tz;
    num_digits -= tz;
//  decimal_exponent += tz; // => decimal_point unchanged.

    if (use_fixed)
    {
        if (decimal_point <= 0)
        {
            // 0.[000]digits
            buffer[1] = '.';
            buffer = digits_end;
        }
        else if (decimal_point < num_digits)
        {
            // dig.its
            std::memmove(buffer + decimal_point + 1, buffer + decimal_point, 8);
            buffer[decimal_point] = '.';
            buffer = digits_end + 1;
        }
        else
        {
            // digits[000]
            buffer += decimal_point;
            if (force_trailing_dot_zero)
            {
                std::memcpy(buffer, ".0", 2);
                buffer += 2;
            }
        }
    }
    else
    {
        buffer[0] = buffer[1];
        if (num_digits == 1)
        {
            // dE+123
            ++buffer;
        }
        else
        {
            // d.igitsE+123
            buffer[1] = '.';
            buffer = digits_end;
        }

        const int32_t scientific_exponent = decimal_point - 1;
//      SF_ASSERT(scientific_exponent != 0);

        std::memcpy(buffer, scientific_exponent < 0 ? "e-" : "e+", 2);
        buffer += 2;

        const uint32_t k = static_cast<uint32_t>(scientific_exponent < 0 ? -scientific_exponent : scientific_exponent);
        if (k < 10)
        {
            *buffer++ = static_cast<char>('0' + k);
        }
        else
        {
            Utoa_2Digits(buffer, k);
            buffer += 2;
        }
    }

    return buffer;
}

static inline char* ToChars(char* buffer, float value, bool force_trailing_dot_zero = false)
{
    const Single v(value);

    const uint32_t significand = v.PhysicalSignificand();
    const uint32_t exponent = v.PhysicalExponent();

    if (exponent != Single::MaxIeeeExponent) // [[likely]]
    {
        // Finite

        buffer[0] = '-';
        buffer += v.SignBit();

        if (exponent != 0 || significand != 0) // [[likely]]
        {
            // != 0

            const auto dec = ToDecimal32(significand, exponent);
            return FormatDigits(buffer, dec.digits, dec.exponent, force_trailing_dot_zero);
        }
        else
        {
            std::memcpy(buffer, "0.0 ", 4);
            buffer += force_trailing_dot_zero ? 3 : 1;
            return buffer;
        }
    }

    if (significand == 0)
    {
        buffer[0] = '-';
        buffer += v.SignBit();

        std::memcpy(buffer, "inf ", 4);
        return buffer + 3;
    }
    else
    {
        std::memcpy(buffer, "nan ", 4);
        return buffer + 3;
    }
}

//==================================================================================================
//
//==================================================================================================

char* schubfach::Ftoa(char* buffer, float value)
{
    return ToChars(buffer, value);
}
