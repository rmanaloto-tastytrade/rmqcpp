find_package(bsl REQUIRED)
find_package(bal REQUIRED)

add_executable(rmqperftest
    rmqperftest.m.cpp
    rmqperftest_args.cpp
    rmqperftest_consumerargs.cpp
    rmqperftest_runner.cpp
)

target_compile_definitions(rmqperftest PRIVATE USES_LIBRMQ_EXPERIMENTAL_FEATURES)

target_link_libraries(rmqperftest PUBLIC
    bsl
    bal
    rmq
    rmqintegration
)
target_include_directories(rmqperftest PRIVATE .)

# BDE configs sometimes inject Linux-only rt/libstdc++ flags; strip them on macOS
# so the example links against libc++ cleanly.
if(APPLE)
    foreach(bde_target IN ITEMS bal bdl bsl inteldfp pcre2 ryu)
        if(TARGET ${bde_target})
            get_target_property(_bde_iface ${bde_target} INTERFACE_LINK_LIBRARIES)
            if(_bde_iface)
                set(_bde_pruned "${_bde_iface}")
                list(FILTER _bde_pruned EXCLUDE REGEX "rt|stdc\\+\\+")
                set_property(TARGET ${bde_target} PROPERTY INTERFACE_LINK_LIBRARIES "${_bde_pruned}")
            endif()
        endif()
    endforeach()
endif()

install(
    TARGETS rmqperftest
    DESTINATION bin
    COMPONENT rmqtools
)
