# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)

project(cutefish-system-monitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CTest)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 DBus Concurrent)
find_package(CutefishFramework REQUIRED COMPONENTS Application)

add_library(monitor-backend STATIC
    src/procreader.cpp
    src/processmodel.cpp
    src/monitor.cpp
)
target_include_directories(monitor-backend PUBLIC src)
target_link_libraries(monitor-backend PUBLIC Qt6::Core Qt6::Concurrent Cutefish::Application)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/historygraph.cpp
    resources.qrc
)
target_compile_definitions(${PROJECT_NAME} PRIVATE SYSTEM_MONITOR_VERSION="${PROJECT_VERSION}")
target_link_libraries(${PROJECT_NAME} PRIVATE
    monitor-backend
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::DBus
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES cutefish-system-monitor.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
