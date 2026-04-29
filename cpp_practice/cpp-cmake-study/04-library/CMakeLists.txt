cmake_minimum_required(VERSION 3.16)

project(LibraryExample)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(calculator
    src/calculator.cpp
)

target_include_directories(calculator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(app
    src/main.cpp
)

target_link_libraries(app PRIVATE
    calculator
)