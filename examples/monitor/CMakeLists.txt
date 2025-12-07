find_package(bsl REQUIRED)
find_package(bal REQUIRED)
find_package(quill CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS beast)
find_package(CLI11 CONFIG REQUIRED)

add_executable(rmqmonitor
    monitor.m.cpp
)

target_link_libraries(rmqmonitor PUBLIC
    rmq
    bsl
    bal
    quill::quill
    Boost::beast
    CLI11::CLI11
)

target_compile_definitions(rmqmonitor PRIVATE USES_LIBRMQ_EXPERIMENTAL_FEATURES)
