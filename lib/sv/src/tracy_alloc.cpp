// tracy_new_delete.cpp
#include <cstddef>
#include <cstdlib>
#include <new>
#include <tracy/Tracy.hpp>

namespace {
constexpr const char* alloc_name = "global-new";

auto
alloc_and_track(std::size_t n) -> void*
{
  if (void* p = std::malloc(n)) {
#if defined(TRACY_ENABLE)
    // choose one:
    // TracyAlloc(p, n);
    TracyAllocN(p, n, alloc_name);
#endif
    return p;
  }
  throw std::bad_alloc{};
}

auto
free_and_track(void* p) noexcept -> void
{
#if defined(TRACY_ENABLE)
  // choose one:
  // TracyFree(p);
  TracyFreeN(p, alloc_name);
#endif
  std::free(p);
}

#if defined(_MSC_VER)
auto
aligned_alloc_impl(std::size_t n, std::size_t a) -> void*
{
  return _aligned_malloc(n, a);
}
auto
aligned_free_impl(void* p) noexcept -> void
{
  _aligned_free(p);
}
#else
auto
aligned_alloc_impl(std::size_t n, std::size_t a) -> void*
{
  if (a < alignof(void*))
    a = alignof(void*);
  const std::size_t rounded = (n + a - 1) / a * a;
  return std::aligned_alloc(a, rounded);
}
auto
aligned_free_impl(void* p) noexcept -> void
{
  std::free(p);
}
#endif

auto
aligned_and_track(std::size_t n, std::size_t a) -> void*
{
  if (void* p = aligned_alloc_impl(n, a)) {
#if defined(TRACY_ENABLE)
    // TracyAlloc(p, n);
    TracyAllocN(p, n, alloc_name);
#endif
    return p;
  }
  throw std::bad_alloc{};
}

auto
aligned_free_and_track(void* p) noexcept -> void
{
#if defined(TRACY_ENABLE)
  // TracyFree(p);
  TracyFreeN(p, alloc_name);
#endif
  aligned_free_impl(p);
}
} // namespace

auto
operator new(std::size_t n) -> void*
{
  return alloc_and_track(n);
}
auto
operator new[](std::size_t n) -> void*
{
  return alloc_and_track(n);
}
auto
operator delete(void* p) noexcept -> void
{
  free_and_track(p);
}
auto
operator delete[](void* p) noexcept -> void
{
  free_and_track(p);
}

#if defined(__cpp_sized_deallocation)
auto
operator delete(void* p, std::size_t) noexcept -> void
{
  free_and_track(p);
}
auto
operator delete[](void* p, std::size_t) noexcept -> void
{
  free_and_track(p);
}
#endif

#if defined(__cpp_aligned_new)
auto
operator new(std::size_t n, std::align_val_t a) -> void*
{
  return aligned_and_track(n, static_cast<std::size_t>(a));
}
auto
operator new[](std::size_t n, std::align_val_t a) -> void*
{
  return aligned_and_track(n, static_cast<std::size_t>(a));
}
auto
operator delete(void* p, std::align_val_t) noexcept -> void
{
  aligned_free_and_track(p);
}
auto
operator delete[](void* p, std::align_val_t) noexcept -> void
{
  aligned_free_and_track(p);
}

#if defined(__cpp_sized_deallocation)
auto
operator delete(void* p, std::size_t, std::align_val_t) noexcept -> void
{
  aligned_free_and_track(p);
}
auto
operator delete[](void* p, std::size_t, std::align_val_t) noexcept -> void
{
  aligned_free_and_track(p);
}
#endif
#endif

auto
operator new(std::size_t n, const std::nothrow_t&) noexcept -> void*
{
  void* p = std::malloc(n);
#if defined(TRACY_ENABLE)
  if (p)
    TracyAllocN(p, n, alloc_name);
#endif
  return p;
}
auto
operator new[](std::size_t n, const std::nothrow_t&) noexcept -> void*
{
  void* p = std::malloc(n);
#if defined(TRACY_ENABLE)
  if (p)
    TracyAllocN(p, n, alloc_name);
#endif
  return p;
}
auto
operator delete(void* p, const std::nothrow_t&) noexcept -> void
{
  free_and_track(p);
}
auto
operator delete[](void* p, const std::nothrow_t&) noexcept -> void
{
  free_and_track(p);
}

#if defined(__cpp_aligned_new)
auto
operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept
  -> void*
{
  void* p = aligned_alloc_impl(n, static_cast<std::size_t>(a));
#if defined(TRACY_ENABLE)
  if (p)
    TracyAllocN(p, n, alloc_name);
#endif
  return p;
}
auto
operator new[](std::size_t n,
               std::align_val_t a,
               const std::nothrow_t&) noexcept -> void*
{
  void* p = aligned_alloc_impl(n, static_cast<std::size_t>(a));
#if defined(TRACY_ENABLE)
  if (p)
    TracyAllocN(p, n, alloc_name);
#endif
  return p;
}
auto
operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept
  -> void
{
  aligned_free_and_track(p);
}
auto
operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept
  -> void
{
  aligned_free_and_track(p);
}
#endif
