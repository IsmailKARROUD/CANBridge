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
    VERSION 0.2.0
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
