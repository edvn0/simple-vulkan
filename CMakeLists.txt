cmake_minimum_required(VERSION 3.31)
project("simple-vulkan" CXX)

include(CTest)
include(CheckCXXSourceCompiles)

if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

check_cxx_source_compiles("
#include <expected>
int main() {
    std::expected<int, int> v{42};
    return v.value();
}
" HAS_STD_EXPECTED)

option(BUILD_TESTING "Should build tests" ON)

add_subdirectory(lib)


add_executable(simple-vulkan "simple-vulkan.cpp")
target_link_libraries(simple-vulkan PRIVATE sv)
set_target_properties(simple-vulkan PROPERTIES CXX_STANDARD 23)
target_compile_options(simple-vulkan PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /EHsc)
