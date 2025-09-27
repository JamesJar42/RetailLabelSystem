# NSIS desktop shortcut configuration for CPack
set(CPACK_NSIS_CREATE_DESKTOP_LINKS ON)
set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
set(CPACK_NSIS_SHORTCUT_NAME "RetailLabeler")
set(CPACK_NSIS_SHORTCUT_TARGET "RetailLabeler.exe")
set(CPACK_NSIS_MENU_LINKS "bin/RetailLabeler.exe" "RetailLabeler")
set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut \"$DESKTOP\\RetailLabeler.lnk\" \"$INSTDIR\\bin\\RetailLabeler.exe\"")
# Ensure the linker can find OpenCV .lib files
link_directories("D:/opencv/build/install/x64/vc17/lib")
# Install resources directory for install/package
install(DIRECTORY "${CMAKE_SOURCE_DIR}/resources" DESTINATION bin)


cmake_minimum_required(VERSION 3.16)

project(RetailLabeler VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Set these to your actual install locations if needed
set(Qt6_ROOT "D:/Qt/6.7.3/msvc2022_64")
set(OpenCV_DIR "D:/opencv/build/install")

# Optionally set OpenCV debug/release bin directories
set(OpenCV_BIN_DIR_RELEASE "${OpenCV_DIR}/x64/vc17/bin")
set(OpenCV_BIN_DIR_DEBUG "${OpenCV_DIR}/x64/vc17/bin")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Test)


# Find OpenCV for both Debug and Release
find_package(OpenCV REQUIRED)

# Manually set OpenCV libraries for Debug and Release (individual modules)
if(MSVC)
    set(OpenCV_LIBS_RELEASE
        opencv_core4120.lib
        opencv_imgproc4120.lib
        opencv_highgui4120.lib
        opencv_imgcodecs4120.lib
        opencv_videoio4120.lib
        opencv_objdetect4120.lib
        opencv_features2d4120.lib
        opencv_calib3d4120.lib
        # add/remove modules as needed
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
        # add/remove modules as needed
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


qt_add_executable(RetailLabeler
    ${SOURCES}
    ${HEADERS}
)

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
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_EXECUTABLES "RetailLabeler" "RetailLabeler")

# Install target
install(TARGETS RetailLabeler
    RUNTIME DESTINATION bin
)

include(CPack)


# Optionally, add resources if needed
# qt_add_resources(RetailLabeler resources/labelTemplate.qrc)