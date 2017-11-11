###
 # Copyright (c) Contributors as noted in the AUTHORS file.
 #
 # This Source Code Form is part of *fuurin* library.
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ##

set(Boost_USE_STATIC_LIBS ON)

find_package(Boost 1.58 COMPONENTS unit_test_framework REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})

function (AddTest TEST)
    message("-- Adding test ${TEST}")
    add_executable(${TEST} ${TEST}.cpp)
    target_link_libraries(${TEST} fuurin_static ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
    target_include_directories(${TEST} PRIVATE ../)
    add_test(NAME ${TEST}
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMAND $<TARGET_FILE:${TEST}>)
endfunction (AddTest)

AddTest(test_common)
