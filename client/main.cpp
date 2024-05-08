#include "client.hpp"
#include "game_loop/game_loop.hpp"
#include "graphics/textured_model_loading/model_loading.hpp"
#include "graphics/shader_pipeline/shader_pipeline.hpp"
#include "input_snapshot/input_snapshot.hpp"
#include "window/window.hpp"
#include "graphics/graphics.hpp"
#include <thread>

void update(double time_since_last_update) {}

std::function<int()> termination_closure(GLFWwindow *window) {
    return [window]() { return glfwWindowShouldClose(window); };
}

int main() {
    unsigned int window_width_px = 600, window_height_px = 600;
    bool start_in_fullscreen = false;
    InputSnapshot input_snapshot;
    glm::vec3 character_position(0, 0, 0);
    GameLoop game_loop;
    Camera camera;

    GLFWwindow *window = initialize_glfw_glad_and_return_window(&window_width_px, &window_height_px, "client",
                                                                start_in_fullscreen, &input_snapshot);

    ShaderPipeline player_pov_shader_pipeline;
    glEnable(GL_DEPTH_TEST); // configure global opengl state
    player_pov_shader_pipeline.load_in_shaders_from_file(
        "../graphics/shaders/"
        "CWL_v_transformation_with_texture_position_passthrough.vert",
        "../graphics/shaders/textured.frag"); // build and compile shaders

    Model map("../assets/maps/ground_test.obj", player_pov_shader_pipeline.shader_program_id);

    std::function<int()> termination = termination_closure(window);
    std::function<void()> render =
        render_closure(player_pov_shader_pipeline.shader_program_id, &map, &character_position, &camera, window,
                       window_width_px, window_height_px);

    ClientNetwork client_network(&input_snapshot);
    client_network.attempt_to_connect_to_server();

    std::thread input_sending_thread(&ClientNetwork::start_input_sending_loop, std::ref(client_network));
    input_sending_thread.detach();

    std::thread game_state_receive_thread(&ClientNetwork::start_game_state_receive_loop, std::ref(client_network),
                                          &character_position, &camera);
    game_state_receive_thread.detach();

    game_loop.start(60, update, render, termination);
}
