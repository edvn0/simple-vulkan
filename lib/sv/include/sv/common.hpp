#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace sv {

template<typename T>
inline auto
make_error(typename T::Code c, std::string_view message) -> std::unexpected<T>
{
  return std::unexpected(T{ c, std::string{ message } });
}

template<typename T>
inline auto
make_error(typename T::Code c, const char* message) -> std::unexpected<T>
{
  return std::unexpected(T{ c, std::string{ message } });
}

template<typename T>
inline auto
make_error(const T& err) -> std::unexpected<T>
{
  return std::unexpected(err);
}

}