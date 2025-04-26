cmake_minimum_required(VERSION 3.10)

project(OpenAL_C4Note)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the ncurses package
find_package(Curses REQUIRED)

# Find OpenAL package
find_package(OpenAL REQUIRED)

include(CheckIncludeFile)

# Check what Open AL headers look like
check_include_file("OpenAL/al.h" HAVE_OPENAL_AL_H)
check_include_file("AL/al.h" HAVE_AL_AL_H)

if(HAVE_OPENAL_AL_H)
    add_definitions(-DUSE_OPENAL_PREFIX)
elseif(HAVE_AL_AL_H)
    add_definitions(-DUSE_AL_PREFIX)
else()
    message(FATAL_ERROR "Neither OpenAL/al.h nor AL/al.h found!")
endif()

# Look for FFTW
add_subdirectory(external/fftw-3.3.10)
add_subdirectory(external/midifile)

find_path(FFTW_INCLUDE_DIR fftw3.h
          HINTS ${CMAKE_CURRENT_SOURCE_DIR}/external/fftw-3.3.10
          PATH_SUFFIXES api include)

if(FFTW_INCLUDE_DIR)
    include_directories(${FFTW_INCLUDE_DIR})
else()
    message(FATAL_ERROR "Cannot find fftw3.h!")
endif()

# Define the keylib library
add_library(keylib STATIC
    src/keyboard.cpp
    src/note.cpp
    src/notes.cpp
    src/sound.cpp
    src/fir.cpp
    src/dft.cpp
    src/waveread.cpp
    src/effect.cpp
    src/adsr.cpp
    src/iir.cpp
    src/keyboardstream.cpp
)

# Link dependencies to the keylib library
target_link_libraries(keylib ${OPENAL_LIBRARY} ${CURSES_LIBRARIES} fftw3)
target_include_directories(keylib PRIVATE include external/nlohmann external/midifile/include ${OPENAL_INCLUDE_DIR} ${FFTW_INCLUDE_DIRS})
target_include_directories(keylib PUBLIC include external/nlohmann external/midifile/include ${CURSES_INCLUDE_DIR} ${FFTW_LIBRARIES})

# Define the keyboard executable
add_executable(keyboard src/main.cpp)

find_package(SDL2 QUIET)
if(SDL2_FOUND)
    message(STATUS "SDL2 found. Building streamtest.")
    
    add_executable(streamtest src/stream_test.cpp)

    target_include_directories(streamtest PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(streamtest keylib midifile ${SDL2_LIBRARIES})

    add_executable(audiolizer src/audiolizer.cpp)

    target_include_directories(audiolizer PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(audiolizer keylib midifile ${SDL2_LIBRARIES})
else()
    message(WARNING "SDL2 not found. Skipping build of streamtest.")
endif()

# Link keylib to the keyboard executable
target_link_libraries(keyboard keylib midifile)

