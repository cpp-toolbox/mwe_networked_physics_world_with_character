#include <thread>
#include "server.hpp"
#include "input_snapshot/input_snapshot.hpp"
#include "rate_limited_loop/rate_limited_loop.hpp"
#include "interaction/physics/physics.hpp"
#include "interaction/camera/camera.hpp"
#include "model_loading/model_loading.hpp"
#include "math/conversions.hpp"
#include "interaction/mouse/mouse.hpp"

#include "thread_safe_queue.hpp"

#include "ftxui/component/component.hpp" // for Checkbox, Renderer, Horizontal, Vertical, Input, Menu, Radiobox, ResizableSplitLeft, Tab
#include "ftxui/component/screen_interactive.hpp" // for Component, ScreenInteractive
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

void update_player_velocity(JPH::Ref<JPH::CharacterVirtual> &character, Camera &camera, Mouse &mouse,
                            InputSnapshot &input_snapshot, float movement_acceleration, double time_since_last_update,
                            JPH::Vec3 gravity) {
    auto [change_in_yaw_angle, change_in_pitch_angle] =
        mouse.get_yaw_pitch_deltas(input_snapshot.mouse_position_x, input_snapshot.mouse_position_y);
    camera.update_look_direction(change_in_yaw_angle, change_in_pitch_angle);

    // printf("    updating character velocity with change in yaw: %f pitch: %f\n", change_in_yaw_angle,
    //        change_in_pitch_angle);

    JPH::Vec3 character_position = character->GetPosition();

    // printf("character position: %f %f %f\n", character_position.GetX(), character_position.GetY(),
    //        character_position.GetZ());

    // in jolt y is z
    glm::vec3 updated_velocity = convert_vec3_from_jolt_to_glm(character->GetLinearVelocity());

    glm::vec3 input_vec =
        camera.input_snapshot_to_input_direction(input_snapshot.forward_pressed, input_snapshot.backward_pressed,
                                                 input_snapshot.right_pressed, input_snapshot.left_pressed);

    updated_velocity += input_vec * movement_acceleration * (float)time_since_last_update;
    glm::vec3 current_xz_velocity = glm::vec3(updated_velocity.x, 0.0f, updated_velocity.z);
    glm::vec3 y_axis = glm::vec3(0, 1, 0);
    float friction = 0.983f;
    updated_velocity *= friction; // friction
    if (character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) {
        updated_velocity.y = 0; // empty out vertical velocity while on ground
        if (input_snapshot.jump_pressed) {
            updated_velocity +=
                (float)1200 * convert_vec3_from_jolt_to_glm(character->GetUp()) * (float)time_since_last_update;
        }
    }

    glm::vec3 glm_gravity = convert_vec3_from_jolt_to_glm(gravity);
    // apply gravity
    updated_velocity += glm_gravity * (float)time_since_last_update;

    character->SetLinearVelocity(convert_vec3_from_glm_to_jolt(updated_velocity));
}

std::function<void(double)> physics_step_closure(InputSnapshot *input_snapshot, Physics *physics,
                                                 std::unordered_map<uint64_t, Camera> &client_id_to_camera,
                                                 std::unordered_map<uint64_t, Mouse> &client_id_to_mouse,
                                                 float movement_acceleration) {
    return [input_snapshot, physics, &client_id_to_camera, &client_id_to_mouse,
            movement_acceleration](double time_since_last_update) {
        while (!physics->input_snapshot_queue.empty()) {
            // InputSnapshot popped_input_snapshot = physics->input_snapshot_queue.front();
            InputSnapshot popped_input_snapshot = physics->input_snapshot_queue.pop();
            uint64_t client_id = popped_input_snapshot.client_id;
            // printf("    draining queue and updating player %lu\n", client_id);
            JPH::Ref<JPH::CharacterVirtual> &physics_character = physics->client_id_to_physics_character[client_id];
            Camera &camera = client_id_to_camera[client_id];
            Mouse &mouse = client_id_to_mouse[client_id];
            update_player_velocity(physics_character, camera, mouse, popped_input_snapshot, movement_acceleration,
                                   time_since_last_update, physics->physics_system.GetGravity());
        }
        physics->update(time_since_last_update);
    };
}

