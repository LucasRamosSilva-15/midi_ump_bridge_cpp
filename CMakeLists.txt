cmake_minimum_required(VERSION 3.16)

project(MIDI2Bridge VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

include(FetchContent)
message(STATUS "Baixando dependencia e compilando: RtMidi...")
FetchContent_Declare(
    rtmidi
    GIT_REPOSITORY https://github.com/thestk/rtmidi.git
    GIT_TAG 6.0.0
)
set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "Desativar testes" FORCE)
set(RTMIDI_BUILD_STATIC ON CACHE BOOL "Build estatico" FORCE)
set(RTMIDI_BUILD_SHARED OFF CACHE BOOL "Build compartilhado" FORCE)
FetchContent_MakeAvailable(rtmidi)

set(SOURCES
    src/main.cpp
    src/converter.cpp
    src/converter.h
    src/ump.h
    src/gui.cpp
    src/gui.h
)

add_executable(MIDI2Bridge ${SOURCES})

target_include_directories(MIDI2Bridge PRIVATE "src")

target_link_libraries(MIDI2Bridge PRIVATE 
    Qt6::Widgets 
    Qt6::Core
    rtmidi
)

# if(WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
#    set_target_properties(MIDI2Bridge PROPERTIES WIN32_EXECUTABLE ON)
# endif()