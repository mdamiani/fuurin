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
        -D CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
)

ExternalProject_Add(google_benchmark
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/google/benchmark"
    INSTALL_DIR "${3RDPARTY_DIR}"

    CMAKE_ARGS
        -D CMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -D CMAKE_BUILD_TYPE=Release
        -D BENCHMARK_ENABLE_TESTING=OFF
        -D BENCHMARK_ENABLE_GTEST_TESTS=OFF
        -D BENCHMARK_ENABLE_LTO=false
        -D CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
)

AddImportedLibrary(boost_utf_static boost_test_framework)
AddImportedLibrary(benchmark_static benchmark)

function (AddTest TEST)
    message("-- Adding test ${TEST}")

    add_executable(${TEST} ${TEST}.cpp)
    add_dependencies(${TEST} fuurin_static boost_utf google_benchmark)

    target_link_libraries(${TEST} fuurin_static boost_utf_static benchmark_static)
    target_include_directories(${TEST} PRIVATE ../)
    target_compile_definitions(${TEST} PRIVATE ZMQ_BUILD_DRAFT_API)

    add_test(NAME ${TEST}
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMAND $<TARGET_FILE:${TEST}>)
endfunction (AddTest)

AddTest(test_common)
AddTest(test_logarg)
AddTest(test_socket)
AddTest(test_timers)
AddTest(test_cancel)
AddTest(test_worker)
AddTest(test_broker)
AddTest(test_connmachine)
AddTest(test_syncmachine)
AddTest(test_uuid)
AddTest(test_network)
AddTest(test_tokenpool)
AddTest(test_cbind)

target_compile_definitions(test_common PRIVATE ${LIB_COMPILE_DEFS})
