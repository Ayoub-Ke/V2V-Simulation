cmake_minimum_required(VERSION 3.16)
project(V2VSimulation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Chemin Qt en dur uniquement utile sous Windows (Visual Studio) ; sous Linux (Docker),
# Qt est installé via apt et trouvé automatiquement par find_package sans ce chemin.
if (WIN32)
    list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.11.2/msvc2022_64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(pugixml CONFIG REQUIRED)

add_executable(V2VSimulation
    src/main.cpp
    src/MapView.cpp
    src/MapView.h
    src/OsmParser.cpp
    src/OsmParser.h
    src/RoadGraph.cpp
    src/RoadGraph.h
    src/TileLayer.cpp
    src/TileLayer.h
    src/Vehicle.h
    src/VehicleSimulation.cpp
    src/VehicleSimulation.h
    src/WebMercator.h
    src/InterferenceGraph.cpp
    src/InterferenceGraph.h
    src/GraphColoring.cpp
    src/GraphColoring.h
    src/HistoryChart.cpp
    src/HistoryChart.h
    src/resources.qrc
)

if (MSVC)
    # Indispensable avec Qt6 : sans ce flag, MSVC ne rapporte pas correctement __cplusplus,
    # ce qui fait échouer la détection de fonctionnalités C++17 dans les headers Qt
    # (symptôme typique : erreurs de syntaxe dans qregularexpression.h alors que le bug n'y est pas).
    target_compile_options(V2VSimulation PRIVATE /Zc:__cplusplus /permissive- /utf-8)
endif()

target_link_libraries(V2VSimulation PRIVATE Qt6::Widgets Qt6::Network pugixml::pugixml)
target_include_directories(V2VSimulation PRIVATE src)

target_compile_definitions(V2VSimulation PRIVATE DATA_DIR="${CMAKE_SOURCE_DIR}/data")

option(V2V_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
if(V2V_SANITIZERS AND NOT MSVC)
    target_compile_options(V2VSimulation PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(V2VSimulation PRIVATE -fsanitize=address,undefined)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests/audit)
endif()
