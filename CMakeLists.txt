cmake_minimum_required(VERSION 3.16)
project(ESP32QtApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt packages (Qt6 prioritized, Qt5 fallback)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Concurrent)

# Windows WinRT support flags when using MSVC
if(MSVC)
    add_compile_options(/permissive- /Zc:twoPhase- /EHsc)
    # Enable C++/WinRT coroutine support if needed
    if(MSVC_VERSION GREATER_EQUAL 1920)
        add_compile_options(/await:strict)
    else()
        add_compile_options(/await)
    endif()
endif()

set(APP_SOURCES
    src/main.cpp
    src/common/AppSettings.h
    src/common/AppSettings.cpp
    src/common/LogService.h
    src/common/LogService.cpp
    src/network/Protocol.h
    src/network/Protocol.cpp
    src/network/UdpManager.h
    src/network/UdpManager.cpp
    src/devices/DeviceRecord.h
    src/devices/DeviceStore.h
    src/devices/DeviceStore.cpp
    src/devices/DeviceManager.h
    src/devices/DeviceManager.cpp
    src/hotspot/IHotspotController.h
    src/hotspot/WindowsHotspotController.h
    src/hotspot/WindowsHotspotController.cpp
    src/hotspot/MockHotspotController.h
    src/hotspot/MockHotspotController.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/app/AppController.h
    src/app/AppController.cpp
)

if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${APP_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${APP_SOURCES})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
)

if(WIN32 AND MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE windowsapp.lib)
endif()

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
