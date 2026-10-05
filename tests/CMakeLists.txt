# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(tst_monitor tst_monitor.cpp)
target_link_libraries(tst_monitor PRIVATE monitor-backend Qt6::Test)
add_test(NAME system_monitor_backend COMMAND tst_monitor)

add_executable(tst_ui
    tst_ui.cpp
    ${PROJECT_SOURCE_DIR}/src/historygraph.cpp
    ${PROJECT_SOURCE_DIR}/resources.qrc
)
target_link_libraries(tst_ui PRIVATE monitor-backend Cutefish::Application
    Qt6::Quick Qt6::QuickControls2 Qt6::Test)
add_test(NAME system_monitor_ui COMMAND tst_ui -platform offscreen)
