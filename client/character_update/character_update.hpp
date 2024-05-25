#ifndef CHARACTER_UPDATE_HPP
#define CHARACTER_UPDATE_HPP

#include "../interaction/multiplayer_physics/physics.hpp"
#include "../interaction/mouse/mouse.hpp"

void update_player_camera_and_velocity(JPH::Ref<JPH::CharacterVirtual> &character, Camera &camera, Mouse &mouse,
                                       NetworkedInputSnapshot &input_snapshot, float movement_acceleration,
                                       double time_since_last_update, JPH::Vec3 gravity);

#endif
