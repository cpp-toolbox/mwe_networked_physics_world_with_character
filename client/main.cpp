#include "client.hpp"
#include "game_loop/game_loop.hpp"

#include "multiplayer_window/window.hpp"
#include "graphics/textured_model_loading/model_loading.hpp"
#include "graphics/shader_pipeline/shader_pipeline.hpp"
#include "graphics/graphics.hpp"

#include "networked_input_snapshot/networked_input_snapshot.hpp"
#include "expiring_data_container/expiring_data_container.hpp"
#include "networked_character_data/networked_character_data.hpp"

#include "interaction/multiplayer_physics/physics.hpp"
#include "character_update/character_update.hpp"
#include "interaction/mouse/mouse.hpp"

#include "rate_limited_loop/rate_limited_loop.hpp"

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

#include "formatting/formatting.hpp"

#include <chrono>
#include <thread>

// void update(double time_since_last_update) {}

std::function<int()> termination_closure(GLFWwindow *window) {
    return [window]() { return glfwWindowShouldClose(window); };
}

void set_character_render_state(std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data,
                                Physics &physics, Camera &camera, uint64_t *client_id) {
    if (*client_id == -1) {
        return; // we've not yet connected to the server, no reason to start doing anything yet. we can do better by
                // waiting to start any thread until this condition is met. which can be check occasionally.
    }
    JPH::Ref<JPH::CharacterVirtual> client_physics_character = physics.client_id_to_physics_character[*client_id];
    client_id_to_character_data[*client_id].camera_yaw_angle = camera.yaw_angle;
    client_id_to_character_data[*client_id].camera_pitch_angle = camera.pitch_angle;
    client_id_to_character_data[*client_id].character_x_position = client_physics_character->GetPosition().GetX();
    client_id_to_character_data[*client_id].character_y_position = client_physics_character->GetPosition().GetY();
    client_id_to_character_data[*client_id].character_z_position = client_physics_character->GetPosition().GetZ();
    spdlog::info("just set character state");
}

std::function<void(double)> update_closure(
    std::mutex &reconcile_mutex, ExpiringDataContainer<NetworkedInputSnapshot> &processed_input_snapshot_history,
    std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data,
    NetworkedInputSnapshot &live_input_snapshot, Physics &physics, Mouse &mouse, Camera &camera, uint64_t *client_id) {
    return [&live_input_snapshot, &processed_input_snapshot_history, &mouse, &camera, client_id,
            &client_id_to_character_data, &physics, &reconcile_mutex](double time_since_last_update_ms) {
        if (*client_id == -1) {
            return; // we've not yet connected to the server, no reason to start doing anything yet. we can do better by
                    // waiting to start any thread until this condition is met. which can be check occasionally.
        }

        NetworkedInputSnapshot frozen_input_snapshot = live_input_snapshot;
        // TODO make sure that the player exists first? mutex it?
        const float movement_acceleration = 15.0f;

        JPH::Ref<JPH::CharacterVirtual> client_physics_character = physics.client_id_to_physics_character[*client_id];
        update_player_camera_and_velocity(client_physics_character, camera, mouse, frozen_input_snapshot,
                                          movement_acceleration, time_since_last_update_ms,
                                          physics.physics_system.GetGravity());

        physics.update(time_since_last_update_ms);

        uint64_t time = std::chrono::steady_clock::now().time_since_epoch().count();
        frozen_input_snapshot.client_input_history_insertion_time_epoch_ms = time;
        frozen_input_snapshot.time_delta_used_for_client_side_processing_ms = time_since_last_update_ms;

        spdlog::info("physics tick with delta: {}\nusing input snapshot: {}, physics world: {}",
                     time_since_last_update_ms, frozen_input_snapshot, physics);

        // reconcile_mutex.unlock();

        // JPH::Vec3 velocity_after_reconciliation = client_physics_character->GetLinearVelocity();
        JPH::Vec3 position_after_update = client_physics_character->GetPosition();
        JPH::Vec3 velocity_after_update = client_physics_character->GetPosition();
        // std::ostringstream pos_stream;
        // pos_stream << position_after_update;
        // std::string pos_str = pos_stream.str();

        processed_input_snapshot_history.insert(frozen_input_snapshot);
        spdlog::info("[]~~ inserting into input snapshot history, it has size {}",
                     processed_input_snapshot_history.size());
        // printf("[]~~ inserting into input snapshot history, it has size %zu\n",
        // processed_input_snapshot_history.size());
        // input_snapshot_history_container.print_state();
    };
}

