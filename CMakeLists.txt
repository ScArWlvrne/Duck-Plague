cmake_minimum_required(VERSION 3.21)

project(DuckPlague LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(DuckPlague
    controller.cpp
    trojan.cpp
    encrypt.cpp
    educate.cpp
    restore.cpp
)

target_link_libraries(DuckPlague PRIVATE Qt6::Widgets)