cmake_minimum_required (VERSION 3.31)
project(simplevk)

include(FetchContent)

FetchContent_Declare(
  vulkan_headers
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(vulkan_headers)

FetchContent_Declare(
  vkbootstrap
  GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
  GIT_TAG        main
)
set(VK_BOOTSTRAP_TEST OFF CACHE INTERNAL "")
set(VK_BOOTSTRAP_INSTALL OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(vkbootstrap)

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

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)
set(GLM_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLM_BUILD_INSTALL OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
  vma
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG master
)
FetchContent_MakeAvailable(vma)

find_package(Vulkan REQUIRED COMPONENTS glslang)

file(GLOB_RECURSE sv_sources CONFIGURE_DEPENDS "sv/src/*.cpp" "sv/include/*.hpp")
add_library(sv STATIC ${sv_sources})
target_precompile_headers(sv PUBLIC sv/include/sv/svpch.hpp)
target_include_directories(sv PUBLIC "sv/include" PRIVATE "${vulkan_headers_SOURCE_DIR}/include")
set_target_properties(sv PROPERTIES CXX_STANDARD 23)
target_compile_options(sv PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /EHsc)

target_compile_definitions(sv PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_LEFT_HANDED)
target_link_libraries(sv
  PUBLIC
    glfw
    Vulkan::Vulkan
    Vulkan::glslang
    vk-bootstrap::vk-bootstrap
    glm::glm
    GPUOpen::VulkanMemoryAllocator
)

if (BUILD_TESTING)
  FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG master
    )
    FetchContent_MakeAvailable(doctest)
    include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
    
    add_executable(sv_tests sv/tests/main.cpp sv/tests/object_pool_tests.cpp)
    doctest_discover_tests(sv_tests)
    set_target_properties(sv_tests PROPERTIES CXX_STANDARD 23)
    target_compile_options(sv_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /EHsc)
    target_link_libraries(sv_tests PRIVATE sv doctest::doctest)
    target_include_directories(sv_tests PRIVATE ${doctest_INCLUDE_DIR})
endif()