/**
 * Using std::make_shared and dynamic memory allocation provides several benefits:
 *
 * 1. Efficiency: std::make_shared performs a single memory allocation for both the object
 *    and the control block, which is more efficient than separate allocations.
 *
 * 2. Exception Safety: std::make_shared ensures that no memory leaks occur if an exception
 *    is thrown during the construction of the object.
 *
 * 3. Readability and Simplicity: std::make_shared simplifies the syntax and makes the code
 *    more readable and easier to understand and maintain.
 *
 * 4. Polymorphism and Lifetime Management: Shared pointers allow polymorphic types to be managed
 *    easily and ensure that the objects are properly cleaned up when no longer needed.
 *
 * 5. Shared Ownership: Shared pointers ensure shared ownership of sinks between multiple loggers.
 *    The sinks will not be destroyed until all loggers using them are destroyed.
 *
 * 6. Flexible Logger Configuration: Dynamic memory allocation allows creating, modifying,
 *    and destroying loggers and sinks at runtime without being constrained by their lifetimes
 *    being tied to a specific scope.
 */
void create_logger_system() {

    // Create console sink with color
    //
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs.txt", true);
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    // console_sink->set_level(spdlog::level::info);
    file_sink->set_level(spdlog::level::info);

    // Create logger for main messages with combined sinks
    // spdlog::sinks_init_list main_sink_list = {console_sink, file_sink};
    spdlog::sinks_init_list main_sink_list = {file_sink};
    auto main_logger = std::make_shared<spdlog::logger>("main", main_sink_list.begin(), main_sink_list.end());
    main_logger->set_level(spdlog::level::debug);

    // spdlog::sinks_init_list network_sink_list = {console_sink, file_sink};
    spdlog::sinks_init_list network_sink_list = {file_sink};
    auto network_logger =
        std::make_shared<spdlog::logger>("network", network_sink_list.begin(), network_sink_list.end());
    network_logger->set_level(spdlog::level::debug);

    spdlog::sinks_init_list update_sink_list = {console_sink};
    auto update_logger = std::make_shared<spdlog::logger>("update", update_sink_list.begin(), update_sink_list.end());
    update_logger->set_level(spdlog::level::debug);

    // Register loggers with spdlog
    spdlog::set_default_logger(main_logger);
    spdlog::register_logger(network_logger);
    spdlog::register_logger(update_logger);

    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%F] [%l] %v");
}

void parse_command_line_arguments(int argc, char *argv[], std::string &ip_address, int &port) {
    bool ip_specified = false;

    // Parsing command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-ip" && i + 1 < argc) {
            ip_address = argv[++i];
            ip_specified = true;
        } else if (arg == "-port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else {
            std::cerr << "Usage: " << argv[0] << " -ip <IP address> [-port <port>]" << std::endl;
            exit(1);
        }
    }

    // Check if IP address was specified
    if (!ip_specified) {
        std::cerr << "Error: IP address is required." << std::endl;
        std::cerr << "Usage: " << argv[0] << " -ip <IP address> [-port <port>]" << std::endl;
        exit(1);
    }
}

