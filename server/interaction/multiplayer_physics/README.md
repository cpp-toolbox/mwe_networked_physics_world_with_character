# Info

A minimal wrapper around the jolt physics library ready for a multiplayer setting

# Depdendencies

* [jolt physics](https://github.com/jrouwe/JoltPhysics)
* [math](https://github.com/opengl-toolbox/math) - conversions between glm and jolt

# CMake

```
...

# Jolt Physics: Physics Engine
include_directories(external_libraries/JoltPhysics)
add_subdirectory(external_libraries/JoltPhysics/Build)

... 

target_link_libraries(your_project_name ... Jolt)
```
