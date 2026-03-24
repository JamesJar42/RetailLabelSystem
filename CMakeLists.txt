# QtIFW desktop shortcut configuration
# Optional build-time custom shortcut icon (path to a .ico file). This is intended
# for builders to supply a per-build icon via -DCPACK_SHORTCUT_ICON=path/to/icon.ico
set(CPACK_SHORTCUT_ICON "" CACHE PATH "Optional .ico used for Start Menu / Desktop shortcuts (build-time)")

# Do not hard-code absolute paths. Allow users to provide paths via CMake cache variables.
# Recommend users pass -DQt6_ROOT=... and -DOpenCV_DIR=... when configuring if needed.
set(Qt6_ROOT "" CACHE PATH "Path to Qt6 installation root (optional). If empty CMake will try to find Qt automatically.")
set(OpenCV_DIR "" CACHE PATH "Path to OpenCV install directory (optional). If empty CMake will try to find OpenCV automatically.")


cmake_minimum_required(VERSION 3.16)

project(RetailLabeler VERSION 1.0.0 LANGUAGES CXX)

add_compile_definitions(RETAIL_LABELER_VERSION="${PROJECT_VERSION}")

# if(NOT CPACK_SHORTCUT_ICON)
#     set(CPACK_SHORTCUT_ICON "${CMAKE_SOURCE_DIR}/resources/logo.ico")
# endif()

# If the builder supplied an icon, install it into the package bin/ directory
# if(CPACK_SHORTCUT_ICON)
#     if(EXISTS "${CPACK_SHORTCUT_ICON}")
#         message(STATUS "Using custom shortcut icon: ${CPACK_SHORTCUT_ICON}")
#         # Install the icon into the package bin folder and normalize the filename
#         get_filename_component(_shortcut_icon_name "${CPACK_SHORTCUT_ICON}" NAME)
#         # Install with a predictable name to avoid path issues
#         install(FILES "${CPACK_SHORTCUT_ICON}" DESTINATION bin RENAME "RetailLabeler.ico" COMPONENT application)
#     else()
#         message(WARNING "CPACK_SHORTCUT_ICON set but file not found: ${CPACK_SHORTCUT_ICON}")
#     endif()
# endif()

# Install resources directory for install/package
install(DIRECTORY "${CMAKE_SOURCE_DIR}/resources" DESTINATION bin COMPONENT application)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use optional environment variables when cache variables were not supplied.
if(NOT Qt6_ROOT)
    if(DEFINED ENV{Qt6_ROOT} AND EXISTS "$ENV{Qt6_ROOT}")
        set(Qt6_ROOT "$ENV{Qt6_ROOT}" CACHE PATH "Path to Qt6 installation root" FORCE)
    endif()
endif()

if(NOT OpenCV_DIR)
    if(DEFINED ENV{OpenCV_DIR} AND EXISTS "$ENV{OpenCV_DIR}")
        set(OpenCV_DIR "$ENV{OpenCV_DIR}" CACHE PATH "Path to OpenCV install directory" FORCE)
    endif()
endif()

if(Qt6_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${Qt6_ROOT}")
endif()
if(OpenCV_DIR)
    list(PREPEND CMAKE_PREFIX_PATH "${OpenCV_DIR}")
endif()


# Set these to your actual install locations if needed
# If the user provided OpenCV_DIR above, derive bin dirs; otherwise leave as empty cache entries
if(OpenCV_DIR)
    set(OpenCV_BIN_DIR_RELEASE "${OpenCV_DIR}/x64/vc17/bin")
    set(OpenCV_BIN_DIR_DEBUG "${OpenCV_DIR}/x64/vc17/bin")
else()
    set(OpenCV_BIN_DIR_RELEASE "" CACHE PATH "Optional OpenCV release bin directory (used by installer step)")
    set(OpenCV_BIN_DIR_DEBUG "" CACHE PATH "Optional OpenCV debug bin directory (used by installer step)")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Network Test)


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
    
    # configure_file("${CMAKE_SOURCE_DIR}/resources/RetailLabeler.rc.in" "${CMAKE_BINARY_DIR}/RetailLabeler.rc" @ONLY)
    # target_sources(RetailLabeler PRIVATE "${CMAKE_BINARY_DIR}/RetailLabeler.rc")

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
    src/PrintDialog.cpp
    src/ProductDelegate.cpp
    include/PrintDialog.h
    include/ProductDelegate.h
)

