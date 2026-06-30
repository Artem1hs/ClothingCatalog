cmake_minimum_required(VERSION 3.20)

project(ClothingCatalogQmlDemo VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(ClothingCatalogQmlDemo
  main.cpp
)

qt_add_qml_module(ClothingCatalogQmlDemo
  URI ClothingCatalogQmlDemo
  VERSION 1.0
  QML_FILES
    Main.qml
)

target_link_libraries(ClothingCatalogQmlDemo
  PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
)

set_target_properties(ClothingCatalogQmlDemo PROPERTIES
  WIN32_EXECUTABLE TRUE
)
