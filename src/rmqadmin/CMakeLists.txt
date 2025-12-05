find_package(Boost REQUIRED COMPONENTS beast)
find_package(CLI11 CONFIG REQUIRED)

add_library(rmqadmin_core STATIC core.cpp)
target_include_directories(rmqadmin_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(rmqadmin_http STATIC http.cpp)
target_include_directories(rmqadmin_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rmqadmin_http PUBLIC rmqadmin_core Boost::beast OpenSSL::SSL OpenSSL::Crypto)

add_library(rmqadmin_cli STATIC cli.cpp)
target_include_directories(rmqadmin_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rmqadmin_cli PUBLIC rmqadmin_core CLI11::CLI11)

add_library(rmqadmin_render STATIC render.cpp)
target_include_directories(rmqadmin_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rmqadmin_render PUBLIC rmqadmin_core)

add_library(rmqadmin_exec STATIC exec.cpp)
target_include_directories(rmqadmin_exec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rmqadmin_exec PUBLIC rmqadmin_core rmqadmin_http rmqadmin_render)

add_executable(rmqadmin main.cpp)
target_link_libraries(rmqadmin PRIVATE rmqadmin_cli rmqadmin_exec)

install(TARGETS rmqadmin)
