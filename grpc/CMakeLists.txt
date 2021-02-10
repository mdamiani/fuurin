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
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

# gRPC C++ plugin
get_target_property(gRPC_CPP_PLUGIN_EXECUTABLE gRPC::grpc_cpp_plugin
    IMPORTED_LOCATION_RELEASE)

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

# Targets
foreach(_target
    fuurin_worker
    fuurin_cli)
    add_executable(${_target} "${_target}.cpp" ${wrk_proto_srcs} ${wrk_grpc_srcs})
    target_include_directories(${_target} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(${_target} protobuf::libprotobuf gRPC::grpc++_unsecure)
endforeach()
