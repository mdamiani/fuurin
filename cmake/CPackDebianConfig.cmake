if(NOT CMAKE_SYSTEM_NAME MATCHES "Linux")
    message(FATAL_ERROR "Cannot configure CPack to generate Debian packages on non-linux systems.")
endif()

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
    "${CMAKE_SOURCE_DIR}/cmake/deb/control/postinst"
    "${CMAKE_SOURCE_DIR}/cmake/deb/control/postrm")

set(CPACK_DEBIAN_LIBFUURIN-DEV_PACKAGE_NAME "libfuurin${LIB_VERSION_MAJOR}-dev")
set(CPACK_DEBIAN_LIBFUURIN-DEV_PACKAGE_DEPENDS "libfuurin${LIB_VERSION_MAJOR} (= ${LIB_VERSION_FULL})")
set(CPACK_DEBIAN_LIBFUURIN-DEV_PACKAGE_SECTION "libdevel")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Community (hopefully)")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
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
