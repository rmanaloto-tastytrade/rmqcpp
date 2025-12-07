add_executable(rmq_order_status_monitor_cli
    main.cpp
    config.cpp
)

target_link_libraries(rmq_order_status_monitor_cli PRIVATE
    rmq
    bsl
    CLI11::CLI11
    quill::quill
)

target_include_directories(rmq_order_status_monitor_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/rmq/rmqio
)

set_target_properties(rmq_order_status_monitor_cli PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
)

# Glaze is header-only; vcpkg provides the include path via the package.
find_path(GLAZE_INCLUDE_DIRS "glaze/glaze.hpp")
if(GLAZE_INCLUDE_DIRS)
    target_include_directories(rmq_order_status_monitor_cli PRIVATE ${GLAZE_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "glaze not found in include paths")
endif()

find_package(quill CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(bsl CONFIG REQUIRED)
