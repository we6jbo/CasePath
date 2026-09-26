cmake_minimum_required(VERSION 3.21)

project(CasePath VERSION 0.2.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(CasePath
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_link_libraries(CasePath PRIVATE Qt6::Widgets)

install(TARGETS CasePath
    RUNTIME DESTINATION bin
)

# Flathub desktop integration
install(FILES org.oneal.CasePath.desktop
        DESTINATION share/applications)
install(FILES org.oneal.CasePath.metainfo.xml
        DESTINATION share/metainfo)
install(FILES org.oneal.CasePath.svg
        DESTINATION share/icons/hicolor/scalable/apps)
