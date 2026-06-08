#ifndef ATOMIC_REF_POLYFILL_H_
#define ATOMIC_REF_POLYFILL_H_
#include <atomic>
#include <type_traits>
#if !defined(__cpp_lib_atomic_ref)
#define __cpp_lib_atomic_ref 201806L
namespace std {
template <typename T> struct atomic_ref {
  static_assert(std::is_trivially_copyable_v<T>);
  static constexpr std::size_t required_alignment = alignof(T);
  explicit atomic_ref(T& obj) : ptr_(&obj) {}
  atomic_ref(const atomic_ref&) = default;
  T load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    return reinterpret_cast<const std::atomic<T>*>(ptr_)->load(order);
  }
  void store(T desired, std::memory_order order = std::memory_order_seq_cst) const noexcept {
    reinterpret_cast<std::atomic<T>*>(ptr_)->store(desired, order);
  }
private:
  T* ptr_;
};
template <typename T> atomic_ref(T&) -> atomic_ref<T>;
}  // namespace std
#endif
#endif
