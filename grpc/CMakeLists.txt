###
 # Copyright (c) Contributors as noted in the AUTHORS file.
 #
 # This Source Code Form is part of *fuurin* library.
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ##

option(ENABLE_SERVICE_GRPC "Enable Service gRPC" OFF)
if(ENABLE_SERVICE_GRPC)
    message(STATUS "Service gRPC enabled")
else()
    message(STATUS "Service gRPC disabled")
    return()
endif()


# Protobuf
find_package(Protobuf REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")


# gRPC
find_package(gRPC CONFIG)
# First attempt to set up gRPC via cmake; but if cmake config files aren't
# available, fallback to pkg-config.
if (gRPC_FOUND)
    # gRPC C++ plugin
    get_target_property(gRPC_CPP_PLUGIN_EXECUTABLE gRPC::grpc_cpp_plugin IMPORTED_LOCATION_RELEASE)
    set(gRPC_INC $<TARGET_PROPERTY:gRPC::grpc++_unsecure,INTERFACE_INCLUDE_DIRECTORIES>)
    set(gRPC_LIB gRPC::grpc++_unsecure)
else()
    message(STATUS "Falling back to finding gRPC with pkg-config")
    # gRPC C++ plugin
    find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
    if (NOT gRPC_CPP_PLUGIN_EXECUTABLE)
        message(FATAL_ERROR "grpc_cpp_plugin not found!")
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_search_module(gRPC REQUIRED grpc)
    pkg_search_module(gRPCPP REQUIRED grpc++)
    set(gRPC_INC ${gRPC_INCLUDE_DIRS} ${gRPCPP_INCLUDE_DIRS})
    set(gRPC_LIB ${gRPCPP_LDFLAGS} ${gRPC_LDFLAGS})
endif()
message(STATUS "Using gRPC ${gRPC_VERSION}")


# Generated sources
set(wrk_proto ${PROJECT_SOURCE_DIR}/grpc/worker.proto)
set(wrk_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/worker.grpc.pb.cc")
set(wrk_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/worker.grpc.pb.h")
protobuf_generate_cpp(wrk_proto_srcs wrk_proto_hdrs "${wrk_proto}")
add_custom_command(
    OUTPUT "${wrk_grpc_srcs}" "${wrk_grpc_hdrs}"
    COMMAND protobuf::protoc
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${PROJECT_SOURCE_DIR}/grpc"
        --plugin=protoc-gen-grpc="${gRPC_CPP_PLUGIN_EXECUTABLE}"
        "${wrk_proto}"
    DEPENDS "${wrk_proto}")


# We are using system gRPC libraries which are not compiled with Sanitizers:
# https://github.com/grpc/grpc/issues/21838
# https://github.com/grpc/grpc/issues/22238
set(GRPC_SAN_SUPPR
    GRPC_ASAN_SUPPRESSED
    GRPC_TSAN_SUPPRESSED
)


# Targets
foreach(_target
    fuurin_worker)

    message("-- Adding gRPC ${_target}")

    add_executable(${_target}
        "${_target}.cpp"
        "${_target}_impl.cpp"
        utils.cpp
        ${wrk_proto_srcs}
        ${wrk_grpc_srcs})

    add_dependencies(${_target} fuurin_static)
    target_compile_definitions(${_target} PUBLIC ${GRPC_SAN_SUPPR})
    target_include_directories(${_target} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" ${gRPC_INC})
    target_link_libraries(${_target} fuurin_static protobuf::libprotobuf ${gRPC_LIB})
endforeach()


set(_gRPC_broker fuurin_broker)

message("-- Adding gRPC ${_gRPC_broker}")
add_executable(${_gRPC_broker} fuurin_broker.cpp utils.cpp)
add_dependencies(${_gRPC_broker} fuurin_static)
target_compile_definitions(${_gRPC_broker} PUBLIC ${GRPC_SAN_SUPPR})
target_include_directories(${_gRPC_broker} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(${_gRPC_broker} fuurin_static)


if(BUILD_TESTS)
    AddTest(test_grpc)

    target_sources(test_grpc PUBLIC
        fuurin_worker_impl.cpp
        fuurin_cli_impl.cpp
        utils.cpp
        ${wrk_proto_srcs}
        ${wrk_grpc_srcs}
    )

    target_compile_definitions(test_grpc PUBLIC ${GRPC_SAN_SUPPR})
    target_include_directories(test_grpc PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" ${gRPC_INC})
    target_link_libraries(test_grpc protobuf::libprotobuf ${gRPC_LIB})
endif()
