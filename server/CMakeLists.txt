cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(server)

add_executable(server 
	main.cpp 
	server.cpp
	rate_limited_loop/rate_limited_loop.cpp
	interaction/physics/physics.cpp
	interaction/camera/camera.cpp
	model_loading/model_loading.cpp
	math/conversions.cpp
	stopwatch/stopwatch.cpp
)

# ENet: Networking
SET(ENET_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(external_libraries/enet)
include_directories(external_libraries/enet/include)

# GLM: opengl mathematics
include_directories(external_libraries/glm)
add_subdirectory(external_libraries/glm)

# Jolt Physics: Physics Engine
include_directories(external_libraries/JoltPhysics)
add_subdirectory(external_libraries/JoltPhysics/Build)

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

# FTXUI: build TUIs easily
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
include_directories(external_libraries/FTXUI/include)
add_subdirectory(external_libraries/FTXUI)

target_link_libraries(server 
	PRIVATE ftxui::screen
	PRIVATE ftxui::dom
	PRIVATE ftxui::component # Not needed for this example.
	enet_static 
	Jolt 
	assimp
)
