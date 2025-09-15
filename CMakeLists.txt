# CMakeList.txt : CMake project for simple-vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("simple-vulkan")

# Add source to this project's executable.
add_subdirectory(lib)

add_executable (simple-vulkan "simple-vulkan.cpp" "simple-vulkan.h")
target_link_libraries(simple-vulkan PRIVATE sv)
set_target_properties(simple-vulkan PROPERTIES CXX_STANDARD 23)
target_compile_options(simple-vulkan PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /EHsc)

# TODO: Add tests and install targets if needed.
