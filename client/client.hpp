#ifndef MWE_NETWORKING_CLIENT_HPP
#define MWE_NETWORKING_CLIENT_HPP

#include "enet.h"
#include "input_snapshot/input_snapshot.hpp"
#include "interaction/camera/camera.hpp"
#include "thread_safe_queue/thread_safe_queue.hpp"
#include "character_data.hpp"
#include <string>

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
    int start_network_loop(int send_frequency_hz,
                           std::unordered_map<uint64_t, PlayerData> &client_id_to_character_data);
    void send_input_snapshot();
    void initialize_client_network();
    void attempt_to_connect_to_server();
    void disconnect_from_server();

  private:
    unsigned int input_snapshot_to_binary();
};

#endif // MWE_NETWORKING_CLIENT_HPP
