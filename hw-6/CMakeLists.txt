cmake_minimum_required(VERSION 3.14)
project(event_driven_architecture CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(userver COMPONENTS rabbitmq REQUIRED)

add_executable(event_producer src/producer.cpp)
target_link_libraries(event_producer userver::rabbitmq)

add_executable(event_consumer src/consumer.cpp)
target_link_libraries(event_consumer userver::rabbitmq)
