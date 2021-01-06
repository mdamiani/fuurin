###
 # Copyright (c) Contributors as noted in the AUTHORS file.
 #
 # This Source Code Form is part of *fuurin* library.
 #
 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ##

include_directories(
    "../src"
)

function (AddExample EXAMPLE)
    message("-- Adding example ${EXAMPLE}")

    add_executable(${EXAMPLE} ${EXAMPLE}.cpp)
    add_dependencies(${EXAMPLE} fuurin_static)

    target_link_libraries(${EXAMPLE} fuurin_static)
    target_include_directories(${EXAMPLE} PRIVATE ../)
endfunction (AddExample)

AddExample(producer)
AddExample(consumer)
