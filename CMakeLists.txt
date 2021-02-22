cmake_minimum_required(VERSION 3.19)
project(ADSiteParsing)

set(CMAKE_CXX_STANDARD 17)
set(CURL_LIBRARY "libcurl")
set(CURL_INCLUDE_DIR "vendor/cmake/include")

find_package(CURL REQUIRED)

include_directories(${CURL_INCLUDE_DIRS})
add_executable(ADSiteParsing main.cpp)
target_link_libraries(ADSiteParsing ${CURL_LIBRARIES})