cmake_minimum_required(VERSION 3.31)

project(simple-vulkan LANGUAGES CXX)

include(CTest)
include(CheckCXXSourceCompiles)
include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  if(MSVC)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<IF:$<CONFIG:Debug,RelWithDebInfo>,EditAndContinue,ProgramDatabase>")
  endif()
endif()

check_cxx_source_compiles("
#include <expected>
int main(){ std::expected<int,int> v{42}; return v.value(); }
" HAS_STD_EXPECTED)

option(BUILD_TESTING "Should build tests" ON)

function(enable_sane_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /EHsc /Zc:__cplusplus)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

FetchContent_Declare(
  vulkan_headers
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
  GIT_TAG main
)
FetchContent_MakeAvailable(vulkan_headers)

FetchContent_Declare(
  vkbootstrap
  GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
  GIT_TAG main
)
set(VK_BOOTSTRAP_TEST OFF CACHE INTERNAL "")
set(VK_BOOTSTRAP_INSTALL OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(vkbootstrap)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG 3.4
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

FetchContent_Declare(
  glslang
  GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
  GIT_TAG main
)
set(ENABLE_OPT OFF CACHE INTERNAL "")
set(GLSLANG_TESTS_DEFAULT OFF CACHE INTERNAL "")
set(GLSLANG_ENABLE_INSTALL_DEFAULT OFF CACHE INTERNAL "")
set(BUILD_EXTERNAL OFF CACHE INTERNAL "")
set(ENABLE_GLSLANG_BINARIES OFF CACHE INTERNAL "")
set(ENABLE_HLSL OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(glslang)

FetchContent_Declare(
  spirv-reflect
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
  GIT_TAG main
)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE INTERNAL "")
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(spirv-reflect)

FetchContent_Declare(
  tracy
  GIT_REPOSITORY https://github.com/wolfpld/tracy.git
  GIT_TAG master
)
FetchContent_MakeAvailable(tracy)

FetchContent_Declare(
  tinyobj
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG release
)
set(TINYOBJLOADER_USE_DOUBLE ON CACHE INTERNAL "")
FetchContent_MakeAvailable(tinyobj)

find_package(Vulkan REQUIRED)

FetchContent_Populate(imgui
  URL https://github.com/ocornut/imgui/archive/docking.zip
  SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/imgui
)
set(imgui_dir ${CMAKE_CURRENT_BINARY_DIR}/imgui)

add_library(imgui_glfw STATIC
  ${imgui_dir}/imgui.cpp
  ${imgui_dir}/imgui_draw.cpp
  ${imgui_dir}/imgui_demo.cpp
  ${imgui_dir}/imgui_tables.cpp
  ${imgui_dir}/imgui_widgets.cpp
  ${imgui_dir}/backends/imgui_impl_glfw.cpp
  ${imgui_dir}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui_glfw PUBLIC ${imgui_dir} ${imgui_dir}/backends)
target_link_libraries(imgui_glfw PUBLIC glfw Vulkan::Vulkan)
# enable_sane_warnings(imgui_glfw)

add_subdirectory(lib)

add_executable(simple-vulkan simple-vulkan.cpp)
target_link_libraries(simple-vulkan PRIVATE sv)
target_compile_features(simple-vulkan PRIVATE cxx_std_23)
enable_sane_warnings(simple-vulkan)

set(shader_dir ${CMAKE_SOURCE_DIR}/shaders)
set(shader_output_dir ${CMAKE_BINARY_DIR}/shaders)
add_custom_command(
  TARGET simple-vulkan POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${shader_dir} ${shader_output_dir}
)
