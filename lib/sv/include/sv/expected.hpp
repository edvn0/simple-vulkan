#pragma once

#ifdef HAS_STD_EXPECTED
#include <expected>
#else
#include <tl/expected.hpp>
#endif

namespace sv {

#ifdef HAS_STD_EXPECTED
template<typename T, typename Err>
using Expected = std::expected<T, Err>;
template<typename T>
using unexpected = std::unexpected<T>;
#else
template<typename T, typename Err>
using Expected = tl::expected<T, Err>;
template<typename T>
using unexpected = tl::unexpected<T>;
#endif

} // namespace VkBindless