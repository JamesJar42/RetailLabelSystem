# NSIS desktop shortcut configuration for CPack
## Ensure CPack NSIS variables are explicitly set in the CACHE so they override
## any previously cached values from earlier configure runs.
## Configure desktop/start-menu shortcuts for NSIS packaging
# Use well-formed, cached CPACK_NSIS variables so CPack writes valid CreateShortCut calls.
set(CPACK_NSIS_CREATE_DESKTOP_LINKS ON CACHE BOOL "Create desktop shortcuts (NSIS)")
# Executables are installed to the 'bin' directory by our install commands
set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin" CACHE STRING "Executables directory within package (relative to install dir)")
# Menu links: semicolon-separated entries of the form "relative/path/to/exe;Display Name"
set(CPACK_NSIS_MENU_LINKS "${CPACK_NSIS_EXECUTABLES_DIRECTORY}/RetailLabeler.exe;RetailLabeler" CACHE STRING "Start Menu link entries")
# Shortcut target should reference the executable path inside the package (use forward slashes)
set(CPACK_NSIS_SHORTCUT_NAME "RetailLabeler" CACHE STRING "Shortcut name")
set(CPACK_NSIS_SHORTCUT_TARGET "${CPACK_NSIS_EXECUTABLES_DIRECTORY}/RetailLabeler.exe" CACHE STRING "Shortcut target executable relative to install dir")
# No extra custom CreateShortCut entries by default
set(CPACK_NSIS_CREATE_ICONS_EXTRA "" CACHE STRING "Extra CreateShortCut entries (empty)")

# Optional build-time custom shortcut icon (path to a .ico file). This is intended
# for builders to supply a per-build icon via -DCPACK_SHORTCUT_ICON=path/to/icon.ico
set(CPACK_SHORTCUT_ICON "" CACHE PATH "Optional .ico used for Start Menu / Desktop shortcuts (build-time)")

# If the builder supplied an icon, install it into the package bin/ directory and
# instruct CPack/NSIS to create shortcuts that use the icon.
if(CPACK_SHORTCUT_ICON)
    if(EXISTS "${CPACK_SHORTCUT_ICON}")
        message(STATUS "Using custom shortcut icon: ${CPACK_SHORTCUT_ICON}")
        # Install the icon into the package bin folder and normalize the filename
        get_filename_component(_shortcut_icon_name "${CPACK_SHORTCUT_ICON}" NAME)
        # Install with a predictable name to avoid path issues
        install(FILES "${CPACK_SHORTCUT_ICON}" DESTINATION bin RENAME "RetailLabeler.ico")
    # Tell NSIS to create Start Menu and Desktop shortcuts using the installed icon
    # Build the NSIS commands with escaped $ (use $$ in CMake to keep literal $ in generated file)
    # Use forward slashes to avoid backslash escape issues when CMake writes the
    # value into the cache file (which would otherwise create sequences like \R).
    set(_nsis_line1 "CreateShortCut \"$$SMPROGRAMS/$$STARTMENU_FOLDER/RetailLabeler.lnk\" \"$$INSTDIR/bin/RetailLabeler.exe\" \"\" \"$$INSTDIR/bin/RetailLabeler.ico\" 0 SW_SHOWNORMAL \"\" \"$$INSTDIR/bin\"")
    set(_nsis_line2 "CreateShortCut \"$$DESKTOP/RetailLabeler.lnk\" \"$$INSTDIR/bin/RetailLabeler.exe\" \"\" \"$$INSTDIR/bin/RetailLabeler.ico\" 0 SW_SHOWNORMAL \"\" \"$$INSTDIR/bin\"")
    # Combine lines but escape the newline as a literal "\n" so CMake's cache
    # (which truncates on actual newlines) stores the full string reliably.
    string(CONCAT _nsis_full_escaped "${_nsis_line1}" "\\n" "${_nsis_line2}")
    # Overwrite any existing cached value so the icon-backed CreateShortCut entries
    # are actually written into the generated CPack/NSIS script. Use FORCE so
    # repeated config runs reliably update the cache.
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "${_nsis_full_escaped}" CACHE STRING "Extra CreateShortCut entries (with icon)" FORCE)
    else()
        message(WARNING "CPACK_SHORTCUT_ICON set but file not found: ${CPACK_SHORTCUT_ICON}")
    endif()
