#ifndef MWE_NETWORKING_CLIENT_HPP
#define MWE_NETWORKING_CLIENT_HPP

#include "enet.h"
#include "interaction/mouse/mouse.hpp"
#include "interaction/multiplayer_physics/physics.hpp"
#include "networked_input_snapshot/networked_input_snapshot.hpp"
#include "interaction/camera/camera.hpp"
#include "expiring_data_container/expiring_data_container.hpp"
#include "networked_character_data/networked_character_data.hpp"
#include <string>

class ClientNetwork {
  public:
    ClientNetwork(NetworkedInputSnapshot *input_snapshot);
    ~ClientNetwork();

    uint64_t id = -1;
    NetworkedInputSnapshot *input_snapshot;
    ENetHost *client;
    ENetPeer *server_connection;
    std::string server_ip_address = "127.0.0.1";
    int server_port = 7777;

    int start_network_loop(int send_frequency_hz, Physics &physics, Camera &camera, Mouse &mouse,
                           std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data,
                           ExpiringDataContainer<NetworkedInputSnapshot> &processed_input_snapshot_history);

    void send_input_snapshot(ExpiringDataContainer<NetworkedInputSnapshot> &processed_input_snapshot_history);

    std::mutex reconcile_mutex;
    void reconcile_local_game_state_with_server_update(
        NetworkedCharacterData &networked_character_data, Physics &physics, Camera &camera, Mouse &mouse,
        std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data,
        ExpiringDataContainer<NetworkedInputSnapshot> &processed_input_snapshot_history);

    void initialize_client_network();
    void attempt_to_connect_to_server();
    void disconnect_from_server();

  private:
    unsigned int input_snapshot_to_binary();
};

#endif // MWE_NETWORKING_CLIENT_HPP
