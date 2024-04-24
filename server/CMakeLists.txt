cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(server)

add_executable(server main.cpp server.cpp)

SET(ENET_STATIC ON CACHE BOOL "" FORCE)

add_subdirectory(external_libraries/enet)
include_directories(external_libraries/enet/include)

target_link_libraries(server enet_static)
