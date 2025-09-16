#pragma once

#include "sv/object_handle.hpp"
#include "sv/staging_allocator.hpp"
#include "sv/texture.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdint>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

namespace sv {

namespace detail {
struct FreelistVector
{
  std::vector<std::uint32_t> data;
  auto push(std::uint32_t v) -> void { data.push_back(v); }
  auto pop() -> std::uint32_t
  {
    if (data.empty())
      return std::numeric_limits<std::uint32_t>::max();
    auto v = data.back();
    data.pop_back();
    return v;
  }
  auto clear() -> void { data.clear(); }
};

struct Tagged
{
  std::uint32_t idx;
  std::uint32_t tag;
};

struct FreelistAtomic
{
  std::atomic<std::uint64_t> head{ pack(
    { std::numeric_limits<std::uint32_t>::max(), 0 }) };
  std::vector<std::uint32_t> next;

  static auto pack(Tagged t) -> std::uint64_t
  {
    return (static_cast<std::uint64_t>(t.tag) << 32) | t.idx;
  }
  static auto unpack(std::uint64_t v) -> Tagged
  {
    return { static_cast<std::uint32_t>(v & 0xFFFF'FFFFull),
             static_cast<std::uint32_t>(v >> 32) };
  }

  auto ensure_capacity(std::size_t n) -> void
  {
    if (next.size() < n)
      next.resize(n, std::numeric_limits<std::uint32_t>::max());
  }

  auto push(std::uint32_t i) -> void
  {
    for (;;) {
      auto h = head.load(std::memory_order_acquire);
      auto t = unpack(h);
      next[i] = t.idx;
      Tagged n{ i, static_cast<std::uint32_t>(t.tag + 1) };
      if (head.compare_exchange_weak(
            h, pack(n), std::memory_order_release, std::memory_order_relaxed))
        return;
    }
  }

  auto pop() -> std::uint32_t
  {
    for (;;) {
      auto h = head.load(std::memory_order_acquire);
      auto t = unpack(h);
      if (t.idx == std::numeric_limits<std::uint32_t>::max())
        return t.idx;
      auto nxt = next[t.idx];
      Tagged n{ nxt, static_cast<std::uint32_t>(t.tag + 1) };
      if (head.compare_exchange_weak(
            h, pack(n), std::memory_order_release, std::memory_order_relaxed))
        return t.idx;
    }
  }

  auto clear() -> void
  {
    head.store(pack({ std::numeric_limits<std::uint32_t>::max(), 0 }),
               std::memory_order_release);
    std::fill(
      next.begin(), next.end(), std::numeric_limits<std::uint32_t>::max());
  }
};

inline auto
bump_generation(std::uint32_t g) -> std::uint32_t
{
  g += 1U;
  if (g == invalid_generation)
    g += 1U;
  return g;
}
}

template<typename Handle, typename TImpl, bool LockFree = false>
class Pool
{
  static constexpr std::uint32_t npos =
    std::numeric_limits<std::uint32_t>::max();
  using Freelist = std::
    conditional_t<LockFree, detail::FreelistAtomic, detail::FreelistVector>;

public:
  using handle_type = Handle;
  using value_type = TImpl;

  auto size() const -> std::size_t { return dense_storage.size(); }
  auto capacity() const -> std::size_t { return dense_storage.capacity(); }

  auto reserved_prefix() -> std::uint32_t& { return reserved; }

  template<typename... Args>
  auto seed_reserved(Args&&... args) -> handle_type
  {
    if (reserved_prefix() > 0)
      return handle_type{ 0, load_generation(0) };
    auto idx = acquire_index(); // will be 0 in an empty pool
    dense_storage.emplace_back(std::forward<Args>(args)...);
    dense_to_sparse.push_back(idx);
    sparse_to_dense.at(idx) = 0;
    ensure_live_generation(idx);
    reserved_prefix() = 1;
    return handle_type{ idx, load_generation(idx) };
  }

  template<typename... Args>
  auto emplace(Args&&... args) -> handle_type
  {
    auto idx = acquire_index();
    auto dense_i = static_cast<std::uint32_t>(dense_storage.size());
    dense_storage.emplace_back(std::forward<Args>(args)...);
    dense_to_sparse[dense_i] = idx;
    sparse_to_dense[idx] = dense_i;
    ensure_live_generation(idx);
    return { idx, load_generation(idx) };
  }