int start_terminal_interface(double *physics_rate_hz, double *game_state_send_rate_hz) {
    using namespace ftxui;

    // for each small menu on the stats page there is an option to open it in
    // full? or just have a page for each one? the former is better because we can
    // monitor all data on one screen and then selectively choose , this can also
    // be done by having an "all in one" page and then other pages.
    // kick player
    // list maps
    // select next map
    // change map
    auto screen = ScreenInteractive::Fullscreen();
    std::vector<std::string> entries = {
        "statistics",
        // "connected players (show username and ping, can show more details on "
        // "click or something)",
        "timeline",
        "command line",
    };
    int selected = 0;
    auto tab_selection = Menu(&entries, &selected, MenuOption::HorizontalAnimated());

    auto exit_button = Button(
        "Stop Server", [&] { screen.Exit(); }, ButtonOption::Animated());

    auto main_container = Container::Vertical({Container::Horizontal({tab_selection, exit_button})});

    auto main_renderer = Renderer(main_container, [&] {
        return vbox({
            text("frag-z server") | bold | hcenter,
            hbox({tab_selection->Render() | flex, exit_button->Render()}),
            hbox({
                vbox({
                    text("physics loop thread"),
                    text("rate " + std::to_string(*physics_rate_hz) + "Hz") | flex,
                    separator(),
                    text("network send"),
                    text("game state send rate " + std::to_string(*physics_rate_hz) + "Hz") | flex,
                }),
                separator(),
                vbox({
                    center(text("game events log")) | flex,
                    separator(),
                    center(text("log level selection menu")),
                    text("-> current map"),
                }) | flex,
                separator(),
                vbox({text("connected players") | flex, separator(), text("map rotation") | flex}),
            }) | border |
                flex,
        });
    });

    std::atomic<bool> refresh_ui_continue = true;
    std::thread refresh_ui([&] {
        while (refresh_ui_continue) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(0.05s); // 20Hz, doesn't have to be fast.
            // After updating the state, request a new frame to be drawn. This is done
            // by simulating a new "custom" event to be handled.
            screen.Post(Event::Custom);
        }
    });

    screen.Loop(main_renderer);
    refresh_ui_continue = false;
    refresh_ui.join();

    return EXIT_SUCCESS;
}

int main() {
    ServerNetwork server_network;
    InputSnapshot input_snapshot;
    std::unordered_map<uint64_t, Camera> client_id_to_camera;
    std::unordered_map<uint64_t, Mouse> client_id_to_mouse;
    const float movement_acceleration = 15.0f;
    const int physics_and_network_send_rate_hz = 60;

    Physics physics;
    Model map("../assets/maps/ground_test.obj");
    physics.load_model_into_physics_world(&map);

    RateLimitedLoop physics_loop;
    std::function<void(double)> physics_step =
        physics_step_closure(&input_snapshot, &physics, client_id_to_camera, client_id_to_mouse, movement_acceleration);
    std::function<bool()> termination_condition = []() { return false; };
    std::function<void()> start_loop = [&]() {
        physics_loop.start(physics_and_network_send_rate_hz, physics_step, termination_condition);
    };
    std::thread physics_thread(start_loop);
    physics_thread.detach();

    auto start_network_loop = [&server_network, &input_snapshot, &physics, &client_id_to_camera,
                               &client_id_to_mouse]() {
        server_network.start_network_loop(40, &input_snapshot, &physics, client_id_to_camera, client_id_to_mouse,
                                          physics.input_snapshot_queue);
    };

    bool using_tui = false;
    double temp_network_freq = -1.0;
    std::thread server_receive_loop_thread(start_network_loop);
    if (using_tui) {
        server_receive_loop_thread.detach();
        return start_terminal_interface(&physics_loop.stopwatch.average_frequency, &temp_network_freq);
    } else {
        server_receive_loop_thread.join();
    }
}
