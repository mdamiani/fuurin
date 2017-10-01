###
 # Copyright (c) Contributors as noted in the AUTHORS file.
 #
 # This Source Code Form is part of *fuurin* library.
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ##

cmake_minimum_required (VERSION 3.5.1)

project (fuurin)

set (CMAKE_CXX_STANDARD 14)

add_definitions(
    -Wall
    -Wextra
)

include(ExternalProject)

set(3RDPARTY_DIR ${CMAKE_BINARY_DIR}/3rdparty)

ExternalProject_Add(zeromq
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/zeromq"
    INSTALL_DIR ${3RDPARTY_DIR}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
)

set (LIB_SOURCES
    src/fuurin.cpp
)

set (LIB_DEPS
    zeromq
)

add_library (fuurin_shared SHARED ${LIB_SOURCES})
add_library (fuurin_static STATIC ${LIB_SOURCES})

set_target_properties(fuurin_shared PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
set_target_properties(fuurin_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

add_dependencies(fuurin_shared ${LIB_DEPS})
add_dependencies(fuurin_static ${LIB_DEPS})

include_directories(include ${3RDPARTY_DIR}/include)
link_directories(${3RDPARTY_DIR}/lib)

if (DEFINED TESTS AND TESTS EQUAL 1)
    message("-- Tests enabled")
    include(CTest)
    add_subdirectory(${PROJECT_SOURCE_DIR}/test)
else()
    message("-- Tests disabled")
endif()
