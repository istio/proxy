// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_CHECKED_ITERATORS_H_
#define BASE_CONTAINERS_CHECKED_ITERATORS_H_

#include <iterator>
#include <memory>
#include <type_traits>

#include "polyfills/base/check_op.h"
#include "base/containers/util.h"
#include "build/build_config.h"

namespace gurl_base {

template <typename T>
class CheckedContiguousIterator {
 public:
  using difference_type = std::ptrdiff_t;
  using value_type = std::remove_cv_t<T>;
  using pointer = T*;
  using reference = T&;
  using iterator_category = std::random_access_iterator_tag;

  // Required for converting constructor below.
  template <typename U>
  friend class CheckedContiguousIterator;

  // Required for certain libc++ algorithm optimizations that are not available
  // for NaCl.
#if defined(_LIBCPP_VERSION) && !BUILDFLAG(IS_NACL)
  template <typename Ptr>
  friend struct std::pointer_traits;
#endif

  constexpr CheckedContiguousIterator() = default;

  constexpr CheckedContiguousIterator(T* start, const T* end)
      : CheckedContiguousIterator(start, start, end) {}

  constexpr CheckedContiguousIterator(const T* start, T* current, const T* end)
      : start_(start), current_(current), end_(end) {
    GURL_CHECK_LE(start, current);
    GURL_CHECK_LE(current, end);
  }

  constexpr CheckedContiguousIterator(const CheckedContiguousIterator& other) =
      default;

  // Converting constructor allowing conversions like CCI<T> to CCI<const T>,
  // but disallowing CCI<const T> to CCI<T> or CCI<Derived> to CCI<Base>, which
  // are unsafe. Furthermore, this is the same condition as used by the
  // converting constructors of std::span<T> and std::unique_ptr<T[]>.
  // See https://wg21.link/n4042 for details.
  template <
      typename U,
      std::enable_if_t<std::is_convertible<U (*)[], T (*)[]>::value>* = nullptr>
  constexpr CheckedContiguousIterator(const CheckedContiguousIterator<U>& other)
      : start_(other.start_), current_(other.current_), end_(other.end_) {
    // We explicitly don't delegate to the 3-argument constructor here. Its
    // CHECKs would be redundant, since we expect |other| to maintain its own
    // invariant. However, DCHECKs never hurt anybody. Presumably.
    GURL_DCHECK_LE(other.start_, other.current_);
    GURL_DCHECK_LE(other.current_, other.end_);
  }

  ~CheckedContiguousIterator() = default;

  constexpr CheckedContiguousIterator& operator=(
      const CheckedContiguousIterator& other) = default;

  friend constexpr bool operator==(const CheckedContiguousIterator& lhs,
                                   const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ == rhs.current_;
  }

  friend constexpr bool operator!=(const CheckedContiguousIterator& lhs,
                                   const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ != rhs.current_;
  }

  friend constexpr bool operator<(const CheckedContiguousIterator& lhs,
                                  const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ < rhs.current_;
  }

  friend constexpr bool operator<=(const CheckedContiguousIterator& lhs,
                                   const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ <= rhs.current_;
  }
  friend constexpr bool operator>(const CheckedContiguousIterator& lhs,
                                  const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ > rhs.current_;
  }

  friend constexpr bool operator>=(const CheckedContiguousIterator& lhs,
                                   const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ >= rhs.current_;
  }

  constexpr CheckedContiguousIterator& operator++() {
    GURL_CHECK_NE(current_, end_);
    ++current_;
    return *this;
  }

  constexpr CheckedContiguousIterator operator++(int) {
    CheckedContiguousIterator old = *this;
    ++*this;
    return old;
  }

  constexpr CheckedContiguousIterator& operator--() {
    GURL_CHECK_NE(current_, start_);
    --current_;
    return *this;
  }

  constexpr CheckedContiguousIterator operator--(int) {
    CheckedContiguousIterator old = *this;
    --*this;
    return old;
  }

  constexpr CheckedContiguousIterator& operator+=(difference_type rhs) {
    if (rhs > 0) {
      GURL_CHECK_LE(rhs, end_ - current_);
    } else {
      GURL_CHECK_LE(-rhs, current_ - start_);
    }
    current_ += rhs;
    return *this;
  }

  constexpr CheckedContiguousIterator operator+(difference_type rhs) const {
    CheckedContiguousIterator it = *this;
    it += rhs;
    return it;
  }

  constexpr CheckedContiguousIterator& operator-=(difference_type rhs) {
    if (rhs < 0) {
      GURL_CHECK_LE(-rhs, end_ - current_);
    } else {
      GURL_CHECK_LE(rhs, current_ - start_);
    }
    current_ -= rhs;
    return *this;
  }