endif()
# Do not hard-code absolute paths. Allow users to provide paths via CMake cache variables.
# Recommend users pass -DQt6_ROOT=... and -DOpenCV_DIR=... when configuring if needed.
set(Qt6_ROOT "" CACHE PATH "Path to Qt6 installation root (optional). If empty CMake will try to find Qt automatically.")
set(OpenCV_DIR "" CACHE PATH "Path to OpenCV install directory (optional). If empty CMake will try to find OpenCV automatically.")
# Install resources directory for install/package
install(DIRECTORY "${CMAKE_SOURCE_DIR}/resources" DESTINATION bin)


cmake_minimum_required(VERSION 3.16)

project(RetailLabeler VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Set these to your actual install locations if needed
# If the user provided OpenCV_DIR above, derive bin dirs; otherwise leave as empty cache entries
if(OpenCV_DIR)
    set(OpenCV_BIN_DIR_RELEASE "${OpenCV_DIR}/x64/vc17/bin")
    set(OpenCV_BIN_DIR_DEBUG "${OpenCV_DIR}/x64/vc17/bin")
else()
    set(OpenCV_BIN_DIR_RELEASE "" CACHE PATH "Optional OpenCV release bin directory (used by installer step)")
    set(OpenCV_BIN_DIR_DEBUG "" CACHE PATH "Optional OpenCV debug bin directory (used by installer step)")
endif()

# Optionally set OpenCV debug/release bin directories
set(OpenCV_BIN_DIR_RELEASE "${OpenCV_DIR}/x64/vc17/bin")
set(OpenCV_BIN_DIR_DEBUG "${OpenCV_DIR}/x64/vc17/bin")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Test)


# Find OpenCV for both Debug and Release
find_package(OpenCV REQUIRED)

# If user provided OpenCV_DIR, add its lib folder to the link search path so MSVC linker can find .lib files
if(OpenCV_DIR)
    set(_opencv_lib_dir "${OpenCV_DIR}/x64/vc17/lib")
    if(EXISTS "${_opencv_lib_dir}")
        message(STATUS "Adding OpenCV lib dir to linker search path: ${_opencv_lib_dir}")
        link_directories("${_opencv_lib_dir}")
    endif()
endif()

# Prefer OpenCV's provided variables/targets. Only fall back to manual names if OpenCV didn't populate OpenCV_LIBS.
if(MSVC AND NOT DEFINED OpenCV_LIBS)
    set(OpenCV_LIBS_RELEASE
        opencv_core4120.lib
        opencv_imgproc4120.lib
        opencv_highgui4120.lib
        opencv_imgcodecs4120.lib
        opencv_videoio4120.lib
        opencv_objdetect4120.lib
        opencv_features2d4120.lib
        opencv_calib3d4120.lib
    )
    set(OpenCV_LIBS_DEBUG
        opencv_core4120d.lib
        opencv_imgproc4120d.lib
        opencv_highgui4120d.lib
        opencv_imgcodecs4120d.lib
        opencv_videoio4120d.lib
        opencv_objdetect4120d.lib
        opencv_features2d4120d.lib
        opencv_calib3d4120d.lib
    )
    set(OpenCV_LIBS
        $<$<CONFIG:Debug>:${OpenCV_LIBS_DEBUG}>
        $<$<CONFIG:Release>:${OpenCV_LIBS_RELEASE}>
    )
endif()

# Enable Qt's automatic processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

qt_standard_project_setup()

# Set the main build output directory to 'build'
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)


