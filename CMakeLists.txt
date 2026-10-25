cmake_minimum_required(VERSION 3.10)
project(HttpServer)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# include_directories(include)
# file(GLOB SOURCES "src/*.cpp")
# add_executable(http_server ${SOURCES})


file(GLOB SERVER_SOURCES "src/*.cpp")
list(REMOVE_ITEM SERVER_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")
add_library(http_server_lib ${SERVER_SOURCES})
target_include_directories(http_server_lib PUBLIC include)

add_executable(http_server src/main.cpp)
target_link_libraries(http_server http_server_lib)


include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(http_tests tests/parser_test.cpp tests/response_test.cpp)

target_link_libraries(http_tests http_server_lib GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(http_tests)