cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(enviromentalModule)

file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

target_sources(app PRIVATE ${SRC_FILES})
target_include_directories(app PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/includes" ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/proto)

list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)             
include(nanopb)  
#generates sensors.pb.c/sensors.pb.h
zephyr_nanopb_sources(app proto/sensors.proto)