  constexpr CheckedContiguousIterator operator-(difference_type rhs) const {
    CheckedContiguousIterator it = *this;
    it -= rhs;
    return it;
  }

  constexpr friend difference_type operator-(
      const CheckedContiguousIterator& lhs,
      const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ - rhs.current_;
  }

  constexpr reference operator*() const {
    GURL_CHECK_NE(current_, end_);
    return *current_;
  }

  constexpr pointer operator->() const {
    GURL_CHECK_NE(current_, end_);
    return current_;
  }

  constexpr reference operator[](difference_type rhs) const {
    GURL_CHECK_GE(rhs, 0);
    GURL_CHECK_LT(rhs, end_ - current_);
    return current_[rhs];
  }

  [[nodiscard]] static bool IsRangeMoveSafe(
      const CheckedContiguousIterator& from_begin,
      const CheckedContiguousIterator& from_end,
      const CheckedContiguousIterator& to) {
    if (from_end < from_begin)
      return false;
    const auto from_begin_uintptr = get_uintptr(from_begin.current_);
    const auto from_end_uintptr = get_uintptr(from_end.current_);
    const auto to_begin_uintptr = get_uintptr(to.current_);
    const auto to_end_uintptr =
        get_uintptr((to + std::distance(from_begin, from_end)).current_);

    return to_begin_uintptr >= from_end_uintptr ||
           to_end_uintptr <= from_begin_uintptr;
  }

 private:
  constexpr void CheckComparable(const CheckedContiguousIterator& other) const {
    GURL_CHECK_EQ(start_, other.start_);
    GURL_CHECK_EQ(end_, other.end_);
  }

  const T* start_ = nullptr;
  T* current_ = nullptr;
  const T* end_ = nullptr;
};

template <typename T>
using CheckedContiguousConstIterator = CheckedContiguousIterator<const T>;

}  // namespace base

#if defined(_LIBCPP_VERSION) && !BUILDFLAG(IS_NACL)
// Specialize both std::__is_cpp17_contiguous_iterator and std::pointer_traits
// for CCI in case we compile with libc++ outside of NaCl. The former is
// required to enable certain algorithm optimizations (e.g. std::copy can be a
// simple std::memmove under certain circumstances), and is a precursor to
// C++20's std::contiguous_iterator concept [1]. Once we actually use C++20 it
// will be enough to add `using iterator_concept = std::contiguous_iterator_tag`
// to the iterator class [2], and we can get rid of this non-standard
// specialization.
//
// The latter is required to obtain the underlying raw pointer without resulting
// in GURL_CHECK failures. The important bit is the `to_address(pointer)` overload,
// which is the standard blessed way to customize `std::to_address(pointer)` in
// C++20 [3].
//
// [1] https://wg21.link/iterator.concept.contiguous
// [2] https://wg21.link/std.iterator.tags
// [3] https://wg21.link/pointer.traits.optmem

#ifdef SUPPORTS_CPP_17_CONTIGUOUS_ITERATOR
#if defined(_LIBCPP_VERSION)

// TODO(crbug.com/1284275): Remove when C++20 is on by default, as the use
// of `iterator_concept` above should suffice.
_LIBCPP_BEGIN_NAMESPACE_STD

// TODO(crbug.com/1449299): https://reviews.llvm.org/D150801 renamed this from
// `__is_cpp17_contiguous_iterator` to `__libcpp_is_contiguous_iterator`. Clean
// up the old spelling after libc++ rolls.
template <typename T>
struct __is_cpp17_contiguous_iterator;
template <typename T>
struct __is_cpp17_contiguous_iterator<::gurl_base::CheckedContiguousIterator<T>>
    : true_type {};
template <typename T>
struct __libcpp_is_contiguous_iterator;
template <typename T>
struct __libcpp_is_contiguous_iterator<::gurl_base::CheckedContiguousIterator<T>>
    : true_type {};

_LIBCPP_END_NAMESPACE_STD

#endif
#endif

namespace std {

template <typename T>
struct pointer_traits<::gurl_base::CheckedContiguousIterator<T>> {
  using pointer = ::gurl_base::CheckedContiguousIterator<T>;
  using element_type = T;
  using difference_type = ptrdiff_t;

  template <typename U>
  using rebind = ::gurl_base::CheckedContiguousIterator<U>;

  static constexpr pointer pointer_to(element_type& r) noexcept {
    return pointer(&r, &r);
  }

  static constexpr element_type* to_address(pointer p) noexcept {
    return p.current_;
  }
};

}  // namespace std
#endif

#endif  // BASE_CONTAINERS_CHECKED_ITERATORS_H_
