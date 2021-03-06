###
 # Copyright (c) Contributors as noted in the AUTHORS file.
 #
 # This Source Code Form is part of *fuurin* library.
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ##

cmake_minimum_required(VERSION 3.16.3)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(fuurin
    VERSION 0.1.1
)

set(CMAKE_CXX_STANDARD 17)

include(BuildType)
include(Sanitizer)
include(Doxygen)
include(ExternalProject)

find_package(Threads REQUIRED)

set(3RDPARTY_DIR ${CMAKE_BINARY_DIR}/3rdparty)

ExternalProject_Add(zeromq
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/zeromq"
    INSTALL_DIR "${3RDPARTY_DIR}"

    CMAKE_ARGS
        -D CMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -D BUILD_TESTS=OFF
        -D ENABLE_CPACK=OFF
        -D BUILD_SHARED=OFF
        -D BUILD_STATIC=ON
        -D ENABLE_DRAFTS=ON
        -D WITH_TLS=OFF
        -D WITH_LIBBSD=OFF
        -D ENABLE_CURVE=OFF
        -D CMAKE_BUILD_TYPE=Release
)


set(LIB_SOURCES
    src/fuurin.cpp
    src/errors.cpp
    src/event.cpp
    src/operation.cpp
    src/logger.cpp
    src/zmqcontext.cpp
    src/zmqsocket.cpp
    src/zmqpollable.cpp
    src/zmqpoller.cpp
    src/zmqpart.cpp
    src/zmqtimer.cpp
    src/failure.cpp
    src/log.cpp
    src/arg.cpp
    src/runner.cpp
    src/broker.cpp
    src/worker.cpp
    src/workerconfig.cpp
    src/brokerconfig.cpp
    src/connmachine.cpp
    src/syncmachine.cpp
    src/stopwatch.cpp
    src/topic.cpp
    src/uuid.cpp
    src/session.cpp
    src/sessionworker.cpp
    src/sessionbroker.cpp
)

set(LIB_PUBLIC_HEADERS
    include/fuurin/arg.h
    include/fuurin/fuurin.h
    include/fuurin/errors.h
    include/fuurin/elapser.h
    include/fuurin/event.h
    include/fuurin/opearation.h
    include/fuurin/logger.h
    include/fuurin/stopwatch.h
    include/fuurin/zmqcontext.h
    include/fuurin/zmqsocket.h
    include/fuurin/zmqpollable.h
    include/fuurin/zmqpoller.h
    include/fuurin/zmqpart.h
    include/fuurin/zmqtimer.h
    include/fuurin/runner.h
    include/fuurin/broker.h
    include/fuurin/worker.h
    include/fuurin/workerconfig.h
    include/fuurin/brokerconfig.h
    include/fuurin/topic.h
    include/fuurin/uuid.h
    include/fuurin/lrucache.h
    include/fuurin/session.h
    include/fuurin/sessionenv.h
    include/fuurin/sessionworker.h
    include/fuurin/sessionbroker.h
)

set(LIB_DEPS
    zeromq
)

set(LIB_VERSION_FULL ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
set(LIB_SOVERSION ${PROJECT_VERSION_MAJOR})
set(LIB_NAME "lib${PROJECT_NAME}")

link_directories(${3RDPARTY_DIR}/lib)

add_library(fuurin_shared SHARED ${LIB_SOURCES})
add_library(fuurin_static STATIC ${LIB_SOURCES})

set(LIB_COMPILE_FLAGS
    -Wall
    -Wextra
)

set(LIB_COMPILE_DEFS
    BOOST_SCOPE_EXIT_CONFIG_USE_LAMBDAS
    ZMQ_BUILD_DRAFT_API

    LIB_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    LIB_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    LIB_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

include(Coverage)

target_compile_options(fuurin_shared PUBLIC ${LIB_COMPILE_FLAGS})
target_compile_options(fuurin_static PUBLIC ${LIB_COMPILE_FLAGS})

target_compile_definitions(fuurin_shared PUBLIC ${LIB_COMPILE_DEFS})
target_compile_definitions(fuurin_static PUBLIC ${LIB_COMPILE_DEFS})

set_target_properties(fuurin_shared PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    VERSION ${LIB_VERSION_FULL}
    SOVERSION ${LIB_SOVERSION}
)

set_target_properties(fuurin_static PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
)

add_dependencies(fuurin_shared ${LIB_DEPS})
add_dependencies(fuurin_static ${LIB_DEPS})

include_directories(
    "include"
    "vendor/boost"
    "${3RDPARTY_DIR}/include"
)

target_link_libraries(fuurin_static PUBLIC zmq ${CMAKE_THREAD_LIBS_INIT} ${LIB_LINK_FLAGS})
target_link_libraries(fuurin_shared PUBLIC zmq ${CMAKE_THREAD_LIBS_INIT} ${LIB_LINK_FLAGS})

option(BUILD_TESTS "Build fuurin tests" OFF)
if(BUILD_TESTS)
    message(STATUS "Tests enabled")
    include(CTest)
    add_subdirectory(${PROJECT_SOURCE_DIR}/test)
else()
    message(STATUS "Tests disabled")
endif()


option(BUILD_EXAMPLES "Build fuurin examples" OFF)
if(BUILD_EXAMPLES)
    message(STATUS "Examples enabled")
    add_subdirectory(${PROJECT_SOURCE_DIR}/examples)
else()
    message(STATUS "Examples disabled")
endif()


option(BUILD_DEB_PACKAGE "Build fuurin Debian package" OFF)
if(BUILD_DEB_PACKAGE)
    message(STATUS "CPack Debian enabled")

    # CPack configuration
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Simple and fast ZeroMQ communication library for microservices")
    set(CPACK_PACKAGE_VERSION_MAJOR ${LIB_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${LIB_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${LIB_VERSION_PATCH})
    set(CPACK_SET_DESTDIR true)
    set(CPACK_COMPONENTS_ALL libfuurin libfuurin-dev)

    include(CPackDebianConfig)
    include(CPack)

    cpack_add_component(libfuurin
        DISPLAY_NAME "Runtime"
        DESCRIPTION "${PROJECT_NAME} library runtime"
    )

    cpack_add_component(libfuurin-dev
        DISPLAY_NAME "Development"
        DESCRIPTION "${PROJECT_NAME} library development files"
        DEPENDS libfuurin
    )
else()
    message(STATUS "CPack Debian disabled")
endif()


# Install
include(GNUInstallDirs)

set(INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}/${CMAKE_LIBRARY_ARCHITECTURE}")
set(INSTALL_INCDIR "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")

install(
    TARGETS fuurin_shared
    LIBRARY DESTINATION "${INSTALL_LIBDIR}" NAMELINK_SKIP
    COMPONENT libfuurin
)

install(
    TARGETS fuurin_shared
    LIBRARY DESTINATION "${INSTALL_LIBDIR}" NAMELINK_ONLY
    COMPONENT libfuurin-dev
)

install(
    TARGETS fuurin_static
    ARCHIVE DESTINATION "${INSTALL_LIBDIR}"
    COMPONENT libfuurin-dev
)

install(
    FILES ${LIB_PUBLIC_HEADERS}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}"
    COMPONENT libfuurin-dev
)