# Collect all source and header files
file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS include/*.h)
# Include Designer forms so AUTOUIC processes them
file(GLOB_RECURSE UI_FILES forms/*.ui)
# Include Qt resource collections so AUTORCC generates qInitResources_* symbols
file(GLOB_RECURSE QRC_FILES resources/*.qrc)


qt_add_executable(RetailLabeler
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
    ${QRC_FILES}
)
    # Configure the resource script from template so the .rc references a
    # valid absolute path to the icon. This avoids CMake variables ending up in
    # the .rc which the resource compiler cannot resolve.
    configure_file("${CMAKE_SOURCE_DIR}/resources/RetailLabeler.rc.in" "${CMAKE_BINARY_DIR}/RetailLabeler.rc" @ONLY)
    target_sources(RetailLabeler PRIVATE "${CMAKE_BINARY_DIR}/RetailLabeler.rc")

# Build the Release executable as a GUI app (no console) so running the Release exe
# doesn't open a terminal window on Windows. Debug builds keep the console for
# diagnostics.
set_target_properties(RetailLabeler PROPERTIES WIN32_EXECUTABLE $<$<CONFIG:Release>:TRUE>)

# Core library for non-GUI logic so tests can link implementation
add_library(retail_core STATIC
    src/labelSystem.cpp
    src/shop.cpp
    src/product.cpp
    src/Menu.cpp
)

target_include_directories(retail_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(retail_core PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport ${OpenCV_LIBS})

# Enable CTest and add unit tests
enable_testing()

# Test executable
add_executable(labelSystem_tests
    tests/test_labelSystem.cpp
    src/labelSystem.cpp
    src/shop.cpp
    src/product.cpp
    src/Menu.cpp
)

target_link_libraries(labelSystem_tests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    ${OpenCV_LIBS}
)

target_include_directories(labelSystem_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(labelSystem_tests PRIVATE retail_core)

add_test(NAME labelSystem_tests COMMAND labelSystem_tests)

# Copy resources to build output directories for local debug/run, excluding Database.txt
file(GLOB RESOURCE_FILES "${CMAKE_SOURCE_DIR}/resources/*")
foreach(res_file ${RESOURCE_FILES})
    get_filename_component(fname "${res_file}" NAME)
    if(NOT fname STREQUAL "Database.txt")
        add_custom_command(TARGET RetailLabeler POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${res_file}"
                "$<TARGET_FILE_DIR:RetailLabeler>/resources/${fname}"
            COMMENT "Copying resource file if different: ${fname}"
        )
    endif()
endforeach()


target_include_directories(RetailLabeler PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RetailLabeler PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::Test
    ${OpenCV_LIBS}
)

    # -------------------
    # Robustly deploy Qt and OpenCV DLLs
    # -------------------
    if(WIN32)
        # Path to windeployqt
        set(WINDEPLOYQT_EXE "${Qt6_ROOT}/bin/windeployqt.exe")
        add_custom_command(TARGET RetailLabeler POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}" "$<TARGET_FILE:RetailLabeler>" --dir "$<TARGET_FILE_DIR:RetailLabeler>" $<$<CONFIG:Debug>:--debug> $<$<CONFIG:Release>:--release>
            COMMENT "Running windeployqt to collect Qt dependencies for correct config"
        )
        # Install all DLLs from build output to install/bin
        file(GLOB BUILD_DLLS
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/*.dll"
        )
        install(FILES ${BUILD_DLLS} DESTINATION bin)

        # Install all Qt plugin directories created by windeployqt
        foreach(plugin_dir platforms imageformats iconengines generic networkinformation platforminputcontexts styles tls)
            if(EXISTS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${plugin_dir}")
                install(DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${plugin_dir}" DESTINATION bin)
            endif()
        endforeach()
        # Install all OpenCV DLLs found in the bin directory
        file(GLOB OPENCV_DLLS
            "${OpenCV_BIN_DIR_RELEASE}/opencv_*.dll"
        )
        install(FILES ${OPENCV_DLLS} DESTINATION bin)

        # Always try to install Qt6Test.dll (CMake will skip if not found)
        install(FILES "${Qt6_ROOT}/bin/Qt6Test.dll" DESTINATION bin)
    endif()
    
# -------------------
# CPack configuration
# -------------------
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_NAME "RetailLabeler")
set(CPACK_PACKAGE_VENDOR "YourCompany")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "RetailLabeler - Retail Label System")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "youremail@example.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_EXECUTABLES "RetailLabeler" "RetailLabeler")

# Install target
install(TARGETS RetailLabeler
    RUNTIME DESTINATION bin
)

# -------------------
# CPack configuration (improved defaults)
# -------------------
# Prefer NSIS for Windows packaging; users can override by setting CPACK_GENERATOR
if(NOT DEFINED CPACK_GENERATOR)
    set(CPACK_GENERATOR "NSIS")
endif()

# Build a predictable package file name including version and arch
if(NOT DEFINED CPACK_PACKAGE_FILE_NAME)
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-win64")
endif()

# Ignore common build artefacts and VCS metadata when creating source packages
if(NOT DEFINED CPACK_SOURCE_IGNORE_FILES)
    set(CPACK_SOURCE_IGNORE_FILES "/build/;\\.git/;\\.vs/;~$;\\.DS_Store$")
else()
    list(APPEND CPACK_SOURCE_IGNORE_FILES "/build/" "\\.git/" "\\.vs/" "~$" "\\.DS_Store$")
endif()

if(WIN32)
    # NSIS-friendly options
    set(CPACK_NSIS_DISPLAY_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_NSIS_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_NSIS_CREATE_DESKTOP_LINKS ON)
    set(CPACK_NSIS_SHORTCUT_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_NSIS_SHORTCUT_TARGET "${CPACK_PACKAGE_EXECUTABLES}")
    # Ensure installer shows proper product version
# Ensure NSIS-related CPACK cache entries are well-formed and overwrite any
# previously cached malformed values (this avoids makensis failing due to bad
# CreateShortCut lines injected into the generated script).
set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin" CACHE STRING "Executables directory within package (relative to install dir)" FORCE)
set(CPACK_NSIS_MENU_LINKS "${CPACK_NSIS_EXECUTABLES_DIRECTORY}/RetailLabeler.exe;RetailLabeler" CACHE STRING "Start Menu link entries" FORCE)
set(CPACK_NSIS_SHORTCUT_TARGET "${CPACK_NSIS_EXECUTABLES_DIRECTORY}/RetailLabeler.exe" CACHE STRING "Shortcut target executable relative to install dir" FORCE)
set(CPACK_NSIS_CREATE_ICONS_EXTRA "" CACHE STRING "Extra CreateShortCut entries (empty)" FORCE)

# Re-include CPack after forcing sanitized CPACK_NSIS variables so the
# generated CPackConfig.cmake is written with the corrected values (avoids
# stale/malformed cached entries producing invalid NSIS scripts).
endif()

# Ensure CPACK_NSIS_EXTRA_INSTALL_COMMANDS is empty so no malformed value is
# written into CPackConfig.cmake. Shortcuts are configured via
# CPACK_NSIS_CREATE_DESKTOP_LINKS and CPACK_NSIS_CREATE_ICONS_EXTRA above.
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "" CACHE STRING "Extra NSIS install commands (empty)" FORCE)

# Prefer our custom installer UI so we can have the Desktop icon checkbox
# default to checked. Place this before including CPack so the value is
# recorded into the generated CPack configuration at configure time.
configure_file("${CMAKE_SOURCE_DIR}/cmake/NSIS.InstallOptions.ini" "${CMAKE_BINARY_DIR}/NSIS.InstallOptions.ini" COPYONLY)
set(CPACK_NSIS_INSTALLER_UI "${CMAKE_BINARY_DIR}/NSIS.InstallOptions.ini" CACHE PATH "Custom NSIS installer UI (InstallOptions.ini)" FORCE)

include(CPack)


# Optionally, add resources if needed
# qt_add_resources(RetailLabeler resources/labelTemplate.qrc)