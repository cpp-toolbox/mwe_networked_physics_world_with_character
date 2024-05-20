#include "client.hpp"
#include "game_loop/game_loop.hpp"
#include "graphics/textured_model_loading/model_loading.hpp"
#include "graphics/shader_pipeline/shader_pipeline.hpp"
#include "networked_input_snapshot/networked_input_snapshot.hpp"
#include "multiplayer_window/window.hpp"
#include "graphics/graphics.hpp"
#include "expiring_data_container/expiring_data_container.hpp"
#include "interaction/mouse/mouse.hpp"
#include "rate_limited_loop/rate_limited_loop.hpp"
#include "networked_character_data/networked_character_data.hpp"
#include <chrono>
#include <thread>

// void update(double time_since_last_update) {}

std::function<int()> termination_closure(GLFWwindow *window) {
    return [window]() { return glfwWindowShouldClose(window); };
}

std::function<void(double)>
update_closure(ExpiringDataContainer<NetworkedInputSnapshot> &processed_input_snapshot_history,
               std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data,
               NetworkedInputSnapshot &live_input_snapshot, Mouse &mouse, Camera &camera, uint64_t *client_id) {
    return [&live_input_snapshot, &processed_input_snapshot_history, &mouse, &camera, client_id,
            &client_id_to_character_data](double time_since_last_update_ms) {
        if (*client_id == -1) {
            return; // we've not yet connected to the server, no reason to start doing anything yet. we can do better by
                    // waiting to start any thread until this condition is met. which can be check occasionally.
        }

        auto [change_in_yaw_angle, change_in_pitch_angle] =
            mouse.get_yaw_pitch_deltas(live_input_snapshot.mouse_position_x, live_input_snapshot.mouse_position_y);
        // TODO this probably needs to be locked with a mutex so that a network and local update can't occur at the
        // same time for now I don't care.
        camera.update_look_direction(change_in_yaw_angle, change_in_pitch_angle);
        NetworkedInputSnapshot captured_input_snapshot = live_input_snapshot;

        // do client side prediction by doing this we consider the snapshot to be "applied locally"
        client_id_to_character_data[*client_id].camera_yaw_angle = camera.yaw_angle;
        client_id_to_character_data[*client_id].camera_pitch_angle = camera.pitch_angle;

        uint64_t time = std::chrono::steady_clock::now().time_since_epoch().count();

        captured_input_snapshot.client_input_history_insertion_time_epoch_ms = time;

        // also we need to "apply the input snapshot here", for now that's just updating the camera based on mouse
        // deltas

        processed_input_snapshot_history.insert(captured_input_snapshot);
        printf("[]~~ inserting into input snapshot history, it has size %zu\n",
               processed_input_snapshot_history.size());
        // input_snapshot_history_container.print_state();
    };
}

int main() {
    Mouse mouse;
    unsigned int window_width_px = 600, window_height_px = 600;
    bool start_in_fullscreen = false;
    const int network_send_rate_hz = 40; // no reason to have a different send rate either because that would imply
                                         // sending the same state twice, maybe consolidate if possible
    const int update_and_input_snapshot_sample_rate_hz =
        40; // "the is no reason to sample at a different rate to which you run updates\n"

    // "live" means that on mouse and keyboard callbacks it is disjointly written to
    // immediately
    NetworkedInputSnapshot live_input_snapshot;
    Camera camera;
    glm::vec3 character_position(0, 0, 0);

    ExpiringDataContainer<NetworkedInputSnapshot> processed_input_snapshot_history(std::chrono::milliseconds(2000));
    // std::function<bool()> false_termination = []() { return false; };

    // std::function start_update_loop = [&]() {
    //     update_loop.start(update_and_input_snapshot_sample_rate_hz, update, false_termination);
    // };

    // std::thread input_snapshot_sampler_thread(start_update_loop);
    // input_snapshot_sampler_thread.detach();

    GameLoop game_loop;

    std::unordered_map<uint64_t, NetworkedCharacterData> client_id_to_character_data;

    GLFWwindow *window = initialize_glfw_glad_and_return_window(&window_width_px, &window_height_px, "client",
                                                                start_in_fullscreen, &live_input_snapshot);

    ShaderPipeline player_pov_shader_pipeline;
    glEnable(GL_DEPTH_TEST); // configure global opengl state
    player_pov_shader_pipeline.load_in_shaders_from_file(
        "../graphics/shaders/"
        "CWL_v_transformation_with_texture_position_passthrough.vert",
        "../graphics/shaders/textured.frag"); // build and compile shaders

    Model map("../assets/maps/ground_test.obj", player_pov_shader_pipeline.shader_program_id);
    Model character_model("../assets/character/character.obj", player_pov_shader_pipeline.shader_program_id);

    ClientNetwork client_network(&live_input_snapshot);
    client_network.attempt_to_connect_to_server();

    std::function<void()> temp = [&]() {
        client_network.start_network_loop(40, camera, mouse, client_id_to_character_data,
                                          processed_input_snapshot_history);
    };
    std::thread network_thread(temp);
    network_thread.detach();

    // std::thread network_thread(&ClientNetwork::start_network_loop, std::ref(client_network), 60,
    //                            client_id_to_character_data);
    // network_thread.detach();
    //
    RateLimitedLoop update_loop;

    std::function<void(double)> update = update_closure(processed_input_snapshot_history, client_id_to_character_data,
                                                        live_input_snapshot, mouse, camera, &client_network.id);

    std::function<int()> termination = termination_closure(window);

    std::function<void()> render =
        render_closure(player_pov_shader_pipeline.shader_program_id, &map, character_model, client_id_to_character_data,
                       &camera, window, window_width_px, window_height_px, &client_network.id);

    game_loop.start(60, update, render, termination);
}
