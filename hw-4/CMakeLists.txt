cmake_minimum_required(VERSION 3.14)
project(messenger_api CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(userver COMPONENTS mongo REQUIRED)

add_executable(messenger_service src/main.cpp)
target_link_libraries(messenger_service userver::core userver::mongo)
