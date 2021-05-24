###
 # Copyright (c) Contributors as noted in the AUTHORS file.
 #
 # This Source Code Form is part of *fuurin* library.
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ##


# Save previous contents

option(ENABLE_ASAN "Build with address sanitizer" OFF)
if(ENABLE_ASAN)
    message(STATUS "Instrumenting with Address Sanitizer")
    set(ASAN_COMPILE_FLAGS
        -fsanitize=address
        -fsanitize-address-use-after-scope
        -fno-omit-frame-pointer
    )
    set(ASAN_LINK_FLAGS
        -fsanitize=address
        -fsanitize-address-use-after-scope
    )
endif()


option(ENABLE_TSAN "Build with thread sanitizer" OFF)
if(ENABLE_TSAN)
    message(STATUS "Instrumenting with Thread Sanitizer")
    set(TSAN_COMPILE_FLAGS
        -fsanitize=thread
        -g
    )
    set(TSAN_LINK_FLAGS
        -fsanitize=thread
    )
endif()


option(ENABLE_MSAN "Build with memory sanitizer" OFF)
if(ENABLE_MSAN)
    message(STATUS "Instrumenting with Memory Sanitizer")
    set(MSAN_COMPILE_FLAGS
        -fsanitize=memory
        -fsanitize-memory-track-origins
        -g
        -fPIE
        -fno-omit-frame-pointer
        -fno-optimize-sibling-calls
    )
    set(MSAN_LINK_FLAGS
        -fsanitize=memory
        -fPIE
        -pie
    )
endif()


option(ENABLE_UBSAN "Build with undefined behavior sanitizer" OFF)
if(ENABLE_UBSAN)
    message(STATUS "Instrumenting with Undefined Behaviour Sanitizer")
    set(UBSAN_COMPILE_FLAGS
        -fsanitize=undefined
    )
    set(UBSAN_LINK_FLAGS
        -fsanitize=undefined
    )
endif()


set(SANITIZERS_COMPILE_FLAGS
    ${ASAN_COMPILE_FLAGS}
    ${TSAN_COMPILE_FLAGS}
    ${MSAN_COMPILE_FLAGS}
    ${UBSAN_COMPILE_FLAGS}
)


set(SANITIZERS_LINK_FLAGS
    ${ASAN_LINK_FLAGS}
    ${TSAN_LINK_FLAGS}
    ${MSAN_LINK_FLAGS}
    ${UBSAN_LINK_FLAGS}
)
