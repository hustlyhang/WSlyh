cmake_minimum_required(VERSION 3.10)
project(demo)
aux_source_directory(. DIR_SRC)
add_executable(demo${DIR_SRC})

