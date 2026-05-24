cmake_minimum_required(VERSION 3.14)
project(profi_services_api CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(userver COMPONENTS postgresql redis REQUIRED)

add_executable(profi_service src/main.cpp)
target_link_libraries(profi_service userver::postgresql userver::redis)
