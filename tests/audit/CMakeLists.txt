cmake_minimum_required(VERSION 3.16)
project(V2VAudit LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(pugixml CONFIG REQUIRED)
get_filename_component(V2V_ROOT "../.." ABSOLUTE)
add_executable(V2VAudit
    audit.cpp
    ${V2V_ROOT}/src/OsmParser.cpp
    ${V2V_ROOT}/src/RoadGraph.cpp
    ${V2V_ROOT}/src/InterferenceGraph.cpp
    ${V2V_ROOT}/src/GraphColoring.cpp
    ${V2V_ROOT}/src/VehicleSimulation.cpp
    ${V2V_ROOT}/src/MapView.cpp
    ${V2V_ROOT}/src/MapView.h
    ${V2V_ROOT}/src/TileLayer.cpp
    ${V2V_ROOT}/src/TileLayer.h)
target_include_directories(V2VAudit PRIVATE ${V2V_ROOT}/src)
target_link_libraries(V2VAudit PRIVATE Qt6::Widgets Qt6::Network pugixml::pugixml)
target_compile_definitions(V2VAudit PRIVATE AUDIT_DATA_DIR="${V2V_ROOT}/data")
if(MSVC)
    target_compile_options(V2VAudit PRIVATE /Zc:__cplusplus /permissive- /utf-8)
endif()
option(AUDIT_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
if(AUDIT_SANITIZERS)
    target_compile_options(V2VAudit PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(V2VAudit PRIVATE -fsanitize=address,undefined)
endif()
enable_testing()
add_test(NAME core COMMAND V2VAudit core)
add_test(NAME reload_map COMMAND V2VAudit reload)
add_test(NAME gui_workflow COMMAND V2VAudit workflow)
set_tests_properties(core reload_map gui_workflow PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ASAN_OPTIONS=detect_leaks=0")
