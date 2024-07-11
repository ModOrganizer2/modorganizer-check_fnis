cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(mo2-cmake CONFIG REQUIRED)
find_package(mo2-uibase)

add_library(check_fnis SHARED)
mo2_configure_plugin(check_fnis WARNINGS OFF)
target_link_libraries(check_fnis PRIVATE mo2::uibase)

mo2_install_plugin(check_fnis)
