#pragma once
#include <compare>
#include <cstdint>
#include <functional>
#include <type_traits>

template<class T>
concept trivially_pod =
  std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

template<class Derived, class T>
struct Equality
{
  friend constexpr auto operator==(Derived a, Derived b) -> bool
  {
    return a.get() == b.get();
  }
};

template<class Derived, class T>
struct Additive
{
  friend constexpr auto operator+(Derived a, Derived b) -> Derived
  {
    return Derived{ a.get() + b.get() };
  }
  friend constexpr auto operator-(Derived a, Derived b) -> Derived
  {
    return Derived{ a.get() - b.get() };
  }
  friend constexpr auto& operator+=(Derived& a, Derived b)
  {
    a = a + b;
    return a;
  }
  friend constexpr auto& operator-=(Derived& a, Derived b)
  {
    a = a - b;
    return a;
  }
};

template<class T, class Tag, template<class, class> class... Mixins>
  requires trivially_pod<T>
struct Strong : Mixins<Strong<T, Tag, Mixins...>, T>...
{
  using value_type = T;

  T value;

  constexpr Strong() = default;
  explicit constexpr Strong(T v)
    : value{ v }
  {
  }

  constexpr auto get() const -> T { return value; }
  explicit constexpr operator T() const { return value; }
};

template<class T, class Tag, template<class, class> class... Ms>
struct std::hash<Strong<T, Tag, Ms...>>
{
  auto operator()(const Strong<T, Tag, Ms...>& v) const noexcept -> std::size_t
  {
    return std::hash<T>{}(v.get());
  }
};

// aliases
struct VertexOffsetTag;
struct IndexOffsetTag;

using VertexOffset = Strong<std::uint32_t, VertexOffsetTag, Equality, Additive>;
using IndexOffset = Strong<std::uint32_t, IndexOffsetTag, Equality, Additive>;
