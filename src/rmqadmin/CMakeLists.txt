find_package(Boost REQUIRED COMPONENTS beast)
find_package(CLI11 CONFIG REQUIRED)
find_package(bsl REQUIRED)
find_package(quill CONFIG REQUIRED)

add_library(rmqadmin_core STATIC core.cpp)
target_include_directories(rmqadmin_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rmqadmin_core PUBLIC bsl)

add_library(rmqadmin_amqp STATIC amqp.cpp)
target_include_directories(rmqadmin_amqp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(rmqadmin_amqp PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/rmq
    ${PROJECT_SOURCE_DIR}/src/rmq/rmqio)
target_link_libraries(rmqadmin_amqp PUBLIC rmqadmin_core rmq)

add_library(rmqadmin_http STATIC http_client.cpp)
target_include_directories(rmqadmin_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rmqadmin_http PUBLIC rmqadmin_core Boost::beast OpenSSL::SSL OpenSSL::Crypto quill::quill)

add_library(rmqadmin_cli STATIC cli.cpp)
target_include_directories(rmqadmin_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rmqadmin_cli PUBLIC rmqadmin_core CLI11::CLI11)

add_library(rmqadmin_render STATIC render.cpp)
target_include_directories(rmqadmin_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rmqadmin_render PUBLIC rmqadmin_core quill::quill)

add_library(rmqadmin_exec STATIC exec.cpp)
target_include_directories(rmqadmin_exec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rmqadmin_exec PUBLIC rmqadmin_core rmqadmin_http rmqadmin_amqp rmqadmin_render)

add_executable(rmqadmin main.cpp)
target_link_libraries(rmqadmin PRIVATE rmqadmin_cli rmqadmin_exec quill::quill)

install(TARGETS rmqadmin)
