cmake_minimum_required(VERSION 3.21)
project(mix_photo_export DESCRIPTION "Export Picture It! MIX and FlashPix images to JPEG" LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32 AND NOT CYGWIN)
    message(FATAL_ERROR "Use Cygwin's CMake and GCC on Windows; native MSVC/MinGW builds are not supported.")
endif()

find_path(TURBOJPEG_INCLUDE_DIR turbojpeg.h REQUIRED)
find_library(TURBOJPEG_LIBRARY NAMES turbojpeg REQUIRED)
add_library(TurboJPEG::TurboJPEG UNKNOWN IMPORTED)
set_target_properties(TurboJPEG::TurboJPEG PROPERTIES
    IMPORTED_LOCATION "${TURBOJPEG_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${TURBOJPEG_INCLUDE_DIR}")

include(FetchContent)
include(ExternalProject)
FetchContent_Declare(libfpx
    GIT_REPOSITORY https://github.com/ImageMagick/libfpx.git
    GIT_TAG bb28e42245701ef5473ac086e06d505491ad3d8a
)
FetchContent_MakeAvailable(libfpx)
find_program(MAKE_PROGRAM make REQUIRED)
ExternalProject_Add(fpx_build
    SOURCE_DIR "${libfpx_SOURCE_DIR}"
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E env
        "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}"
        "CXXFLAGS=-O2 -std=gnu++98"
        <SOURCE_DIR>/configure --disable-shared --enable-static
    BUILD_COMMAND "${MAKE_PROGRAM}" -j4
    BUILD_IN_SOURCE TRUE
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS "${libfpx_SOURCE_DIR}/.libs/libfpx.a"
)

add_library(fpx STATIC IMPORTED)
set_target_properties(fpx PROPERTIES IMPORTED_LOCATION "${libfpx_SOURCE_DIR}/.libs/libfpx.a")
add_dependencies(fpx fpx_build)
target_compile_definitions(fpx INTERFACE _UNIX HAVE_CONFIG_H)
target_include_directories(fpx SYSTEM INTERFACE
    "${libfpx_SOURCE_DIR}" "${libfpx_SOURCE_DIR}/basics"
    "${libfpx_SOURCE_DIR}/oless" "${libfpx_SOURCE_DIR}/oless/h"
    "${libfpx_SOURCE_DIR}/ole" "${libfpx_SOURCE_DIR}/fpx"
    "${libfpx_SOURCE_DIR}/ri_image"
)

add_executable(mix-photo-export decode_mix.cpp storage.cpp)
target_compile_features(mix-photo-export PRIVATE cxx_std_17)
target_compile_options(mix-photo-export PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(mix-photo-export PRIVATE fpx TurboJPEG::TurboJPEG)
install(TARGETS mix-photo-export RUNTIME DESTINATION bin)

include(CTest)
set(MIX_TEST_INPUT "" CACHE PATH "Optional folder of MIX files for real-image regression tests")
if(BUILD_TESTING)
    add_executable(mix-fixtures tests/fixtures.cpp storage.cpp)
    target_compile_features(mix-fixtures PRIVATE cxx_std_17)
    target_compile_definitions(mix-fixtures PRIVATE MIX_TESTING)
    target_compile_options(mix-fixtures PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(mix-fixtures PRIVATE fpx TurboJPEG::TurboJPEG)
    add_test(NAME regression COMMAND "${CMAKE_COMMAND}"
        "-DAPP=$<TARGET_FILE:mix-photo-export>"
        "-DFIXTURES=$<TARGET_FILE:mix-fixtures>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/regression"
        "-DPHOTO_DIR=${MIX_TEST_INPUT}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/regression.cmake")
    set_tests_properties(regression PROPERTIES TIMEOUT 180)
endif()