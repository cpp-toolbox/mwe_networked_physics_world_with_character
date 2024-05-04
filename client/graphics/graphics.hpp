#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "textured_model_loading/model_loading.hpp"
#include "../interaction/camera/camera.hpp"

#include <functional>
std::function<void()> render_closure(GLuint shader_program_id, Model *map, glm::vec3 *character_position,
                                     Camera *camera, GLFWwindow *window, unsigned int screen_width_px,
                                     unsigned int screen_height_px);

#endif
