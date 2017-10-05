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

project(fuurin)

set(CMAKE_CXX_STANDARD 14)

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

set(LIB_SOURCES
    src/fuurin.cpp
)

set(LIB_PUBLIC_HEADERS
    include/fuurin/fuurin.h
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

add_library(fuurin_shared SHARED ${LIB_SOURCES})
add_library(fuurin_static STATIC ${LIB_SOURCES})

set_target_properties(fuurin_shared PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    VERSION ${LIB_VERSION_FULL}
    SOVERSION ${LIB_VERSION_MAJOR}
)

set_target_properties(fuurin_static PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
)

add_dependencies(fuurin_shared ${LIB_DEPS})
add_dependencies(fuurin_static ${LIB_DEPS})

include_directories(include ${3RDPARTY_DIR}/include)
link_directories(${3RDPARTY_DIR}/lib)

if(DEFINED TESTS AND TESTS EQUAL 1)
    message("-- Tests enabled")
    include(CTest)
    add_subdirectory(${PROJECT_SOURCE_DIR}/test)
else()
    message("-- Tests disabled")
endif()


# CPack configuration
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Simple and fast ZeroMQ communication library for microservices")
set(CPACK_PACKAGE_VERSION_MAJOR ${LIB_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${LIB_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${LIB_VERSION_PATCH})
set(CPACK_SET_DESTDIR true)
set(CPACK_COMPONENTS_ALL libfuurin libfuurin-dev)

# Debian package
if(CMAKE_SYSTEM_NAME MATCHES Linux)
    execute_process(COMMAND dpkg --print-architecture
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE)

    set(CPACK_GENERATOR "DEB")
    set(CPACK_INSTALL_PREFIX /usr)
    set(CPACK_STRIP_FILES true)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS true)
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION true)

    set(CPACK_DEBIAN_LIBFUURIN_PACKAGE_NAME "libfuurin${LIB_VERSION_MAJOR}")
    set(CPACK_DEBIAN_LIBFUURIN_PACKAGE_SECTION "libs")
    set(CPACK_DEBIAN_LIBFUURIN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_SOURCE_DIR}/cpack/deb/control/postinst"
        "${CMAKE_SOURCE_DIR}/cpack/deb/control/postrm")

    set(CPACK_DEBIAN_LIBFUURIN-DEV_PACKAGE_NAME "libfuurin${LIB_VERSION_MAJOR}-dev")
    set(CPACK_DEBIAN_LIBFUURIN-DEV_PACKAGE_DEPENDS "libfuurin${LIB_VERSION_MAJOR}")
    set(CPACK_DEBIAN_LIBFUURIN-DEV_PACKAGE_SECTION "libdevel")

    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Community (hopefully)")
    set(CPACK_DEBIAN_PACKAGE_PREDEPENDS "multiarch-support")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.23), libstdc++6 (>= 5.4.0)")

    string(CONCAT DEB_FILE_NAME
        "${CMAKE_PROJECT_NAME}_"
        "${LIB_VERSION_MAJOR}."
        "${LIB_VERSION_MINOR}."
        "${LIB_VERSION_PATCH}_"
        "${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")

    set(CPACK_DEB_COMPONENT_INSTALL 1)
    set(CPACK_PACKAGE_FILE_NAME ${DEB_FILE_NAME})
endif()

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
