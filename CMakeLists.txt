cmake_minimum_required(VERSION 3.10)

project(OpenAL_C4Note)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
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

# Define the executable
add_executable(keyboard keyboard.cpp note.cpp notes.cpp sound.cpp fir.cpp main.cpp)

# Link OpenAL to the executables
target_link_libraries(keyboard ${OPENAL_LIBRARY})
target_link_libraries(keyboard ${CURSES_LIBRARIES})
target_include_directories(keyboard PRIVATE ${OPENAL_INCLUDE_DIR})
target_include_directories(keyboard PUBLIC ${CURSES_INCLUDE_DIR})

# Look for FFTW
add_subdirectory(fftw-3.3.10)

find_path(FFTW_INCLUDE_DIR fftw3.h
          HINTS ${CMAKE_SOURCE_DIR}/fftw-3.3.10
          PATH_SUFFIXES api include)

if(FFTW_INCLUDE_DIR)
    include_directories(${FFTW_INCLUDE_DIR})
else()
    message(FATAL_ERROR "Cannot find fftw3.h!")
endif()

# Define the executable
add_executable(play_note play_note.cpp fir.cpp adsr.cpp dft.cpp)

# Link OpenAL to the executables
target_link_libraries(play_note ${OPENAL_LIBRARY})
target_link_libraries(play_note ${CURSES_LIBRARIES} fftw3)
target_include_directories(play_note PRIVATE ${OPENAL_INCLUDE_DIR} ${FFTW_INCLUDE_DIRS})
target_include_directories(play_note PUBLIC ${CURSES_INCLUDE_DIR} ${FFTW_LIBRARIES})
