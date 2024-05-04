#include <thread>
#include "server.hpp"
#include "input_snapshot/input_snapshot.hpp"
#include "rate_limited_loop/rate_limited_loop.hpp"
#include "interaction/physics/physics.hpp"
#include "interaction/camera/camera.hpp"
#include "model_loading/model_loading.hpp"
#include "math/conversions.hpp"

std::function<void(double)> physics_step_closure(InputSnapshot *input_snapshot, Physics *physics, Camera *camera,
                                                 float movement_acceleration) {
    return [input_snapshot, physics, camera, movement_acceleration](double time_since_last_update) {
        // auto [change_in_yaw_angle, change_in_pitch_angle] =
        //     mouse->get_yaw_pitch_deltas(input_snapshot->mouse_position_x, input_snapshot->mouse_position_y);
        camera->update_look_direction(0, 0);

        printf("delta time: %f\n", time_since_last_update);

        JPH::Vec3 character_position = physics->character->GetPosition();

        printf("character position: %f %f %f\n", character_position.GetX(), character_position.GetY(),
               character_position.GetZ());

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

int main() {
    ServerNetwork server_network;
    InputSnapshot input_snapshot;
    Camera camera;
    const float movement_acceleration = 15.0f;

    const int physics_and_network_send_rate_hz = 60;

    Physics physics;
    Model map("../assets/maps/ground_test.obj");
    physics.load_model_into_physics_world(&map);

    RateLimitedLoop physics_loop;
    std::function<void(double)> physics_step =
        physics_step_closure(&input_snapshot, &physics, &camera, movement_acceleration);
    std::function<bool()> termination_condition = []() { return false; };
    std::function<void()> start_loop = [&]() {
        physics_loop.start(physics_and_network_send_rate_hz, physics_step, termination_condition);
    };
    std::thread physics_thread(start_loop);
    physics_thread.detach();

    RateLimitedLoop game_state_send_loop;
    std::function<void(double)> game_state_send_step = server_network.game_state_send_step_closure(&physics);
    std::function<void()> start_game_state_send_loop = [&]() {
        game_state_send_loop.start(physics_and_network_send_rate_hz, game_state_send_step, termination_condition);
    };
    std::thread game_state_send_loop_thread(start_game_state_send_loop);
    game_state_send_loop_thread.detach();
    // std::function<void(double)> network_out_step =     std::function<void()> start_network_out_loop =

    auto start_server_receive_loop = [&server_network, &input_snapshot]() {
        server_network.start_receive_loop(&input_snapshot);
    };
    std::thread server_receive_loop_thread(start_server_receive_loop);
    server_receive_loop_thread.join();
}
