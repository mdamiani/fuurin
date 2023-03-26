###
 # Copyright (c) Contributors as noted in the AUTHORS file.
 #
 # This Source Code Form is part of *fuurin* library.
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ##

set(LIB_COMPILE_FLAGS
    -Wall
    -Wextra
)

set(LIB_LINK_FLAGS
    ${CMAKE_THREAD_LIBS_INIT}
)

set(LIB_COMPILE_DEFS
    BOOST_SCOPE_EXIT_CONFIG_USE_LAMBDAS
    ZMQ_BUILD_DRAFT_API

    LIB_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    LIB_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    LIB_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

include(Coverage)


function (AddLibrary TARGET)
    message("-- Adding library ${TARGET}")
    target_compile_definitions(${TARGET} PRIVATE ${LIB_COMPILE_DEFS})
    target_compile_options(${TARGET} PUBLIC ${LIB_COMPILE_FLAGS} ${SANITIZERS_COMPILE_FLAGS})
    target_link_libraries(${TARGET} PUBLIC ${LIB_LINK_FLAGS} ${SANITIZERS_LINK_FLAGS})
    add_dependencies(${TARGET} zeromq)
endfunction (AddLibrary)


option(BUILD_SHARED "Build shared library" ON)
if (BUILD_SHARED)
    add_library(fuurin_shared SHARED ${LIB_SOURCES})
    AddLibrary(fuurin_shared)

    target_link_libraries(fuurin_shared PUBLIC zeromq_static)

    set_target_properties(fuurin_shared PROPERTIES
        VERSION ${LIB_VERSION_FULL}
        SOVERSION ${LIB_SOVERSION}
    )
endif()


option(BUILD_STATIC "Build static library" ON)
if (BUILD_STATIC)
    add_library(fuurin_static STATIC ${LIB_SOURCES})
    AddLibrary(fuurin_static)

    # Merge zmq into fuurin static archive
    add_custom_target(make-zeromq-obj-dir ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/zeromq-obj"
    )
    add_custom_command(
        TARGET fuurin_static
        POST_BUILD
        COMMAND ${CMAKE_AR} -x   $<TARGET_FILE:zeromq_static>
        COMMAND ${CMAKE_AR} -qcs $<TARGET_FILE:fuurin_static> *.o
        COMMAND ${CMAKE_RANLIB}  $<TARGET_FILE:fuurin_static>
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/zeromq-obj"
    )
    add_dependencies(fuurin_static make-zeromq-obj-dir)
endif()

