# Possible values are:
# Debug, Release, MinSizeRel, RelWithDebInfo

if(NOT CMAKE_BUILD_TYPE)
    if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
        set(CMAKE_BUILD_TYPE "Debug")
    else()
        set(CMAKE_BUILD_TYPE "Release")
    endif()
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
