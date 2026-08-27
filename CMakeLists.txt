cmake_minimum_required(VERSION 3.16)
project(matbin VERSION 1.3.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 1. Discover Qt6 Core, Widgets, SVG, and Concurrent
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg Concurrent)

# 2. Configure OS Desktop / Executable Icons
set(APP_ICON_RESOURCE "")

if(WIN32)
    # Windows RC Resource file linking to .ico
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/appicon.rc")
elseif(APPLE)
    # macOS Bundle Info.plist icon link
    set(MACOSX_BUNDLE_ICON_FILE appicon.icns)
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/appicon.icns")
    set_source_files_properties(${APP_ICON_RESOURCE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

# Configure Matio build options before including third_party/matio
set(MATIO_MAT73 OFF CACHE BOOL "Disable MAT v7.3 support" FORCE)
set(MATIO_WITH_HDF5 OFF CACHE BOOL "Disable HDF5 support" FORCE)
set(MATIO_WITH_ZLIB OFF CACHE BOOL "Disable zlib support" FORCE)
set(MATIO_SHARED OFF CACHE BOOL "Build Matio as static library" FORCE)
set(MATIO_PIC ON CACHE BOOL "Build Matio with Position Independent Code" FORCE)
set(MATIO_BUILD_TESTING OFF CACHE BOOL "Disable Matio tests" FORCE)
set(MATIO_BUILD_TOOLS OFF CACHE BOOL "Disable Matio CLI tools" FORCE)
set(MATIO_TOOLS OFF CACHE BOOL "Disable Matio CLI tools" FORCE)

add_subdirectory(third_party/matio)

# Create compatibility alias for matio::matio
if(NOT TARGET matio::matio)
    if(TARGET matio)
        add_library(matio::matio ALIAS matio)
    elseif(TARGET matio_static)
        add_library(matio::matio ALIAS matio_static)
    endif()
endif()

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/BinToMatConverter.cpp
)

set(HEADERS
    src/MainWindow.h
    src/BinToMatConverter.h
)

# 3. Add Executable including OS Icon Resource (WIN32 hides the console terminal window)
add_executable(matbin WIN32 MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${APP_ICON_RESOURCE})

# 4. Add In-App SVG Icons using Qt Resource System
qt_add_resources(matbin "app_icons"
    PREFIX "/icons"
    FILES
        resources/icons/browse.svg
        resources/icons/convert.svg
        resources/icons/clear.svg
        resources/icons/folder.svg
        resources/icons/app_logo.svg
)

target_link_libraries(matbin
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Svg
        Qt6::Concurrent
        matio::matio
)

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "C:/Qt/6.11.1/msvc2022_64/bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET matbin POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:matbin>"
            COMMENT "Deploying Qt runtime libraries with windeployqt..."
        )
    endif()
endif()