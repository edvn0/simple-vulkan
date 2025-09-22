file(GLOB_RECURSE sv_sources CONFIGURE_DEPENDS "sv/src/*.cpp" "sv/include/*.hpp")

add_library(sv STATIC ${sv_sources})
set_target_properties(sv PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(sv PUBLIC cxx_std_23)
target_include_directories(sv
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${vulkan_headers_SOURCE_DIR}/include
)
target_precompile_headers(sv PUBLIC sv/include/sv/svpch.hpp)

set(simple_vulkan_compile_defs
  GLM_FORCE_DEPTH_ZERO_TO_ONE
  GLM_FORCE_LEFT_HANDED
  HAS_TRACY_TRACING
)
if(HAS_STD_EXPECTED)
  list(APPEND simple_vulkan_compile_defs HAS_STD_EXPECTED)
endif()
target_compile_definitions(sv PUBLIC ${simple_vulkan_compile_defs})

target_link_libraries(sv
  PUBLIC
    glfw
    Vulkan::Vulkan
    glslang::glslang
    spirv-reflect-static
    TracyClient
    vk-bootstrap::vk-bootstrap
    glm::glm
    GPUOpen::VulkanMemoryAllocator
    imgui_glfw
)

function(enable_sane_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /EHsc /Zc:__cplusplus)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()
enable_sane_warnings(sv)

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG master
  )
  FetchContent_MakeAvailable(doctest)
  include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

  add_executable(sv_tests sv/tests/main.cpp sv/tests/object_pool_tests.cpp)
  target_link_libraries(sv_tests PRIVATE sv doctest::doctest)
  target_include_directories(sv_tests PRIVATE ${doctest_INCLUDE_DIR})
  target_compile_features(sv_tests PRIVATE cxx_std_23)
  enable_sane_warnings(sv_tests)
  doctest_discover_tests(sv_tests)
endif()
