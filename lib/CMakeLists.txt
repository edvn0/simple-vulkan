cmake_minimum_required(VERSION 3.21)
project(simplevk)

include(FetchContent)

# Vulkan-Headers (latest from repo — consider pinning to a tag for reproducibility)
FetchContent_Declare(
  vulkan_headers
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(vulkan_headers)

# vk-bootstrap; make it use the headers above
FetchContent_Declare(
  vkbootstrap
  GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
  GIT_TAG        main
)
# Important: tell vk-bootstrap where the headers are
set(VK_BOOTSTRAP_TEST OFF CACHE INTERNAL "")
set(VK_BOOTSTRAP_INSTALL OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(vkbootstrap)

# GLFW (unchanged)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG        3.4
)
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")
set(GLFW_INSTALL OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(glfw)

# If you still want SDK tools like glslang:
find_package(Vulkan REQUIRED COMPONENTS glslang)

file(GLOB_RECURSE sv_sources CONFIGURE_DEPENDS "*.cpp" "*.hpp")
add_library(sv STATIC ${sv_sources})
target_include_directories(sv PUBLIC "sv/include")

# Make your target include the same headers to avoid mixing versions
target_include_directories(sv PRIVATE "${vulkan_headers_SOURCE_DIR}/include")

set_target_properties(sv PROPERTIES CXX_STANDARD 23)
target_compile_options(sv PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /EHsc)

target_link_libraries(sv
  PUBLIC
    glfw
    Vulkan::Vulkan
    Vulkan::glslang
    vk-bootstrap
)
