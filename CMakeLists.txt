cmake_minimum_required(VERSION 3.10)

project(keyboard_synth)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(WITH_NCURSES "Enable ncurses support" ON)

include(CheckIncludeFile)

# Optional OpenAL
find_package(OpenAL QUIET)

set(KEYLIB_SRCS
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
    src/looper.cpp
)

if(OPENAL_FOUND)
  message(STATUS "OpenAL found. Building keyboard.cpp support.")
  list(APPEND KEYLIB_SRCS src/keyboard.cpp)

  # Check what OpenAL headers look like
  check_include_file("OpenAL/al.h" HAVE_OPENAL_AL_H)
  check_include_file("AL/al.h" HAVE_AL_AL_H)

  if(HAVE_OPENAL_AL_H)
    add_definitions(-DUSE_OPENAL_PREFIX)
  elseif(HAVE_AL_AL_H)
    add_definitions(-DUSE_AL_PREFIX)
  else()
    message(FATAL_ERROR "OpenAL found, but neither OpenAL/al.h nor AL/al.h found!")
  endif()
else()
  message(WARNING "OpenAL not found. Excluding src/keyboard.cpp from keylib.")
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
add_library(keylib STATIC ${KEYLIB_SRCS})

if(WITH_NCURSES)
  find_package(Curses)

  if(CURSES_FOUND)
    target_compile_definitions(keylib PUBLIC BUILD_WITH_NCURSES=1)
    target_link_libraries(keylib PUBLIC ${CURSES_LIBRARIES})
    target_include_directories(keylib PUBLIC ${CURSES_INCLUDE_DIR})
  else()
    message(FATAL_ERROR "WITH_NCURSES=ON but ncurses not found")
  endif()
endif()

if(OPENAL_FOUND)
  target_link_libraries(keylib PUBLIC ${OPENAL_LIBRARY})
  target_include_directories(keylib PRIVATE ${OPENAL_INCLUDE_DIR})
  target_compile_definitions(keylib PUBLIC BUILD_WITH_OPENAL=1)
endif()

target_link_libraries(keylib PUBLIC fftw3)
target_include_directories(keylib PRIVATE
    include
    external/nlohmann
    external/midifile/include
    ${FFTW_INCLUDE_DIR}
)
target_include_directories(keylib PUBLIC
    include
    external/nlohmann
    external/midifile/include
)

# Define the keyboard executable
add_executable(keyboard src/main.cpp)
target_link_libraries(keyboard keylib midifile)

find_package(SDL2 QUIET)
if(SDL2_FOUND)
  message(STATUS "SDL2 found. Building streaming based keyboard (more advanced).")

  add_subdirectory(external/civetweb)

  add_executable(keyboardstream
      src/main_stream.cpp
      src/api.cpp
      external/civetweb/src/CivetServer.cpp
  )

  target_include_directories(keyboardstream PRIVATE civetweb ${SDL2_INCLUDE_DIRS})
  target_compile_definitions(keyboardstream PRIVATE CIVETWEB)
  target_link_libraries(keyboardstream civetweb-c-library keylib midifile ${SDL2_LIBRARIES})

  add_executable(audiolizer src/audiolizer.cpp)
  target_include_directories(audiolizer PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(audiolizer keylib midifile ${SDL2_LIBRARIES})
else()
  message(WARNING "SDL2 not found. Skipping build of streaming based keyboard (more advanced).")
endif()