target_include_directories(retail_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(retail_core PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport Qt6::Network ${OpenCV_LIBS})

# Enable CTest and add unit tests
enable_testing()

# Non-GUI unit tests split by feature area
function(add_core_logic_test TARGET_NAME TEST_MAIN TEST_IMPL)
    add_executable(${TARGET_NAME}
        ${TEST_MAIN}
        ${TEST_IMPL}
        src/labelSystem.cpp
        src/shop.cpp
        src/product.cpp
        src/Menu.cpp
        src/PrintDialog.cpp
        src/ProductDelegate.cpp
        include/PrintDialog.h
        include/ProductDelegate.h
    )

    target_link_libraries(${TARGET_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
        Qt6::Network
        ${OpenCV_LIBS}
    )

    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
    )

    target_link_libraries(${TARGET_NAME} PRIVATE retail_core)
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
endfunction()

add_core_logic_test(product_tests tests/test_product_main.cpp tests/test_product.cpp)
add_core_logic_test(shop_tests tests/test_shop_main.cpp tests/test_shop.cpp)
add_core_logic_test(labelsystem_queue_tests tests/test_labelSystem_queue_main.cpp tests/test_labelSystem_queue.cpp)
add_core_logic_test(labelsystem_files_tests tests/test_labelSystem_files_main.cpp tests/test_labelSystem_files.cpp)

# Qt GUI tests for MainWindow interaction behavior (split by feature area)
function(add_mainwindow_gui_test TARGET_NAME TEST_SOURCE)
    add_executable(${TARGET_NAME}
        ${TEST_SOURCE}
        src/AppUpdater.cpp
        src/MainWindow.cpp
        src/ConfigEditorDialog.cpp
        src/DatabaseSettingsDialog.cpp
        src/VisualSettingsDialog.cpp
        src/PrintDialog.cpp
        src/ProductDelegate.cpp
        src/labelSystem.cpp
        src/shop.cpp
        src/product.cpp
        src/Menu.cpp
        include/MainWindow.h
        include/AppUpdater.h
        include/ConfigEditorDialog.h
        include/DatabaseSettingsDialog.h
        include/VisualSettingsDialog.h
        include/PrintDialog.h
        include/ProductDelegate.h
    )

    target_link_libraries(${TARGET_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
        Qt6::Network
        Qt6::Test
        ${OpenCV_LIBS}
    )

    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
    )

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /FS)
    endif()

    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
endfunction()

add_mainwindow_gui_test(mainwindow_gui_filters_tests tests/test_mainwindow_gui_filters.cpp)
add_mainwindow_gui_test(mainwindow_gui_shortcuts_tests tests/test_mainwindow_gui_shortcuts.cpp)
add_mainwindow_gui_test(mainwindow_gui_actions_tests tests/test_mainwindow_gui_actions.cpp)

