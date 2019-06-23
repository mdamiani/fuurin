###
 # Copyright (c) Contributors as noted in the AUTHORS file.
 #
 # This Source Code Form is part of *fuurin* library.
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ##

cmake_minimum_required(VERSION 3.5.1)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(fuurin)

set(CMAKE_CXX_STANDARD 17)

add_definitions(
    -Wall
    -Wextra
    -DBOOST_SCOPE_EXIT_CONFIG_USE_LAMBDAS
    -DZMQ_BUILD_DRAFT_API
)

include(BuildType)
include(Sanitizer)
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
        -D CMAKE_BUILD_TYPE=Release
)

set(LIB_SOURCES
    src/fuurin.cpp
    src/errors.cpp
    src/logger.cpp
    src/zmqcontext.cpp
    src/zmqsocket.cpp
    src/zmqpoller.cpp
    src/zmqpart.cpp
    src/failure.cpp
    src/log.cpp
    src/arg.cpp
    src/broker.cpp
    src/worker.cpp
)

set(LIB_PUBLIC_HEADERS
    include/fuurin/arg.h
    include/fuurin/fuurin.h
    include/fuurin/errors.h
    include/fuurin/logger.h
    include/fuurin/zmqcontext.h
    include/fuurin/zmqsocket.h
    include/fuurin/zmqpoller.h
    include/fuurin/zmqpart.h
    include/fuurin/broker.h
    include/fuurin/worker.h
)

set(LIB_DEPS
    zeromq
)

set(LIB_VERSION_MAJOR 0)
set(LIB_VERSION_MINOR 1)
set(LIB_VERSION_PATCH 1)
set(LIB_VERSION_FULL ${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH})
set(LIB_SOVERSION ${LIB_VERSION_MAJOR})
set(LIB_NAME "lib${PROJECT_NAME}")

link_directories(${3RDPARTY_DIR}/lib)

add_library(fuurin_shared SHARED ${LIB_SOURCES})
add_library(fuurin_static STATIC ${LIB_SOURCES})

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

target_link_libraries(fuurin_static zmq ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(fuurin_shared zmq ${CMAKE_THREAD_LIBS_INIT})

option(BUILD_TESTS "Build fuurin tests" OFF)
if(BUILD_TESTS)
    message(STATUS "Tests enabled")
    include(CTest)
    add_subdirectory(${PROJECT_SOURCE_DIR}/test)
else()
    message(STATUS "Tests disabled")
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
