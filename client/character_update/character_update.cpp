#include "character_update.hpp"
#include "../math/conversions.hpp"

void update_player_camera_and_velocity(JPH::Ref<JPH::CharacterVirtual> &character, Camera &camera, Mouse &mouse,
                                       NetworkedInputSnapshot &input_snapshot, float movement_acceleration,
                                       double time_since_last_update, JPH::Vec3 gravity) {
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
