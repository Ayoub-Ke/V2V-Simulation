cmake_minimum_required(VERSION 3.16)
project(V2VSimulation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# IMPORTANT : adapte ce chemin à ta version exacte de Qt installée (MSVC, pas MinGW)
set(CMAKE_PREFIX_PATH "C:/Qt/6.11.2/msvc2022_64")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(V2VSimulation
    src/main.cpp
    src/MapView.cpp
    src/MapView.h
)

target_link_libraries(V2VSimulation PRIVATE Qt6::Widgets)
target_include_directories(V2VSimulation PRIVATE src)
