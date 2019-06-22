###
 # Copyright (c) Contributors as noted in the AUTHORS file.
 #
 # This Source Code Form is part of *fuurin* library.
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ##

if(NOT CMAKE_SYSTEM_NAME MATCHES "Linux")
    message(FATAL_ERROR "Cannot configure CPack to generate Debian packages on non-linux systems.")
endif()

execute_process(COMMAND dpkg --print-architecture
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE)

set(CPACK_GENERATOR "DEB")
set(CPACK_INSTALL_PREFIX /usr)
set(CPACK_STRIP_FILES true)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS false)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION true)

set(FILE_POSTINST "${CMAKE_CURRENT_BINARY_DIR}/deb/control/postinst")
set(FILE_POSTRM   "${CMAKE_CURRENT_BINARY_DIR}/deb/control/postrm")
set(FILE_SHLIBS   "${CMAKE_CURRENT_BINARY_DIR}/deb/control/shlibs")

set(CPACK_DEBIAN_LIBFUURIN_PACKAGE_NAME "${LIB_NAME}${LIB_SOVERSION}")
set(CPACK_DEBIAN_LIBFUURIN_PACKAGE_SECTION "libs")
set(CPACK_DEBIAN_LIBFUURIN_PACKAGE_CONTROL_EXTRA
    "${FILE_POSTINST}"
    "${FILE_POSTRM}"
    "${FILE_SHLIBS}"
)

set(CPACK_DEBIAN_LIBFUURIN-DEV_PACKAGE_NAME "${LIB_NAME}${LIB_SOVERSION}-dev")
set(CPACK_DEBIAN_LIBFUURIN-DEV_PACKAGE_DEPENDS "${LIB_NAME}${LIB_SOVERSION} (= ${LIB_VERSION_FULL})")
set(CPACK_DEBIAN_LIBFUURIN-DEV_PACKAGE_SECTION "libdevel")

set(CPACK_DEBIAN_PACKAGE_SOURCE ${PROJECT_NAME})
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Community (hopefully)")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_PREDEPENDS "multiarch-support")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.23), libstdc++6 (>= 5.4.0)")

string(CONCAT DEB_FILE_NAME
    "${CMAKE_PROJECT_NAME}_"
    "${LIB_VERSION_MAJOR}."
    "${LIB_VERSION_MINOR}."
    "${LIB_VERSION_PATCH}_"
    "${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}"
)

set(CPACK_DEB_COMPONENT_INSTALL 1)
set(CPACK_PACKAGE_FILE_NAME ${DEB_FILE_NAME})


# Generate postinst and prerm hooks
file(WRITE "${FILE_POSTINST}"
"#!/bin/sh
set -e

if [ \"$1\" = \"configure\" ]; then
    dpkg-trigger ldconfig
fi
")

file(WRITE "${FILE_POSTRM}"
"#!/bin/sh
set -e

if [ \"$1\" = \"remove\" ]; then
    dpkg-trigger ldconfig
fi
")

# Generate shlibs
file(WRITE "${FILE_SHLIBS}"
"${LIB_NAME} ${LIB_SOVERSION} ${CPACK_DEBIAN_LIBFUURIN_PACKAGE_NAME} (>= ${LIB_VERSION_FULL})
")

execute_process(COMMAND chmod 755 "${FILE_POSTINST}" "${FILE_POSTRM}")
execute_process(COMMAND chmod 644 "${FILE_SHLIBS}")
