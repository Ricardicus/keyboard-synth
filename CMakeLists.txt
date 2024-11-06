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
add_subdirectory(fftw-3.3.10)

find_path(FFTW_INCLUDE_DIR fftw3.h
          HINTS ${CMAKE_CURRENT_SOURCE_DIR}/fftw-3.3.10
          PATH_SUFFIXES api include)

if(FFTW_INCLUDE_DIR)
    include_directories(${FFTW_INCLUDE_DIR})
else()
    message(FATAL_ERROR "Cannot find fftw3.h!")
endif()

# Define the keylib library
add_library(keylib STATIC
    keyboard.cpp
    note.cpp
    notes.cpp
    sound.cpp
    fir.cpp
    dft.cpp
    waveread.cpp
    effects.cpp
)

# Link dependencies to the keylib library
target_link_libraries(keylib ${OPENAL_LIBRARY} ${CURSES_LIBRARIES} fftw3)
target_include_directories(keylib PRIVATE ${OPENAL_INCLUDE_DIR} ${FFTW_INCLUDE_DIRS})
target_include_directories(keylib PUBLIC ${CURSES_INCLUDE_DIR} ${FFTW_LIBRARIES})

# Define the keyboard executable
add_executable(keyboard main.cpp)

# Link keylib to the keyboard executable
target_link_libraries(keyboard keylib)

