cmake_minimum_required(VERSION 3.19)
project(CANBridge LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)

qt_standard_project_setup()

qt_add_executable(CANBridge
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    canframe.h
    canframe.cpp
    tcpserver.h
    tcpserver.cpp
    messagemodel.h
    messagemodel.cpp
    aboutdialog.h
    aboutdialog.cpp


    mainwindow.ui
    tcpclient.h tcpclient.cpp
)

target_link_libraries(CANBridge
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Network
)

include(GNUInstallDirs)

install(TARGETS CANBridge
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET CANBridge
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


project(CANBridge
    VERSION 0.1.0
    LANGUAGES CXX
)

# Configure a header file to pass version to source code
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
)

# Make sure the generated header is in include path
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
