cmake_minimum_required(VERSION 4.4)
project(waterfall VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6.6's Android NDK r25 accepts -std=c++2b and rejects -std=c++23.
if(ANDROID)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-std=c++23" WATERFALL_HAS_CXX23)
    if(NOT WATERFALL_HAS_CXX23)
        check_cxx_compiler_flag("-std=c++2b" WATERFALL_HAS_CXX2B)
        if(WATERFALL_HAS_CXX2B)
            set(CMAKE_CXX23_STANDARD_COMPILE_OPTION "-std=c++2b")
            set(CMAKE_CXX23_EXTENSION_COMPILE_OPTION "-std=gnu++2b")
        endif()
    endif()
endif()

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Multimedia
        REQUIRED)
if(UNIX AND NOT ANDROID AND NOT APPLE)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
endif()

# PocketFFT is header-only, so FetchContent just vendors the header.
# Pinned to hayguen/pocketfft ea778e37 (BSD-3-Clause).
include(FetchContent)
FetchContent_Declare(
        pocketfft
        GIT_REPOSITORY https://github.com/hayguen/pocketfft.git
        GIT_TAG ea778e37710c07723435b1be58235996d1d43a5a
)
FetchContent_MakeAvailable(pocketfft)
add_library(waterfall_pocketfft INTERFACE)
target_include_directories(waterfall_pocketfft INTERFACE "${pocketfft_SOURCE_DIR}")
target_compile_definitions(waterfall_pocketfft INTERFACE POCKETFFT_NO_MULTITHREADING)

