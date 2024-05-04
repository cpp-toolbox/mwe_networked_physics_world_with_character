#include "graphics.hpp"
#include "shaders/CWL_uniform_binder_camera_pov.hpp"

void render(GLuint shader_program_id, Model *map, glm::vec3 *character_position, Camera *camera, int screen_width,
            int screen_height) {
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::vec3 glm_character_position = *character_position;

    bind_CWL_matrix_uniforms_camera_pov(shader_program_id, screen_width, screen_height, glm_character_position, *camera,
                                        90, 100);

    map->draw();
}

std::function<void()> render_closure(GLuint shader_program_id, Model *map, glm::vec3 *character_position,
                                     Camera *camera, GLFWwindow *window, unsigned int screen_width_px,
                                     unsigned int screen_height_px) {
    return [shader_program_id, map, window, camera, screen_width_px, screen_height_px, character_position]() {
        render(shader_program_id, map, character_position, camera, screen_width_px, screen_height_px);
        glfwSwapBuffers(window);
        glfwPollEvents();
    };
}
