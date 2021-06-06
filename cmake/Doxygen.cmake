###
 # Copyright (c) Contributors as noted in the AUTHORS file.
 #
 # This Source Code Form is part of *fuurin* library.
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ##

option(ENABLE_DOXYGEN "Enable Doxygen" OFF)
if(ENABLE_DOXYGEN)
    message(STATUS "Doxygen enabled")
else()
    message(STATUS "Doxygen disabled")
    return()
endif()

find_package(Doxygen REQUIRED dot OPTIONAL_COMPONENTS mscgen dia)

# Doxygen settings can be set here, prefixed with "DOXYGEN_"
set(DOXYGEN_SOURCE_BROWSER YES)
set(DOXYGEN_EXTRACT_PRIVATE YES)
set(DOXYGEN_WARN_AS_ERROR YES)
set(DOXYGEN_EXTRACT_ALL YES)
set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doxygen")
set(DOXYGEN_EXCLUDE_PATTERNS "*/test_grpc.cpp")

# this target will only be built if specifically asked to.
# run "make api-docs" to create the doxygen documentation
doxygen_add_docs(doxygen
    include/fuurin
    src
    grpc
    COMMENT "Generate API documentation with Doxygen"
)
