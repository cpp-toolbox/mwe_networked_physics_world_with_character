#include <thread>
#include "server.hpp"
#include "input_snapshot/input_snapshot.hpp"
#include "rate_limited_loop/rate_limited_loop.hpp"
#include "interaction/physics/physics.hpp"
#include "interaction/camera/camera.hpp"
#include "model_loading/model_loading.hpp"
#include "math/conversions.hpp"
#include "interaction/mouse/mouse.hpp"

#include "ftxui/component/component.hpp" // for Checkbox, Renderer, Horizontal, Vertical, Input, Menu, Radiobox, ResizableSplitLeft, Tab
#include "ftxui/component/screen_interactive.hpp" // for Component, ScreenInteractive
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

std::function<void(double)> physics_step_closure(InputSnapshot *input_snapshot, Physics *physics, Camera *camera,
                                                 Mouse *mouse, float movement_acceleration) {
    return [input_snapshot, physics, camera, mouse, movement_acceleration](double time_since_last_update) {
        // auto [change_in_yaw_angle, change_in_pitch_angle] =
        //     mouse->get_yaw_pitch_deltas(input_snapshot->mouse_position_x, input_snapshot->mouse_position_y);
        auto [change_in_yaw_angle, change_in_pitch_angle] =
            mouse->get_yaw_pitch_deltas(input_snapshot->mouse_position_x, input_snapshot->mouse_position_y);
        camera->update_look_direction(change_in_yaw_angle, change_in_pitch_angle);

        // printf("delta time: %f\n", time_since_last_update);

        JPH::Vec3 character_position = physics->character->GetPosition();

        // printf("character position: %f %f %f\n", character_position.GetX(), character_position.GetY(),
        //        character_position.GetZ());

        // in jolt y is z
        glm::vec3 updated_velocity = convert_vec3_from_jolt_to_glm(physics->character->GetLinearVelocity());

        glm::vec3 input_vec =
            camera->input_snapshot_to_input_direction(input_snapshot->forward_pressed, input_snapshot->backward_pressed,
                                                      input_snapshot->right_pressed, input_snapshot->left_pressed);

        updated_velocity += input_vec * movement_acceleration * (float)time_since_last_update;

        glm::vec3 current_xz_velocity = glm::vec3(updated_velocity.x, 0.0f, updated_velocity.z);

        glm::vec3 y_axis = glm::vec3(0, 1, 0);

        float friction = 0.983f;

        updated_velocity *= friction; // friction

        // jump if needed.
        if (physics->character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) {
            updated_velocity.y = 0; // empty out vertical velocity while on ground
            if (input_snapshot->jump_pressed) {
                updated_velocity += (float)1200 * convert_vec3_from_jolt_to_glm(physics->character->GetUp()) *
                                    (float)time_since_last_update;
            }
        }

        glm::vec3 gravity = convert_vec3_from_jolt_to_glm(physics->physics_system.GetGravity());
        // apply gravity
        updated_velocity += gravity * (float)time_since_last_update;

        physics->character->SetLinearVelocity(convert_vec3_from_glm_to_jolt(updated_velocity));
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
    Camera camera;
    Mouse mouse;
    const float movement_acceleration = 15.0f;
    const int physics_and_network_send_rate_hz = 60;

    Physics physics;
    Model map("../assets/maps/ground_test.obj");
    physics.load_model_into_physics_world(&map);

    RateLimitedLoop physics_loop;
    std::function<void(double)> physics_step =
        physics_step_closure(&input_snapshot, &physics, &camera, &mouse, movement_acceleration);
    std::function<bool()> termination_condition = []() { return false; };
    std::function<void()> start_loop = [&]() {
        physics_loop.start(physics_and_network_send_rate_hz, physics_step, termination_condition);
    };
    std::thread physics_thread(start_loop);
    physics_thread.detach();

    RateLimitedLoop game_state_send_loop;
    std::function<void(double)> game_state_send_step = server_network.game_state_send_step_closure(&physics, &camera);
    std::function<void()> start_game_state_send_loop = [&]() {
        game_state_send_loop.start(physics_and_network_send_rate_hz, game_state_send_step, termination_condition);
    };

    std::thread game_state_send_loop_thread(start_game_state_send_loop);
    game_state_send_loop_thread.detach();

    auto start_server_receive_loop = [&server_network, &input_snapshot]() {
        server_network.start_receive_loop(&input_snapshot);
    };

    bool using_tui = false;

    std::thread server_receive_loop_thread(start_server_receive_loop);
    if (using_tui) {
        server_receive_loop_thread.detach();
        return start_terminal_interface(&physics_loop.stopwatch.average_frequency,
                                        &game_state_send_loop.stopwatch.average_frequency);
    } else {
        server_receive_loop_thread.join();
    }
}
