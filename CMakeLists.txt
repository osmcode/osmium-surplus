#-----------------------------------------------------------------------------
#
#  CMake Config
#
#  Osmium Surplus
#
#-----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.5.0)

project(OsmiumSurplus VERSION 0.0.1 LANGUAGES CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")


#-----------------------------------------------------------------------------

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(AUTHOR "Jochen Topf <jochen@topf.org>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


#-----------------------------------------------------------------------------
#
#  Find external dependencies
#
#-----------------------------------------------------------------------------

find_package(Osmium 2.17.0 COMPONENTS io ogr)
include_directories(SYSTEM ${OSMIUM_INCLUDE_DIRS})

find_package(SQLite3 REQUIRED)

include_directories(SYSTEM include)


#-----------------------------------------------------------------------------
#
#  Compiler and Linker flags
#
#-----------------------------------------------------------------------------
set(USUAL_COMPILE_OPTIONS "-O3 -g -fno-omit-frame-pointer")

set(CMAKE_CXX_FLAGS_DEV "${USUAL_COMPILE_OPTIONS}"
    CACHE STRING "Flags used by the compiler during developer builds."
    FORCE)

set(CMAKE_EXE_LINKER_FLAGS_DEV ""
    CACHE STRING "Flags used by the linker during developer builds."
    FORCE)
mark_as_advanced(
    CMAKE_CXX_FLAGS_DEV
    CMAKE_EXE_LINKER_FLAGS_DEV
)

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${USUAL_COMPILE_OPTIONS}"
    CACHE STRING "Flags used by the compiler during RELWITHDEBINFO builds."
    FORCE)


#-----------------------------------------------------------------------------
#
#  Build Type
#
#-----------------------------------------------------------------------------
set(CMAKE_CONFIGURATION_TYPES "Debug Release RelWithDebInfo MinSizeRel Dev")

# In 'Dev' mode: compile with very strict warnings and turn them into errors.
if(CMAKE_BUILD_TYPE STREQUAL "Dev")
    if(NOT MSVC)
        add_definitions(-Werror -fno-omit-frame-pointer)
    endif()
    add_definitions(${OSMIUM_WARNING_OPTIONS})
endif()

# Force RelWithDebInfo build type if none was given
if(CMAKE_BUILD_TYPE)
    set(build_type ${CMAKE_BUILD_TYPE})
else()
    set(build_type "RelWithDebInfo")
endif()

set(CMAKE_BUILD_TYPE ${build_type}
    CACHE STRING
    "Choose the type of build, options are: ${CMAKE_CONFIGURATION_TYPES}."
    FORCE)


#-----------------------------------------------------------------------------
#
#  Optional "cppcheck" target that checks C++ code
#
#-----------------------------------------------------------------------------
message(STATUS "Looking for cppcheck")
find_program(CPPCHECK cppcheck)

if(CPPCHECK)
    message(STATUS "Looking for cppcheck - found")
    set(CPPCHECK_OPTIONS --enable=all)

    # cpp doesn't find system includes for some reason, suppress that report
    set(CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --suppress=missingIncludeSystem)

    file(GLOB ALL_CODE src/*.cpp)

    set(CPPCHECK_FILES ${ALL_CODE})

    add_custom_target(cppcheck
        ${CPPCHECK}
        --std=c++17 ${CPPCHECK_OPTIONS}
        ${CPPCHECK_FILES}
    )
else()
    message(STATUS "Looking for cppcheck - not found")
    message(STATUS "  Build target 'cppcheck' will not be available.")
endif(CPPCHECK)


#-----------------------------------------------------------------------------
#
#  Optional "clang-tidy" target
#
#-----------------------------------------------------------------------------
message(STATUS "Looking for clang-tidy")
find_program(CLANG_TIDY NAMES clang-tidy clang-tidy-14 clang-tidy-13 clang-tidy-12 clang-tidy-11)

if(CLANG_TIDY)
    message(STATUS "Looking for clang-tidy - found ${CLANG_TIDY}")

    file(GLOB _sources RELATIVE "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/src/*.cpp")

    add_custom_target(clang-tidy
        ${CLANG_TIDY}
        -p ${CMAKE_BINARY_DIR}
        ${_sources}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src"
    )
else()
    message(STATUS "Looking for clang-tidy - not found")
    message(STATUS "  Build target 'clang-tidy' will not be available.")
endif()


#-----------------------------------------------------------------------------

add_definitions(${OSMIUM_WARNING_OPTIONS})

add_subdirectory(src)

enable_testing()
add_subdirectory(test)


#-----------------------------------------------------------------------------
