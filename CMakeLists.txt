# EDA P2P – LAN-Chat ohne zentralen Server (C++17, Qt 6, QML)
#
# Aufbau in drei Targets:
#   p2pcore   – statische Bibliothek: Nachrichtenformat + Netzwerk (kein GUI)
#   eda-p2p   – die eigentliche App (QML-Oberfläche + ChatController)
#   tst_*     – Qt-Test-Programme, die nur p2pcore brauchen
# Warum eine Bibliothek? App und Tests nutzen denselben Code, ohne ihn doppelt
# zu kompilieren, und die Tests brauchen keine Grafik.

cmake_minimum_required(VERSION 3.21)
project(EdaP2P VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   # echtes C++17, keine Compiler-Erweiterungen (gnu++17)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Quick)

# Setzt Qt-Standards: AUTOMOC (moc läuft automatisch für Q_OBJECT-Klassen),
# Ausgabeverzeichnisse und Qt-Policies passend zu Qt 6.5+.
qt_standard_project_setup(REQUIRES 6.5)
# Neueres Qt kennt Policies, die 6.5 noch nicht hatte. Nur setzen, wenn bekannt,
# damit Teammitglieder mit älterem Qt 6 trotzdem bauen können.
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

# Einheitliche Warnstufen auf allen Compilern: Fehler früh sehen.
function(eda_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

# ---------------------------------------------------------------- p2pcore
qt_add_library(p2pcore STATIC
    src/core/Message.h          src/core/Message.cpp
    src/core/FrameCodec.h       src/core/FrameCodec.cpp
    src/network/UdpService.h    src/network/UdpService.cpp
    src/network/PeerConnection.h src/network/PeerConnection.cpp
    src/network/TcpService.h    src/network/TcpService.cpp
)
# PUBLIC: Wer p2pcore linkt, bekommt automatisch den Include-Pfad und Qt::Network.
target_include_directories(p2pcore PUBLIC src)
target_link_libraries(p2pcore PUBLIC Qt6::Core Qt6::Network)
eda_enable_warnings(p2pcore)

# ---------------------------------------------------------------- App
qt_add_executable(eda-p2p src/main.cpp)

# QML-Modul "EdaP2P": enthält Main.qml und die C++-Klassen mit QML_ELEMENT.
# Die C++-Klassen müssen hier (nicht in p2pcore) stehen, damit Qt sie beim
# Bauen für QML registriert.
qt_add_qml_module(eda-p2p
    URI EdaP2P
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    SOURCES
        src/app/ChatController.h src/app/ChatController.cpp
        src/app/ChatLogModel.h   src/app/ChatLogModel.cpp
)
target_include_directories(eda-p2p PRIVATE src/app)
target_link_libraries(eda-p2p PRIVATE p2pcore Qt6::Quick)

set_target_properties(eda-p2p PROPERTIES
    # macOS: echtes .app-Bundle mit eigener Info.plist. Ab macOS 15 fragt das
    # System nach "Lokales Netzwerk"; ohne Beschreibungstext wird der Zugriff
    # u. U. still verweigert.
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER de.th-koeln.eda-p2p
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
    # Windows: GUI-Programm ohne zusätzliches Konsolenfenster.
    WIN32_EXECUTABLE TRUE
)
eda_enable_warnings(eda-p2p)

# ---------------------------------------------------------------- Tests
include(CTest)   # stellt die Option BUILD_TESTING bereit (Standard: ON)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    foreach(test_name tst_message tst_framecodec tst_network)
        qt_add_executable(${test_name} tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE p2pcore Qt6::Test)
        eda_enable_warnings(${test_name})
        add_test(NAME ${test_name} COMMAND ${test_name})
        set_tests_properties(${test_name} PROPERTIES TIMEOUT 30)
    endforeach()
endif()