  auto insert(TImpl&& value) -> handle_type
  {
    auto idx = acquire_index();
    auto dense_i = static_cast<std::uint32_t>(dense_storage.size());
    dense_storage.emplace_back(std::move(value));
    dense_to_sparse[dense_i] = idx;
    sparse_to_dense[idx] = dense_i;
    ensure_live_generation(idx);
    return { idx, load_generation(idx) };
  }

  auto erase(handle_type h) -> bool
  {
    if (!is_valid(h))
      return false;
    auto s = h.index();
    auto d = sparse_to_dense[s];
    auto last = static_cast<std::uint32_t>(dense_storage.size() - 1);

    if (d != last) {
      std::swap(dense_storage[d], dense_storage[last]);
      auto moved_s = dense_to_sparse[last];
      dense_to_sparse[d] = moved_s;
      sparse_to_dense[moved_s] = d;
    }

    dense_storage.pop_back();
    retire_index(s);
    return true;
  }

  auto is_valid(handle_type h) const -> bool
  {
    auto i = h.index();
    if (i >= generations.size())
      return false;
    if (i == npos)
      return false;
    if (sparse_to_dense[i] == npos)
      return false;
    auto g = load_generation(i);
    if (g == invalid_generation)
      return false;
    return g == h.generation();
  }

  auto get(handle_type h) -> TImpl*
  {
    if (!is_valid(h))
      return nullptr;
    return &dense_storage[sparse_to_dense[h.index()]];
  }

  auto get(std::uint32_t index) -> TImpl*
  {
    if (!is_valid({
          index,
          generations.at(index),
        }))
      return nullptr;
    return &dense_storage[sparse_to_dense.at(index)];
  }

  auto get(handle_type h) const -> const TImpl*
  {
    if (!is_valid(h))
      return nullptr;
    return &dense_storage[sparse_to_dense[h.index()]];
  }

  auto operator[](handle_type h) -> TImpl& { return *get(h); }
  auto operator[](handle_type h) const -> const TImpl& { return *get(h); }

  auto clear() -> void
  {
    dense_storage.clear();
    for (std::uint32_t i = 0; i < generations.size(); ++i) {
      store_generation(i, detail::bump_generation(load_generation(i)));
      sparse_to_dense[i] = npos;
      freelist.push(i);
    }
  }

  template<typename Fn>
  auto for_each_dense(Fn&& fn)
  {
    for (auto i = 0ULL; i < dense_storage.size(); ++i) {
      fn(static_cast<std::uint32_t>(i), dense_storage[i]);
    }
  }

private:
  auto acquire_index() -> std::uint32_t
  {
    if constexpr (LockFree)
      freelist.ensure_capacity(generations.size());
    auto idx = freelist.pop();
    if (idx != npos)
      return idx;

    auto new_idx = static_cast<std::uint32_t>(generations.size());
    generations.push_back(invalid_generation);
    sparse_to_dense.push_back(npos);
    dense_to_sparse.push_back(npos);
    return new_idx;
  }

  auto ensure_live_generation(std::uint32_t idx) -> void
  {
    auto g = load_generation(idx);
    if (g == invalid_generation)
      store_generation(idx, detail::bump_generation(g));
  }

  auto retire_index(std::uint32_t idx) -> void
  {
    store_generation(idx, detail::bump_generation(load_generation(idx)));
    sparse_to_dense[idx] = npos;
    freelist.push(idx);
  }

  auto load_generation(std::uint32_t idx) const -> std::uint32_t
  {
    if constexpr (LockFree) {
      std::atomic_ref<const std::uint32_t> ar{ generations[idx] };
      return ar.load(std::memory_order_acquire);
    } else {
      return generations[idx];
    }
  }

  auto store_generation(std::uint32_t idx, std::uint32_t v) -> void
  {
    if constexpr (LockFree) {
      std::atomic_ref<std::uint32_t> ar{ generations[idx] };
      ar.store(v, std::memory_order_release);
    } else {
      generations[idx] = v;
    }
  }

  std::uint32_t reserved{ 0 };
  std::vector<TImpl> dense_storage;
  std::vector<std::uint32_t> sparse_to_dense;
  std::vector<std::uint32_t> dense_to_sparse;
  std::vector<std::uint32_t> generations;
  Freelist freelist;
};

using TexturePool = Pool<TextureHandle, VulkanTextureND>;
using BufferPool = Pool<BufferHandle, VulkanDeviceBuffer>;

}