qt_add_executable(waterfall
        main.cpp
        src/MainWindow.cpp
        src/WaterfallView.cpp
        src/AppSettings.cpp
        src/SettingsDialog.cpp
        src/AudioPermission.cpp
        src/AudioRingBuffer.cpp
        src/FftEngine.cpp
        src/Palette.cpp
        src/AudioPipeline.cpp
        src/AppIcon.cpp
        src/ScreenAwake.cpp
        src/SpectrogramWidget.cpp
)
set_target_properties(waterfall PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        AUTOUIC ON
)
if(WIN32)
    set_target_properties(waterfall PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_include_directories(waterfall PRIVATE src)
if(ANDROID)
    set_target_properties(waterfall PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
    )
endif()
target_link_libraries(waterfall PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Multimedia
        waterfall_pocketfft
)
if(TARGET Qt6::DBus)
    target_link_libraries(waterfall PRIVATE Qt6::DBus)
    target_compile_definitions(waterfall PRIVATE WATERFALL_HAS_DBUS=1)
endif()

if(NOT CMAKE_CROSSCOMPILING)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(waterfall_tests
            tests/smoke_test.cpp
            tests/waterfall_view_test.cpp
            tests/settings_test.cpp
            src/MainWindow.cpp
            src/WaterfallView.cpp
            src/AppSettings.cpp
            src/SettingsDialog.cpp
            src/AudioPermission.cpp
            src/AudioRingBuffer.cpp
            src/FftEngine.cpp
            src/Palette.cpp
            src/AudioPipeline.cpp
            src/AppIcon.cpp
            src/ScreenAwake.cpp
            src/SpectrogramWidget.cpp
            tests/audio_test.cpp
    )
    target_include_directories(waterfall_tests PRIVATE src)
    set_target_properties(waterfall_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(waterfall_tests PRIVATE
            GTest::gtest_main
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Qt::Multimedia
            waterfall_pocketfft
    )
    if(TARGET Qt6::DBus)
        target_link_libraries(waterfall_tests PRIVATE Qt6::DBus)
        target_compile_definitions(waterfall_tests PRIVATE WATERFALL_HAS_DBUS=1)
    endif()
    include(GoogleTest)
    gtest_discover_tests(waterfall_tests)
    add_test(
            NAME build_scripts
            COMMAND bash "${CMAKE_SOURCE_DIR}/tests/build_scripts_test.sh"
    )
endif()

if(ANDROID)
    return()
endif()

include(GNUInstallDirs)

set(APP_ID "waterfall")
set(APP_DISPLAY_NAME "Waterfall")
set(APP_EXE_NAME "waterfall")

install(TARGETS waterfall
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32)
    if(NOT QT_TARGET_PREFIX AND Qt6_DIR)
        get_filename_component(_qt6_dir "${Qt6_DIR}" DIRECTORY)
        get_filename_component(_qt6_dir "${_qt6_dir}" DIRECTORY)
        get_filename_component(QT_TARGET_PREFIX "${_qt6_dir}" DIRECTORY)
    endif()
    if(NOT EXISTS "${QT_TARGET_PREFIX}/bin/Qt6Core.dll")
        message(FATAL_ERROR
                "Qt Windows DLLs not found under '${QT_TARGET_PREFIX}'. "
                "Run ./install-prereqs.sh --mode windows")
    endif()

    file(GLOB _qt_dlls "${QT_TARGET_PREFIX}/bin/*.dll")
    install(FILES ${_qt_dlls} DESTINATION ${CMAKE_INSTALL_BINDIR})
    foreach(_plugin platforms styles imageformats iconengines tls multimedia)
        if(EXISTS "${QT_TARGET_PREFIX}/plugins/${_plugin}")
            install(DIRECTORY "${QT_TARGET_PREFIX}/plugins/${_plugin}"
                    DESTINATION ${CMAKE_INSTALL_BINDIR})
        endif()
    endforeach()

    foreach(_dll_name libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
        execute_process(
                COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=${_dll_name}
                OUTPUT_VARIABLE _dll_path
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(EXISTS "${_dll_path}")
            install(FILES "${_dll_path}" DESTINATION ${CMAKE_INSTALL_BINDIR})
        else()
            message(FATAL_ERROR
                    "MinGW runtime DLL not found: ${_dll_name}. Install mingw-w64.")
        endif()
    endforeach()
endif()

if(UNIX AND NOT APPLE)
    configure_file(
            "${CMAKE_SOURCE_DIR}/resources/waterfall.desktop.in"
            "${CMAKE_BINARY_DIR}/${APP_ID}.desktop"
            @ONLY
    )
    install(FILES "${CMAKE_BINARY_DIR}/${APP_ID}.desktop"
            DESTINATION share/applications
    )
    install(FILES "${CMAKE_SOURCE_DIR}/resources/waterfall.svg"
            DESTINATION share/icons/hicolor/scalable/apps
    )
    file(WRITE "${CMAKE_BINARY_DIR}/postinst"
"#!/bin/sh
set -e
update-desktop-database -q || true
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q /usr/share/icons/hicolor || true
fi
exit 0
")
    file(WRITE "${CMAKE_BINARY_DIR}/postrm"
"#!/bin/sh
set -e
update-desktop-database -q || true
exit 0
")
    execute_process(COMMAND chmod +755 "${CMAKE_BINARY_DIR}/postinst" "${CMAKE_BINARY_DIR}/postrm")
endif()

set(CPACK_PACKAGE_NAME "${APP_ID}")
set(CPACK_PACKAGE_VENDOR "waterfall")
set(CPACK_PACKAGE_CONTACT "waterfall@localhost")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Waterfall")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${APP_DISPLAY_NAME}")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_MONOLITHIC_INSTALL ON)
    set(CPACK_NSIS_DISPLAY_NAME "${APP_DISPLAY_NAME}")
    set(CPACK_NSIS_PACKAGE_NAME "${APP_DISPLAY_NAME}")
    set(CPACK_NSIS_CONTACT "waterfall@localhost")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_INSTALL_ALL_USERS ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_DEFINES "RequestExecutionLevel admin")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\${APP_EXE_NAME}.exe")
    set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS
"DetailPrint 'Stopping running ${APP_DISPLAY_NAME} instances (if any)...'
nsExec::ExecToLog 'taskkill /F /IM ${APP_EXE_NAME}.exe'
Sleep 500"
    )
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
"CreateDirectory '\$SMPROGRAMS\\\\${APP_DISPLAY_NAME}'
CreateShortCut '\$SMPROGRAMS\\\\${APP_DISPLAY_NAME}\\\\${APP_DISPLAY_NAME}.lnk' '\$INSTDIR\\\\bin\\\\${APP_EXE_NAME}.exe'
CreateShortCut '\$DESKTOP\\\\${APP_DISPLAY_NAME}.lnk' '\$INSTDIR\\\\bin\\\\${APP_EXE_NAME}.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
"Delete '\$SMPROGRAMS\\\\${APP_DISPLAY_NAME}\\\\${APP_DISPLAY_NAME}.lnk'
RMDir '\$SMPROGRAMS\\\\${APP_DISPLAY_NAME}'
Delete '\$DESKTOP\\\\${APP_DISPLAY_NAME}.lnk'"
    )
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Waterfall Maintainers <waterfall@localhost>")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
            "${CMAKE_BINARY_DIR}/postinst;${CMAKE_BINARY_DIR}/postrm"
    )
endif()

include(CPack)
