cmake_minimum_required(VERSION 3.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # allows for language servers to understand the codebase
set(CMAKE_LINKER_TYPE MOLD) # use a modern linker

project(mwe_networked_physics_world_with_character_client)

add_executable(client 
	main.cpp 

	client.cpp
	multiplayer_window/window.cpp 

	graphics/graphics.cpp
	graphics/shader_pipeline/shader_pipeline.cpp
	graphics/textured_model_loading/model_loading.cpp
	graphics/shaders/CWL_uniform_binder_camera_pov.cpp

	networked_input_snapshot/networked_input_snapshot.cpp

	interaction/multiplayer_physics/physics.cpp
	interaction/camera/camera.cpp
	interaction/mouse/mouse.cpp
	character_update/character_update.cpp
	
	math/conversions.cpp


	rate_limited_loop/rate_limited_loop.cpp
	game_loop/game_loop.cpp 
	stopwatch/stopwatch.cpp
)

# GLAD: opengl function loader
include_directories(external_libraries/glad_opengl_3.3_core/include)
add_subdirectory(external_libraries/glad_opengl_3.3_core)

# GLFW: opengl framework
# disable unnessary steps in build
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external_libraries/glfw)

# ENET: reliable UDP
SET(ENET_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(external_libraries/enet)
include_directories(external_libraries/enet/include)


# GLM: opengl mathematics
include_directories(external_libraries/glm)
add_subdirectory(external_libraries/glm)

# ASSIMP: Asset Importer
# only use obj to speed up compilation
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
include_directories(external_libraries/assimp/include)
add_subdirectory(external_libraries/assimp)

# STB_H: single file libraries
include_directories(external_libraries/stb)

# Jolt Physics: Physics Engine
include_directories(external_libraries/JoltPhysics)
add_subdirectory(external_libraries/JoltPhysics/Build)

target_link_libraries(client glfw glad enet_static assimp Jolt)
