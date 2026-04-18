cmake_minimum_required(VERSION 3.16)

project(MIDI2Bridge VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.11.0/msvc2022_64")
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

set(RTMIDI_DIR "C:/Users/LucasRamosSilva/Documents/rtmidi_lib")
include_directories("${RTMIDI_DIR}/include")
link_directories("${RTMIDI_DIR}/lib")

set(SOURCES
    src/main.cpp
    src/converter.cpp
    src/converter.h
    src/ump.h
    src/gui.cpp
    src/gui.h
)

add_executable(MIDI2Bridge ${SOURCES})

target_link_libraries(MIDI2Bridge PRIVATE 
    Qt6::Widgets 
    Qt6::Core
    rtmidi
)

if(WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_target_properties(MIDI2Bridge PROPERTIES WIN32_EXECUTABLE ON)
endif()