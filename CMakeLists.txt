cmake_minimum_required(VERSION 3.16)
project(gitstory VERSION 0.2.0 DESCRIPTION "The story of a git repository, told in your terminal" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

add_library(gitstory_core STATIC
  src/analyze.cpp
  src/cli.cpp
  src/git.cpp
  src/i18n.cpp
  src/platform.cpp
  src/process.cpp
  src/render_json.cpp
  src/render_svg.cpp
  src/render_term.cpp
  src/roast.cpp
  src/util.cpp
)
target_include_directories(gitstory_core PUBLIC src)
target_compile_definitions(gitstory_core PUBLIC GITSTORY_VERSION="${PROJECT_VERSION}")
target_link_libraries(gitstory_core PUBLIC Threads::Threads)

if(MSVC)
  target_compile_options(gitstory_core PUBLIC /utf-8 /W4 /permissive-)
  target_compile_definitions(gitstory_core PUBLIC _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(gitstory_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
  target_compile_definitions(gitstory_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
  target_link_libraries(gitstory_core PUBLIC shell32)
endif()

add_executable(gitstory src/main.cpp)
target_link_libraries(gitstory PRIVATE gitstory_core)

if(MINGW)
  # Ship a single self-contained .exe: no libstdc++/winpthread DLLs needed next to it.
  target_link_options(gitstory PRIVATE -static)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(gitstory_tests tests/tests.cpp)
  target_link_libraries(gitstory_tests PRIVATE gitstory_core)
  if(MINGW)
    target_link_options(gitstory_tests PRIVATE -static)
  endif()
  add_test(NAME gitstory_tests COMMAND gitstory_tests)
endif()

include(GNUInstallDirs)
install(TARGETS gitstory RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
