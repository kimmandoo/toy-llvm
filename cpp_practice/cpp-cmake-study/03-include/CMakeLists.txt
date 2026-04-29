cmake_minimum_required(VERSION 3.16)

project(IncludeExample)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(app
    src/main.cpp
    src/calculator.cpp
)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)