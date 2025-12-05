cmake_minimum_required(VERSION 3.25)
project(rmq LANGUAGES C CXX)

message("[PATH:$ENV{PATH}]")

# Silence FindBoost CMP0167 warning when using the vcpkg wrapper.
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

option(ENABLE_COMPRESSION "Enable zstd compression support" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
enable_testing()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Honor the preset-provided BDE_BUILD_TARGET_CPP17 cache entry to keep tool
# invocations warning-free and aligned with the requested C++ standard.
if(DEFINED BDE_BUILD_TARGET_CPP17)
    message(STATUS "Using BDE_BUILD_TARGET_CPP17=${BDE_BUILD_TARGET_CPP17}")
    set(CMAKE_CXX_STANDARD
        ${BDE_BUILD_TARGET_CPP17}
        CACHE STRING "C++ standard (kept in sync with BDE target)" FORCE)
endif()

if(DEFINED BDE_BUILD_TARGET_SAFE)
    message(STATUS "Using BDE_BUILD_TARGET_SAFE=${BDE_BUILD_TARGET_SAFE}")
endif()

# C++20+ removes several deprecated std symbols that older BDE releases still
# alias. Guard them out explicitly for C++23/C++26 builds.
if(CMAKE_CXX_STANDARD GREATER_EQUAL 23)
    add_compile_definitions(
        BSLS_LIBRARYFEATURES_HAS_CPP17_DEPRECATED_REMOVED
        BSLS_LIBRARYFEATURES_HAS_CPP20_DEPRECATED_REMOVED)
endif()

# Fail fast if libc++ is not in use when requested by the preset flags.
include(CheckCXXSourceCompiles)
set(_old_req_flags "${CMAKE_REQUIRED_FLAGS}")
set(_old_req_libs "${CMAKE_REQUIRED_LIBRARIES}")
set(_old_req_link_opts "${CMAKE_REQUIRED_LINK_OPTIONS}")
set(LIBCXX_PREFIX "/usr/lib/llvm-22" CACHE PATH "Prefix for libc++ installation")

set(_libcxx_include "")
find_path(
    _libcxx_include
    NAMES cstddef
    PATH_SUFFIXES c++/v1
    PATHS
        "${LIBCXX_PREFIX}/include"
        "${LIBCXX_PREFIX}/bin/../include"
        "/usr/include"
)
# Fallback to well-known Clang layout if find_path did not locate headers.
if(NOT _libcxx_include)
    if(EXISTS "${LIBCXX_PREFIX}/include/c++/v1")
        set(_libcxx_include "${LIBCXX_PREFIX}/include/c++/v1")
    elseif(EXISTS "${LIBCXX_PREFIX}/bin/../include/c++/v1")
        set(_libcxx_include "${LIBCXX_PREFIX}/bin/../include/c++/v1")
    endif()
endif()

set(_libcxx_libdir "")
find_library(
    _libcxx_lib
    NAMES c++
    PATHS
        "${LIBCXX_PREFIX}/lib"
        "${LIBCXX_PREFIX}/lib/x86_64-linux-gnu"
        "/usr/lib/llvm-22/lib"
        "/usr/lib/llvm-22/lib/x86_64-linux-gnu"
        "/usr/lib/x86_64-linux-gnu"
)
if(_libcxx_lib)
    get_filename_component(_libcxx_libdir "${_libcxx_lib}" DIRECTORY)
endif()

if(_libcxx_include)
    list(APPEND CMAKE_REQUIRED_FLAGS "-isystem${_libcxx_include}")
endif()
list(APPEND CMAKE_REQUIRED_FLAGS "-stdlib=libc++")
if(_libcxx_libdir)
    list(APPEND CMAKE_REQUIRED_LINK_OPTIONS "-L${_libcxx_libdir}" "-Wl,-rpath,${_libcxx_libdir}")
endif()
set(CMAKE_REQUIRED_LIBRARIES c++ c++abi)
check_cxx_source_compiles("
#ifndef _LIBCPP_VERSION
# error \"libc++ not detected (expected with preset)\"
#endif
int main() { return 0; }
" RMQCPP_USE_LIBCXX_OK)
set(CMAKE_REQUIRED_FLAGS "${_old_req_flags}")
set(CMAKE_REQUIRED_LIBRARIES "${_old_req_libs}")
set(CMAKE_REQUIRED_LINK_OPTIONS "${_old_req_link_opts}")
if(NOT RMQCPP_USE_LIBCXX_OK)
    message(WARNING "Preset requires libc++; compiler is not using libc++.\n"
                        "Install libc++/libc++abi dev packages (e.g. libc++-22-dev libc++abi-22-dev) and ensure "
                        "the libc++ include/lib directories are on the search path (try -DLIBCXX_PREFIX=/usr/lib/llvm-22), "
                        "or drop -stdlib=libc++.\n"
                        "Probe info: include='${_libcxx_include}', lib='${_libcxx_lib}', libdir='${_libcxx_libdir}'.")
endif()

add_subdirectory(src)
add_subdirectory(examples)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "SunPro")
    # _RWSTD_ALLOCATOR tells the solaris <memory> header to define a std::allocator
    # which conforms better to the C++ standard, which is expected by Boost. Without
    # this, the library does not build due to missing std::allocator<..>::rebind

    # "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__" tells the solaris sys/socket.h to conform
    # better to what boost asio expects (to have a msg_flags member)
    target_compile_definitions(rmq PRIVATE _RWSTD_ALLOCATOR "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__")

    # Solaris doesn't pull these symbols in automatically
    # Libraries such as bte set this manually (we don't depend on bte)
    target_link_options(rmq PUBLIC -lnsl -lsocket)

    set(PKG_LIBS "-lnsl -lsocket")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "XL")
    # The ibm/xl compiler does not appreciate this boost feature
    target_compile_definitions(rmq PRIVATE BOOST_ASIO_DISABLE_HANDLER_TYPE_REQUIREMENTS)
else()
    # In general the sunpro/xl warnings are not worth the bother
    set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
endif()
