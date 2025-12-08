add_executable(rmq_order_status_monitor_cli
    main.cpp
    config.cpp
)

target_link_libraries(rmq_order_status_monitor_cli PRIVATE
    rmq
    bsl
    CLI11::CLI11
    quill::quill
    fmt::fmt
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
find_package(fmt CONFIG REQUIRED)
find_package(opentelemetry-cpp CONFIG)
find_package(Python3 COMPONENTS Interpreter)

if(opentelemetry-cpp_FOUND)
    target_link_libraries(rmq_order_status_monitor_cli PRIVATE
        opentelemetry-cpp::sdk
        opentelemetry-cpp::otlp_http_exporter
        opentelemetry-cpp::otlp_http_metric_exporter
    )
    target_compile_definitions(rmq_order_status_monitor_cli PRIVATE OSMCLI_HAVE_OTEL=1 OSMCLI_HAVE_OTEL_GRPC=0 OSMCLI_HAVE_OTEL_HTTP=1)
else()
    message(STATUS "opentelemetry-cpp not found; OTel disabled")
endif()

if(Python3_Interpreter_FOUND)
    add_custom_target(order_status_monitor_cli_diff
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_overview.py
                --config ${CMAKE_CURRENT_SOURCE_DIR}/config.local.json
                --logs-dir ${CMAKE_CURRENT_BINARY_DIR}/logs
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Diff /api/overview (curl) vs latest http_overview-*.json")
else()
    message(STATUS "Python3 interpreter not found; diff target disabled")
endif()
