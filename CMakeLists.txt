cmake_minimum_required(VERSION 3.21)
project(omaview LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# GuiPrivate is only for the header that hands out a window's wl_surface (see
# src/inhibit.cpp); nothing links against Qt's private symbols.
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 REQUIRED COMPONENTS
  Core Gui GuiPrivate Network Qml Quick QuickControls2 Multimedia DBus Concurrent Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

# The idle-inhibit protocol's client code, generated at build time.
set(idle_xml ${WAYLAND_PROTOCOLS_DIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml)
set(idle_header ${CMAKE_BINARY_DIR}/idle-inhibit-unstable-v1-client-protocol.h)
set(idle_code ${CMAKE_BINARY_DIR}/idle-inhibit-unstable-v1-protocol.c)
add_custom_command(OUTPUT ${idle_header}
  COMMAND ${WAYLAND_SCANNER} client-header ${idle_xml} ${idle_header} DEPENDS ${idle_xml})
add_custom_command(OUTPUT ${idle_code}
  COMMAND ${WAYLAND_SCANNER} private-code ${idle_xml} ${idle_code} DEPENDS ${idle_xml})

# Everything but main.cpp, shared by the app and the tests, so each source
# compiles once and a new file in src/ needs no line here.
file(GLOB omaview_sources CONFIGURE_DEPENDS src/*.cpp src/*.h)
list(REMOVE_ITEM omaview_sources ${CMAKE_SOURCE_DIR}/src/main.cpp)
add_library(omaview_core STATIC ${omaview_sources} ${idle_header} ${idle_code})
target_include_directories(omaview_core PUBLIC src ${CMAKE_BINARY_DIR})
target_link_libraries(omaview_core PUBLIC
  Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2
  Qt6::Multimedia Qt6::DBus Qt6::Concurrent PkgConfig::WAYLAND)
target_compile_options(omaview_core PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)

add_executable(omaview src/main.cpp src/resources.qrc)
target_link_libraries(omaview PRIVATE omaview_core)

# The tests load the real Main.qml, so they carry the resources too.
enable_testing()
add_executable(omaview_tests tests/omaview_tests.cpp src/resources.qrc)
target_link_libraries(omaview_tests PRIVATE omaview_core Qt6::Test)
target_compile_definitions(omaview_tests PRIVATE OMAVIEW_DESKTOP_FILE="${CMAKE_SOURCE_DIR}/share/omaview.desktop")
add_test(NAME omaview_tests COMMAND omaview_tests)
set_tests_properties(omaview_tests PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

add_executable(ui_tests tests/ui_tests.cpp src/resources.qrc)
target_link_libraries(ui_tests PRIVATE omaview_core Qt6::Test)
add_test(NAME ui_tests COMMAND ui_tests)
set_tests_properties(ui_tests PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# The command line is tested as an agent uses it: the built binary, no display.
add_test(NAME cli_tests
  COMMAND python3 -m unittest discover -s ${CMAKE_SOURCE_DIR}/tests -p "test_*.py")
set_tests_properties(cli_tests PROPERTIES ENVIRONMENT OMAVIEW=$<TARGET_FILE:omaview>)

# `cmake --install build --prefix ~/.local` puts the app in the launcher
# before it is packaged; a package installs the same files under /usr.
include(GNUInstallDirs)
install(TARGETS omaview RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES share/omaview.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES share/omaview.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/omaview)