void start_linear_order_setup(int argc, char *argv[]) {

    // Default port value
    int port = 7777; // Default port

    // Variable to store the IP address
    std::string ip_address;

    // Parse command line arguments
    parse_command_line_arguments(argc, argv, ip_address, port);

    create_logger_system();

    Mouse mouse;
    unsigned int window_width_px = 600, window_height_px = 600;
    bool start_in_fullscreen = false;
    bool vsync = false;
    const int render_max_rate_hz = 60;
    const int update_rate_hz = 60;
    const int network_send_rate_hz = 60;

    // "live" means that on mouse and keyboard callbacks it is disjointly written to
    // immediately
    NetworkedInputSnapshot live_input_snapshot;
    Camera camera;
    glm::vec3 character_position(0, 0, 0);

    ExpiringDataContainer<NetworkedInputSnapshot> processed_input_snapshot_history(std::chrono::milliseconds(2000));

    GameLoop game_loop;

    std::unordered_map<uint64_t, NetworkedCharacterData> client_id_to_character_data;

    GLFWwindow *window = initialize_glfw_glad_and_return_window(&window_width_px, &window_height_px, "client",
                                                                start_in_fullscreen, vsync, &live_input_snapshot);

    ShaderPipeline player_pov_shader_pipeline;
    glEnable(GL_DEPTH_TEST); // configure global opengl state
    player_pov_shader_pipeline.load_in_shaders_from_file(
        "../graphics/shaders/"
        "CWL_v_transformation_with_texture_position_passthrough.vert",
        "../graphics/shaders/textured.frag"); // build and compile shaders

    Model map("../assets/maps/ground_test.obj", player_pov_shader_pipeline.shader_program_id);
    Model character_model("../assets/character/character.obj", player_pov_shader_pipeline.shader_program_id);

    Physics physics;
    physics.load_model_into_physics_world(&map);

    ClientNetwork client_network(&live_input_snapshot, ip_address, port);
    client_network.attempt_to_connect_to_server();

    std::function<void(double)> process_received_game_states_received_since_end_of_last_tick_and_reconcile =
        client_network.network_step_closure(network_send_rate_hz, physics, camera, mouse, client_id_to_character_data,
                                            processed_input_snapshot_history);

    std::function<void(double)> update =
        update_closure(client_network.reconcile_mutex, processed_input_snapshot_history, client_id_to_character_data,
                       live_input_snapshot, physics, mouse, camera, &client_network.id);

    std::function<void(double)> render =
        render_closure(player_pov_shader_pipeline.shader_program_id, &map, character_model, client_id_to_character_data,
                       &camera, window, window_width_px, window_height_px, &client_network.id);

    std::function<int()> termination = termination_closure(window);

    // RateLimitedLoop render_loop;
    // std::function start_render_loop = [&]() { render_loop.start(render_max_rate_hz, render, termination); };
    // std::thread render_thread(start_render_loop);
    // render_thread.detach();

    const uint32_t target_frame_duration_ms = 1000 / 60; // Target frame duration in milliseconds (16.67 ms)
    auto previous_frame_time = std::chrono::high_resolution_clock::now();
    while (!termination()) {

        auto current_frame_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> delta_time = current_frame_time - previous_frame_time;
        double delta_time_seconds = delta_time.count(); // Delta time in seconds
        previous_frame_time = current_frame_time;

        // Update physics with delta time in seconds
        //
        spdlog::info("starting update");
        update(delta_time_seconds);
        spdlog::info("update complete");

        // by sending first, we guarentee a common frequency of sending and it won't the frequency will not
        // pick up random time variance by sending after render.
        bool established_connection = client_network.id != -1;
        if (established_connection) {
            client_network.send_input_snapshot(processed_input_snapshot_history);
        }

        // this can't be in the established connection block for some reason, look into that if you ever need
        // to put it in the block
        process_received_game_states_received_since_end_of_last_tick_and_reconcile(0);

        // Render with delta time in seconds, always render after, because it it was done first it
        // would add random offsets to the send time.

        set_character_render_state(client_id_to_character_data, physics, camera, &client_network.id);

        spdlog::info("starting render");
        render(delta_time_seconds);
        spdlog::info("render complete");

        // Calculate elapsed time after physics and rendering
        auto after_update_and_render_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed_update_and_render_time =
            after_update_and_render_time - current_frame_time;

        spdlog::info("update and render took {} milliseconds", elapsed_update_and_render_time.count());

        // Calculate total elapsed time for the frame
        auto frame_end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed_frame_time = frame_end_time - current_frame_time;

        // Calculate sleep time to maintain 60 Hz frequency
        auto sleep_duration = std::chrono::milliseconds(target_frame_duration_ms) - elapsed_frame_time;
        if (sleep_duration > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(sleep_duration);
        }
    }
}

int main(int argc, char *argv[]) { start_linear_order_setup(argc, argv); }
