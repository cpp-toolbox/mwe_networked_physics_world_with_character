#include "graphics.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "shaders/CWL_uniform_binder_camera_pov.hpp"

void render(GLuint shader_program_id, Model *map, Model &character_model,
            std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data, Camera *camera,
            int screen_width, int screen_height, uint64_t *client_id) {

    if (*client_id == -1) {
        return; // we've not yet connected to the server, nothing to render.
    }

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // instanced rendering of all the players at their different locations or something? or just multiple draw calls
    // with different transforms for each character, get the characters position, turn it into a matrix, bind that
    // matrix into the shader program and call draw. so we need to iterate through the received data which is a list of
    // blah blah

    glm::vec3 client_character_position;
    Camera client_camera;
    std::unordered_map<uint64_t, glm::mat4>
        client_id_to_translation_matrix; // excludes our own, we don't need to draw our own model

    for (auto pair : client_id_to_character_data) {
        NetworkedCharacterData player_data = pair.second;
        glm::vec3 glm_character_position(player_data.character_x_position, player_data.character_y_position,
                                         player_data.character_z_position);
        if (player_data.client_id == *client_id) {
            client_character_position = glm_character_position;
            client_camera.set_look_direction(player_data.camera_yaw_angle, player_data.camera_pitch_angle);
        } else {
            glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), glm_character_position);
            client_id_to_translation_matrix[player_data.client_id] = translation_matrix;
        }
    }

    for (auto pair : client_id_to_translation_matrix) {
        glm::mat4 translation_matrix = pair.second;
        bind_CWL_matrix_uniforms_camera_pov(shader_program_id, screen_width, screen_height, client_character_position,
                                            translation_matrix, client_camera, 90, 100);
        character_model.draw();
    }

    // for the map we use the identity transfomraiton, it stays fixed
    bind_CWL_matrix_uniforms_camera_pov(shader_program_id, screen_width, screen_height, client_character_position,
                                        glm::mat4(1.0f), client_camera, 90, 100);

    map->draw();
}

std::function<void()> render_closure(GLuint shader_program_id, Model *map, Model &character_model,
                                     std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data,
                                     Camera *camera, GLFWwindow *window, unsigned int screen_width_px,
                                     unsigned int screen_height_px, uint64_t *client_id) {
    return [shader_program_id, map, window, camera, screen_width_px, screen_height_px, &character_model, client_id,
            &client_id_to_character_data]() {
        render(shader_program_id, map, character_model, client_id_to_character_data, camera, screen_width_px,
               screen_height_px, client_id);
        glfwSwapBuffers(window);
        glfwPollEvents();
    };
}
