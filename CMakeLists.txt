# ==============================================================================
# CANBridge - CMake Build Configuration
#
# Builds the CANBridge Qt 6 desktop application for CAN bus communication
# over TCP/IP networks. Requires Qt 6.5+ with Core, Widgets, and Network modules.
# ==============================================================================

cmake_minimum_required(VERSION 3.19)
project(CANBridge LANGUAGES CXX)

# Locate required Qt 6 modules
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)

# Apply standard Qt project setup (handles MOC, UIC, RCC automatically)
qt_standard_project_setup()

# Define the executable target with all source files
qt_add_executable(CANBridge
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/core/CANFrame.h
    src/core/CANFrame.cpp
    src/network/tcpserver.h
    src/network/tcpserver.cpp
    src/network/tcpclient.h
    src/network/tcpclient.cpp
    src/models/messagemodel.h
    src/models/messagemodel.cpp
    src/models/framefilterproxymodel.h
    src/models/framefilterproxymodel.cpp
    src/ui/mainwindow.h
    src/ui/mainwindow.cpp
    src/ui/mainwindow_menubar.cpp
    src/ui/mainwindow.ui
    src/ui/aboutdialog.h
    src/ui/aboutdialog.cpp
    src/ui/framewidget.h
    src/ui/framewidget.cpp
)

# Link against Qt libraries
target_link_libraries(CANBridge
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Network
)

# Use GNU standard install directories (bin, lib, etc.)
include(GNUInstallDirs)

# Install the application binary (supports macOS .app bundles)
install(TARGETS CANBridge
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Generate a platform-specific deployment script for bundling Qt libraries
qt_generate_deploy_app_script(
    TARGET CANBridge
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# Require C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set project version (used by version.h.in template)
project(CANBridge
    VERSION 0.3.0
    LANGUAGES CXX
)

# Generate version.h from template, injecting CMake version variables
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
)

# Add source subdirectories and build directory to include path
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${CMAKE_CURRENT_BINARY_DIR}"
)

# Shared metadata used by both macOS bundle and Windows version resource
set(PRODUCT_NAME     "CANBridge")
set(COPYRIGHT        "Copyright © 2026 Ismail KARROUD")

# Qt resource system — embeds logo.png for in-app use (e.g. About dialog)
qt_add_resources(CANBridge "appResources"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/logo.png
)

# macOS bundle: app icon (.icns) + custom Info.plist
set(MACOSX_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/logo.icns")
set_source_files_properties(${MACOSX_ICON} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
)
target_sources(CANBridge PRIVATE ${MACOSX_ICON})
set_target_properties(CANBridge PROPERTIES
    MACOSX_BUNDLE_ICON_FILE    logo.icns
    MACOSX_BUNDLE_INFO_PLIST   "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
)

if(WIN32)
    set(COMPANY_NAME     "Ismail KARROUD")
    set(FILE_DESCRIPTION "CAN Bus Analyzer and Simulator")
    set(APP_ICON_PATH    "${CMAKE_CURRENT_SOURCE_DIR}/resources/logo.ico")

    # Generate the Windows version-resource file from the template
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
        @ONLY
    )

    # Append the resource to the existing target (do NOT call add_executable again)
    target_sources(CANBridge PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
endif()
