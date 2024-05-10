#ifndef MWE_NETWORKING_CLIENT_HPP
#define MWE_NETWORKING_CLIENT_HPP

#include "enet.h"
#include "input_snapshot/input_snapshot.hpp"
#include "interaction/camera/camera.hpp"
#include <string>

// BAD DON"T PUT THIS HERE NEEDS TO GO IN SUBMODULE SO
// STAYS SYNCED BETWEEN CLIENT AND SERVER
struct PlayerData {
    uint64_t client_id;
    float character_x_position;
    float character_y_position;
    float character_z_position;
    double camera_yaw_angle;
    double camera_pitch_angle;
};

class ClientNetwork {
  public:
    ClientNetwork(InputSnapshot *input_snapshot);
    ~ClientNetwork();
    uint64_t id = -1;
    InputSnapshot *input_snapshot;
    ENetHost *client;
    ENetPeer *server_connection;
    std::string server_ip_address = "127.0.0.1";
    int server_port = 7777;
    void start_input_sending_loop();
    int start_game_state_receive_loop(glm::vec3 *character_position, Camera *camera);
    void initialize_client_network();
    void attempt_to_connect_to_server();
    void disconnect_from_server();
    void attempt_to_send_test_packet();

  private:
    unsigned int input_snapshot_to_binary();
};

#endif // MWE_NETWORKING_CLIENT_HPP
