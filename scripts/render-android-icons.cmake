if(NOT DEFINED IM OR NOT DEFINED SVG OR NOT DEFINED SRC OR NOT DEFINED OUT)
    message(FATAL_ERROR "render-android-icons.cmake needs IM, SVG, SRC, and OUT")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${SRC}" "${OUT}"
    COMMAND_ERROR_IS_FATAL ANY
)

set(_densities mdpi 48 hdpi 72 xhdpi 96 xxhdpi 144 xxxhdpi 192)
list(LENGTH _densities _count)
math(EXPR _last "${_count} - 1")
foreach(_index RANGE 0 ${_last} 2)
    list(GET _densities ${_index} _density)
    math(EXPR _size_index "${_index} + 1")
    list(GET _densities ${_size_index} _size)
    set(_dir "${OUT}/res/mipmap-${_density}")
    file(MAKE_DIRECTORY "${_dir}")
    execute_process(
        COMMAND "${IM}" -background none -resize ${_size}x${_size} -depth 8
                "${SVG}" "${_dir}/ic_launcher.png"
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy
                "${_dir}/ic_launcher.png" "${_dir}/ic_launcher_round.png"
        COMMAND_ERROR_IS_FATAL ANY
    )
endforeach()
