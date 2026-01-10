find_package(Threads REQUIRED) # CMake 3.26-rc3 Bug https://gitlab.kitware.com/cmake/cmake/-/issues/24505
find_package(ZLIB REQUIRED)
if (ENABLE_COMPRESSION)
    find_package(zstd CONFIG REQUIRED)
endif()
find_package(Boost REQUIRED)
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
# Use CMake's built-in GTest module; vcpkg installs headers/libs but no config files.
find_package(GTest REQUIRED)

# BDE-required subpackages
find_package(bal REQUIRED)

# The BDE targets occasionally drag Linux-only link flags (librt/libstdc++)
# via their interface libraries. Scrub those on macOS so we don't try to link
# unavailable system libs.
if(APPLE)
    foreach(bde_target IN ITEMS bal bdl bsl inteldfp pcre2 ryu)
        if(TARGET ${bde_target})
            # Drop Linux-only link flags (rt/libstdc++) from BDE interface
            # libraries while keeping the real dependencies (pcre2, inteldfp, etc).
            get_target_property(_bde_iface ${bde_target} INTERFACE_LINK_LIBRARIES)
            if(_bde_iface)
                set(_bde_pruned "${_bde_iface}")
                list(FILTER _bde_pruned EXCLUDE REGEX "rt|stdc\\+\\+")
                set_property(TARGET ${bde_target} PROPERTY INTERFACE_LINK_LIBRARIES "${_bde_pruned}")
            endif()
        endif()
    endforeach()
endif()

add_subdirectory(rmq)
add_subdirectory(rmqtestmocks)
add_subdirectory(rmqadmin)
add_subdirectory(tests)
