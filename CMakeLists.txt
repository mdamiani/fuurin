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
        -D CMAKE_BUILD_TYPE=RelWithDebInfo
        -D CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
)


function (AddImportedLibrary TARGET LIB)
    message("-- Adding imported library ${TARGET} ${LIB}")

    if(WIN32)
        set(prefix "")
        set(suffix ".lib")
    else()
        set(prefix "lib")
        set(suffix ".a")
    endif()

    add_library(${TARGET} STATIC IMPORTED GLOBAL)
    set_target_properties(${TARGET} PROPERTIES
        IMPORTED_LOCATION "${3RDPARTY_DIR}/lib/${prefix}${LIB}${suffix}"
    )
endfunction (AddImportedLibrary)


AddImportedLibrary(zeromq_static zmq)


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
    src/zmqcancel.cpp
    src/zmqiotimer.cpp
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
    src/tokenpool.cpp
    src/c/cutils.cpp
    src/c/cuuid.cpp
    src/c/cbroker.cpp
    src/c/cworker.cpp
    src/c/ctopic.cpp
    src/c/cevent.cpp
)

set(LIB_PUBLIC_HEADERS
    include/fuurin/arg.h
    include/fuurin/fuurin.h
    include/fuurin/errors.h
    include/fuurin/elapser.h
    include/fuurin/event.h
    include/fuurin/operation.h
    include/fuurin/logger.h
    include/fuurin/stopwatch.h
    include/fuurin/zmqcontext.h
    include/fuurin/zmqsocket.h
    include/fuurin/zmqpollable.h
    include/fuurin/zmqpoller.h
    include/fuurin/zmqpart.h
    include/fuurin/zmqtimer.h
    include/fuurin/zmqcancel.h
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
    include/fuurin/tokenpool.h
    include/fuurin/c/cbroker.h
    include/fuurin/c/cevent.h
    include/fuurin/c/ctopic.h
    include/fuurin/c/cuuid.h
    include/fuurin/c/cworker.h
)

set(LIB_VERSION_FULL ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
set(LIB_SOVERSION ${PROJECT_VERSION_MAJOR})
set(LIB_NAME "lib${PROJECT_NAME}")

link_directories(${3RDPARTY_DIR}/lib)

include(Library)

include_directories(
    "include"
    "vendor/boost"
    "${3RDPARTY_DIR}/include"
)


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

add_subdirectory(${PROJECT_SOURCE_DIR}/grpc)


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
option(USE_GNUINSTALLDIRS "Use GNUInstallDirs" ON)
if(USE_GNUINSTALLDIRS)
    message(STATUS "GNUInstallDirs enabled")
    include(GNUInstallDirs)
    set(INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}/${CMAKE_LIBRARY_ARCHITECTURE}")
    set(INSTALL_INCDIR "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")
else()
    set(CMAKE_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include")
endif()

if(BUILD_SHARED)
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
endif()

if(BUILD_STATIC)
    install(
        TARGETS fuurin_static
        ARCHIVE DESTINATION "${INSTALL_LIBDIR}"
        COMPONENT libfuurin-dev
    )
endif()

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT libfuurin-dev
)
