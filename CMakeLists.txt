cmake_minimum_required(VERSION 3.21)
project(EdaP2P LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Network)
qt_standard_project_setup(REQUIRES 6.5 SUPPORTS_UP_TO 6.12)

qt_add_executable(eda-p2p src/main.cpp)
qt_add_qml_module(eda-p2p
    URI EdaP2P
    QML_FILES qml/Main.qml
)
target_link_libraries(eda-p2p PRIVATE Qt6::Quick Qt6::Network)
set_target_properties(eda-p2p PROPERTIES MACOSX_BUNDLE TRUE WIN32_EXECUTABLE TRUE)
