cmake_minimum_required(VERSION 3.16)

project(pulse VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick Network)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

find_program(
    WAYLAND_SCANNER_EXECUTABLE
    NAMES wayland-scanner
    HINTS /home/linuxbrew/.linuxbrew/bin
    REQUIRED
)

set(PULSE_PROTOCOL_XML ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml)
set(PULSE_PROTOCOL_HEADER ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-client-protocol.h)
set(PULSE_PROTOCOL_CODE ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-protocol.c)

add_custom_command(
    OUTPUT ${PULSE_PROTOCOL_HEADER} ${PULSE_PROTOCOL_CODE}
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${PULSE_PROTOCOL_XML} ${PULSE_PROTOCOL_HEADER}
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code ${PULSE_PROTOCOL_XML} ${PULSE_PROTOCOL_CODE}
    DEPENDS ${PULSE_PROTOCOL_XML}
    VERBATIM
)

qt_add_executable(pulse
    src/main.cpp
    src/WaylandLayerShell.cpp
    src/AgentInfo.h
    src/AgentModel.h      src/AgentModel.cpp
    src/ProcScanner.h     src/ProcScanner.cpp
    src/WindowManager.h       src/WindowManager.cpp
    src/HyprlandWindowManager.h src/HyprlandWindowManager.cpp
    src/NullWindowManager.h
    src/HyprlandClient.h  src/HyprlandClient.cpp
    src/TmuxResolver.h    src/TmuxResolver.cpp
    src/ClaudeMetaReader.h src/ClaudeMetaReader.cpp
    src/CodexMetaReader.h  src/CodexMetaReader.cpp
    src/Settings.h        src/Settings.cpp
    src/MiniMd.h          src/MiniMd.cpp
    src/ResponseWriter.h  src/ResponseWriter.cpp
    src/SubscriptionMonitor.h src/SubscriptionMonitor.cpp
    ${PULSE_PROTOCOL_CODE}
    ${PULSE_PROTOCOL_HEADER}
)

target_include_directories(pulse PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(pulse PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    PkgConfig::WAYLAND_CLIENT
)

# pulse-toggle: sends show/hide/toggle command via Unix socket
qt_add_executable(pulse-toggle pulse-toggle/main.cpp)
target_link_libraries(pulse-toggle PRIVATE Qt6::Core Qt6::Network)

qt_add_resources(pulse pulse_qml
    PREFIX "/"
    FILES
        qml/qmldir
        qml/main.qml
        qml/Theme.qml
        qml/AgentRow.qml
        qml/HeaderBar.qml
        qml/QuestionView.qml
        qml/PlanView.qml
        qml/PermissionView.qml
        qml/ExpandedView.qml
        qml/Keycap.qml
        assets/claude-code.png
        assets/codex.png
)
