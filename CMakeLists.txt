cmake_minimum_required(VERSION 3.5...3.25)

include(git_func.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CANDLESDK_VERSION_1 1)
set(CANDLESDK_VERSION_2 5)
set(CANDLESDK_VERSION_3 1)
set(CANDLESDK_VERSION
    ${CANDLESDK_VERSION_1}.${CANDLESDK_VERSION_2}.${CANDLESDK_VERSION_3})
add_compile_definitions(CANDLESDK_VERSION="${CANDLESDK_VERSION}")
add_compile_definitions(CANDLESDK_VERSION_HASH="${GIT_HASH}")
get_tag(CANDLESDK_VERSION_TAG)

project(
  candlesdk
  VERSION ${CANDLESDK_VERSION_1}.${CANDLESDK_VERSION_2}.${CANDLESDK_VERSION_3})

option(CANDLESDK_BUILD_CANDLETOOL "Build candletool (nucleotool) executable" ON)
option(
  CANDLE_BUILD_STATIC
  "Build static library (has no effect on windows library builds)"
  ON)
option(MAKE_TESTS "Enable/disable some of the unit tests" OFF)
option(SUPPRESS_WARN "Enable/disable compilation warnings" OFF)

message("====================")
message("CandleSDK version: ${CANDLESDK_VERSION} ${CANDLESDK_VERSION_TAG}")
message("Available flags and their values:")
message("  CANDLESDK_BUILD_CANDLETOOL = ${CANDLESDK_BUILD_CANDLETOOL}")
message("  CANDLE_BUILD_STATIC = ${CANDLE_BUILD_STATIC}")
message("  MAKE_TESTS = ${MAKE_TESTS}")
message("  SUPPRESS_WARN = ${SUPPRESS_WARN}")
message("====================")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT SUPPRESS_WARN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wfatal-errors -Wall -Wextra -Wpedantic
                        -Wno-unused-parameter -Wno-missing-field-initializers)
    if(NOT WIN32)
      add_compile_options(-Werror)
    endif()
  
  elseif(MSVC)
    add_compile_options(/W4) 
  endif()
endif()

if(WIN32)
  add_compile_definitions(WIN32_LEAN_AND_MEAN)
endif()

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64" OR ${CMAKE_SYSTEM_PROCESSOR}
                                                   STREQUAL "armhf")
    add_compile_options(-Wno-array-bounds -Wno-stringop-overread
                      -Wno-stringop-overflow)
endif()

set(mINI_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/mINI/inc/)

macro(add_unit_test_executable test_name)
    message("Adding unit test executable ${test_name}")
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} PRIVATE Unit_test)
    set_target_properties(
    ${test_name}
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ut_runners/
               OUTPUT_NAME test_${test_name})

endmacro(add_unit_test_executable)

add_subdirectory(commons)
add_subdirectory(candlelib)
if(CANDLESDK_BUILD_CANDLETOOL)
    add_subdirectory(candletool)
    # candletool generates its own CPackConfig (see its `include(CPack)`), so wire up `make package`
    add_custom_target(
      package COMMAND ${CMAKE_CPACK_COMMAND} --config
                      ${CMAKE_BINARY_DIR}/CPackConfig_candletool.cmake
      COMMENT "Building CPack packages"
      VERBATIM)
endif()
