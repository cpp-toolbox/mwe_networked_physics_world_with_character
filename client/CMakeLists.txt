cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(mwe_networked_physics_world_with_character_client)

add_executable(client 
	main.cpp 
	window/window.cpp 
	game_loop/game_loop.cpp 
	stopwatch/stopwatch.cpp
	client.cpp
)

# GLAD: opengl function loader

include_directories(external_libraries/glad_opengl_3.3_core/include)
add_subdirectory(external_libraries/glad_opengl_3.3_core)

# GLFW: opengl framework

# disable unnessary steps in build
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# ENET: reliable UDP

SET(ENET_STATIC ON CACHE BOOL "" FORCE)

add_subdirectory(external_libraries/enet)
include_directories(external_libraries/enet/include)

add_subdirectory(external_libraries/glfw)

target_link_libraries(client glfw glad enet_static)
