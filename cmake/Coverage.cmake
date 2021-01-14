###
 # Copyright (c) Contributors as noted in the AUTHORS file.
 #
 # This Source Code Form is part of *fuurin* library.
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ##

option(ENABLE_COVERAGE "Enable coverage" OFF)
if(ENABLE_COVERAGE)
    message(STATUS "Coverage enabled")
else()
    message(STATUS "Coverage disabled")
    return()
endif()

# TODO: make it work for Clang
if(NOT CMAKE_COMPILER_IS_GNUCXX)
    message(FATAL_ERROR "Coverage: Error: Compiler is not GNU gcc")
endif()

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "Coverage: Error: Build type is not Debug")
endif()

set(LIB_COMPILE_FLAGS ${LIB_COMPILE_FLAGS} "--coverage")
set(LIB_LINK_FLAGS ${LIB_LINK_FLAGS} "--coverage")
