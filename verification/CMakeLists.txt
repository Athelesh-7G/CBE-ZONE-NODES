cmake_minimum_required(VERSION 3.16)
project(cbe_verification LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(CBE_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

add_library(cbe_options INTERFACE)
target_compile_options(cbe_options INTERFACE -Wall -Wextra -Wpedantic -Werror)
target_include_directories(cbe_options INTERFACE include framework)
if(CBE_SANITIZE)
  target_compile_options(cbe_options INTERFACE -fsanitize=address,undefined
                         -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g)
  target_link_options(cbe_options INTERFACE -fsanitize=address,undefined)
endif()

# Test sources are compiled once as objects and linked into each executable that
# needs the registered tests (a static library would drop the static registrars).
add_library(cbe_suite OBJECT
  tests/test_solvers.cpp
  tests/test_ts_bridge.cpp)
target_link_libraries(cbe_suite PUBLIC cbe_options)

add_executable(cbe_tests framework/tt/main.cpp $<TARGET_OBJECTS:cbe_suite>)
target_link_libraries(cbe_tests PRIVATE cbe_options)

add_executable(cbe_gen tools/gen.cpp)
target_link_libraries(cbe_gen PRIVATE cbe_options)
