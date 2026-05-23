cmake_minimum_required(VERSION 3.14)
project(warehouse_inventory_api CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(userver COMPONENTS core REQUIRED)

add_executable(warehouse_service src/main.cpp)
target_link_libraries(warehouse_service userver::core)
