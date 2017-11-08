###
 # Copyright (c) Contributors as noted in the AUTHORS file.
 #
 # This Source Code Form is part of *fuurin* library.
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ##

include_directories(
    "../src"
    "../vendor/boost"
)

ExternalProject_Add(boost_utf
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/boost/libs/test/build"
    INSTALL_DIR "${3RDPARTY_DIR}"
    CMAKE_ARGS
        -D CMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -D CMAKE_BUILD_TYPE=Release
)

function (AddTest TEST)
    message("-- Adding test ${TEST}")

    add_executable(${TEST} ${TEST}.cpp)
    add_dependencies(${TEST} boost_utf)

    target_link_libraries(${TEST} fuurin_static boost_test_framework)
    target_include_directories(${TEST} PRIVATE ../)

    add_test(NAME ${TEST}
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMAND $<TARGET_FILE:${TEST}>)
endfunction (AddTest)

AddTest(test_common)
AddTest(test_zmqlow)
