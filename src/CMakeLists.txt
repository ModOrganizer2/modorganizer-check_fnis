cmake_minimum_required(VERSION 3.16)

add_library(check_fnis SHARED)
mo2_configure_plugin(check_fnis WARNINGS OFF)
mo2_install_target(check_fnis)
