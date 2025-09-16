#include "doctest/doctest.h"
#include "sv/object_pool.hpp"
#include <string>

using namespace sv;

struct Dummy
{
  int v{};
  std::string s;
  Dummy() = default;
  Dummy(int a, std::string b)
    : v(a)
    , s(std::move(b))
  {
  }
};
using DummyHandle = Handle<Dummy>;
template<bool LF = false>
using DummyPool = Pool<DummyHandle, Dummy, LF>;

TEST_CASE("default_handle_is_invalid_generation_zero")
{
  DummyHandle h{};
  CHECK(h.empty());
  CHECK(h.generation() == invalid_generation);
}

TEST_CASE("allocate_sets_generation_gt_zero_even_for_index_zero")
{
  DummyPool<> pool;
  auto h0 = pool.emplace(1, "a"); // first allocation should get index 0
  CHECK(h0.index() == 0U);
  CHECK(h0.generation() != invalid_generation);
  CHECK(pool.is_valid(h0));
}

TEST_CASE("erase_bumps_generation_and_invalidates_old_handle")
{
  DummyPool<> pool;
  auto h = pool.emplace(7, "x");
  auto idx = h.index();
  auto gen = h.generation();
  CHECK(pool.erase(h));
  CHECK_FALSE(pool.is_valid(h));
  auto h2 = pool.emplace(8, "y");
  CHECK(h2.index() == idx);
  CHECK(h2.generation() != gen);
  CHECK(h2.generation() != invalid_generation);
}

TEST_CASE("dense_compaction_on_middle_erase")
{
  DummyPool<> pool;
  auto h1 = pool.emplace(1, "a");
  auto h2 = pool.emplace(2, "b");
  auto h3 = pool.emplace(3, "c");
  CHECK(pool.erase(h2));
  CHECK(pool.size() == 2);
  CHECK(pool.is_valid(h1));
  CHECK(pool.is_valid(h3));
}

TEST_CASE("clear_invalidates_all_handles")
{
  DummyPool<> pool;
  auto h1 = pool.emplace(1, "a");
  auto h2 = pool.emplace(2, "b");
  pool.clear();
  CHECK_FALSE(pool.is_valid(h1));
  CHECK_FALSE(pool.is_valid(h2));
  CHECK(pool.size() == 0);
  auto h3 = pool.emplace(3, "c");
  CHECK(pool.is_valid(h3));
  CHECK(h3.generation() != invalid_generation);
}

TEST_CASE("lockfree_mode_reuses_indices_and_never_uses_generation_zero")
{
  DummyPool<true> pool;
  auto h1 = pool.emplace(10, "aa");
  auto h2 = pool.emplace(20, "bb");
  CHECK(h1.generation() != invalid_generation);
  CHECK(h2.generation() != invalid_generation);
  CHECK(pool.erase(h1));
  CHECK_FALSE(pool.is_valid(h1));
  auto h3 = pool.emplace(30, "cc");
  CHECK(h3.index() == h1.index());
  CHECK(h3.generation() != h1.generation());
  CHECK(h3.generation() != invalid_generation);
}

TEST_CASE("get_returns_null_for_invalid_generation_zero")
{
  DummyPool<> pool;
  DummyHandle invalid{};
  CHECK(pool.get(invalid) == nullptr);
  auto h = pool.emplace(1, "x");
  CHECK(pool.get(h) != nullptr);
  pool.erase(h);
  CHECK(pool.get(h) == nullptr);
}