# Copy resources to build output directories for local debug/run, excluding Database.txt
file(GLOB RESOURCE_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/resources/*")
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
    Qt6::Network
    Qt6::Test
    ${OpenCV_LIBS}
)

# Install target
install(TARGETS RetailLabeler
    RUNTIME DESTINATION bin
    COMPONENT application
)

    # -------------------
    # Robustly deploy Qt and OpenCV DLLs
    # -------------------
    if(WIN32)
        # Path to windeployqt
        set(WINDEPLOYQT_EXE "${Qt6_ROOT}/bin/windeployqt.exe")
        
        # 1. Run windeployqt POST_BUILD for local debugging convenience
        add_custom_command(TARGET RetailLabeler POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}" "$<TARGET_FILE:RetailLabeler>" --dir "$<TARGET_FILE_DIR:RetailLabeler>" $<$<CONFIG:Debug>:--debug> $<$<CONFIG:Release>:--release> --no-translations
            COMMENT "Running windeployqt to collect Qt dependencies for correct config"
        )

        # 2. Run windeployqt during INSTALL for packaging
        # This ensures the package receives a self-contained bin folder
        install(CODE "
            message(STATUS \"Deploying Qt dependencies to install prefix...\")
            execute_process(
                COMMAND \"${WINDEPLOYQT_EXE}\" \"\${CMAKE_INSTALL_PREFIX}/bin/RetailLabeler.exe\" --no-translations --compiler-runtime
            )
        " COMPONENT application)
        
        # 3. Install OpenCV DLLs
        # These exist at configure time, so file(GLOB) is safe here
        file(GLOB OPENCV_DLLS
            "${OpenCV_BIN_DIR_RELEASE}/opencv_*.dll"
        )
        install(FILES ${OPENCV_DLLS} DESTINATION bin COMPONENT application)

        # Always try to install Qt6Test.dll (CMake will skip if not found)
        install(FILES "${Qt6_ROOT}/bin/Qt6Test.dll" DESTINATION bin COMPONENT application)
    endif()
    
# -------------------
# CPack configuration
# -------------------
# Explicitly install system runtimes (MSVCP140.dll, etc.) into the 'application' component
# because the default rule uses the 'Unspecified' component, which is excluded.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP ON)
include(InstallRequiredSystemLibraries)
install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION bin COMPONENT application)

set(CPACK_PACKAGE_NAME "RetailLabeler")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "RetailLabeler - Retail Label System")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "jlyttle26@outlook.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_EXECUTABLES "RetailLabeler" "RetailLabeler")



# -------------------
# CPack configuration (improved defaults)
# -------------------

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

# Qt Installer Framework location (optional)
set(QTIFW_ROOT "" CACHE PATH "Path to Qt Installer Framework (optional)")
if(NOT QTIFW_ROOT AND DEFINED ENV{QTIFW_ROOT} AND EXISTS "$ENV{QTIFW_ROOT}")
    set(QTIFW_ROOT "$ENV{QTIFW_ROOT}" CACHE PATH "Path to Qt Installer Framework (optional)" FORCE)
endif()
if(EXISTS "${QTIFW_ROOT}")
    set(CPACK_IFW_ROOT "${QTIFW_ROOT}")
endif()

if(WIN32)
    set(CPACK_GENERATOR "IFW")
    
    # QtIFW Configuration
    set(CPACK_IFW_PACKAGE_GROUP "RetailLabeler")
    set(CPACK_IFW_PACKAGE_NAME "RetailLabeler")
    set(CPACK_IFW_PACKAGE_TITLE "Retail Labeler System")
    set(CPACK_IFW_PACKAGE_PUBLISHER "JamesJar42")
    set(CPACK_IFW_PRODUCT_VERSION "${PROJECT_VERSION}")
    # Explicitly clear installer icon settings so no custom logo is shown.
    set(CPACK_IFW_PACKAGE_ICON "")
    set(CPACK_IFW_PACKAGE_WINDOW_ICON "")
    set(CPACK_IFW_PACKAGE_START_MENU_DIRECTORY "RetailLabeler")
    set(CPACK_IFW_PACKAGE_CONTROL_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/controlscript.qs")
        
    set(CPACK_COMPONENTS_ALL application desktop_shortcut)
endif()

# Include CPackComponent to ensure we can use its macros before CPack module is potentially included
include(CPackComponent)

if(WIN32)
    # Define Application Component (Core)
    cpack_add_component(application
        DISPLAY_NAME "Retail Labeler Core"
        DESCRIPTION "The core application and dependencies."
        REQUIRED
    )

    # Define Desktop Shortcut Component properly
    # Removing 'DISABLED' and 'GROUP' to ensure it appears checked and visible in the root list
    cpack_add_component(desktop_shortcut
        DISPLAY_NAME "Desktop Shortcut"
        DESCRIPTION "Create a shortcut on the Windows Desktop."
    )
    
    # Add a dummy file so CPack generates the component package
    install(FILES "${CMAKE_SOURCE_DIR}/README.txt" DESTINATION bin COMPONENT desktop_shortcut)
endif()


include(CPack)
include(CPackIFW)

if(WIN32)
    # 1. Main Application Component (Required)
    cpack_ifw_configure_component(application
        SCRIPT "${CMAKE_SOURCE_DIR}/cmake/installscript.qs"
        DISPLAY_NAME "Retail Labeler Core"
        DESCRIPTION "The main application executables and libraries."
        REQUIRED
        SORTING_PRIORITY 90
    )

    # 2. Desktop Shortcut Component (Optional, Default Checked)
    cpack_ifw_configure_component(desktop_shortcut
        SCRIPT "${CMAKE_SOURCE_DIR}/cmake/desktopshortcut.qs"
        DISPLAY_NAME "Desktop Shortcut"
        DESCRIPTION "Create a shortcut on the Windows Desktop."
        DEFAULT "true"
        SORTING_PRIORITY 80
    )
endif()

