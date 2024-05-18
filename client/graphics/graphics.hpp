#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "textured_model_loading/model_loading.hpp"
#include "../interaction/camera/camera.hpp"
#include "../networked_character_data/networked_character_data.hpp"

#include <functional>

std::function<void()> render_closure(GLuint shader_program_id, Model *map, Model &character_model,
                                     std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data,
                                     Camera *camera, GLFWwindow *window, unsigned int screen_width_px,
                                     unsigned int screen_height_px, uint64_t *client_id);

#endif
