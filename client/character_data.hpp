#ifndef CHARACTER_DATA_HPP
#define CHARACTER_DATA_HPP

#include <cstdint>

struct PlayerData {
    uint64_t client_id;
    float character_x_position;
    float character_y_position;
    float character_z_position;
    double camera_yaw_angle;
    double camera_pitch_angle;
};

#endif
