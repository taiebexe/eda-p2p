cmake_minimum_required(VERSION 3.21)
project(EdaP2P VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick Qml Network)
qt_standard_project_setup(REQUIRES 6.5 SUPPORTS_UP_TO 6.12)

set(NETWORK_SOURCES
    src/networking/Message.h
    src/networking/UdpDiscovery.h src/networking/UdpDiscovery.cpp
    src/networking/TcpPeer.h src/networking/TcpPeer.cpp
)

qt_add_executable(eda-p2p src/main.cpp ${NETWORK_SOURCES})
qt_add_qml_module(eda-p2p
    URI EdaP2P
    VERSION 1.0
    QML_FILES qml/Main.qml
)
target_link_libraries(eda-p2p PRIVATE Qt6::Quick Qt6::Network)
set_target_properties(eda-p2p PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER de.th-koeln.eda-p2p
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
    WIN32_EXECUTABLE TRUE
)

function(enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()
enable_warnings(eda-p2p)

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(network-smoke tests/network_smoke.cpp ${NETWORK_SOURCES})
    target_include_directories(network-smoke PRIVATE src)
    target_link_libraries(network-smoke PRIVATE Qt6::Core Qt6::Network)
    enable_warnings(network-smoke)
    add_test(NAME network-smoke COMMAND network-smoke)
    set_tests_properties(network-smoke PROPERTIES TIMEOUT 15)
endif()
