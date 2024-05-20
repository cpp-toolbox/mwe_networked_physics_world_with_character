#ifndef MWE_NETWORKING_SERVER_HPP
#define MWE_NETWORKING_SERVER_HPP

#include "enet.h"
#include "networked_input_snapshot/networked_input_snapshot.hpp"
#include "interaction/mouse/mouse.hpp"
#include "interaction/multiplayer_physics/physics.hpp"
#include "interaction/camera/camera.hpp"
#include "thread_safe_queue.hpp"

// A simple structure to represent a client with a unique ID
struct Client {
    ENetPeer *peer;
    uint64_t uniqueID;
};

// A class to generate unique IDs for each connected client
class UniqueIDGenerator {
  public:
    UniqueIDGenerator() : counter(0) {}
    uint64_t generate();

  private:
    std::atomic<uint64_t>
        counter; // atomic so that if two requests come in it's guarenteed that they get different values, probably not
                 // needed atm because this isn't being acces through multiple threads
};

class ServerNetwork {
  public:
    ServerNetwork();
    unsigned int port = 7777;
    ENetHost *server;
    int start_network_loop(
        int send_frequency_hz, NetworkedInputSnapshot *input_snapshot, Physics *physics,
        std::unordered_map<uint64_t, Camera> &client_id_to_camera,
        std::unordered_map<uint64_t, Mouse> &client_id_to_mouse,
        std::unordered_map<uint64_t, uint64_t> &client_id_to_cihtems_of_last_server_processed_input_snapshot,
        ThreadSafeQueue<NetworkedInputSnapshot> &input_snapshot_queue);
    void send_game_state(
        Physics *physics, std::unordered_map<uint64_t, Camera> &client_id_to_camera,
        std::unordered_map<uint64_t, uint64_t> &client_id_to_cihtems_of_last_server_processed_input_snapshot);

    void remove_client_data_from_engine(ENetEvent disconnect_event, Physics *physics,
                                        std::unordered_map<uint64_t, Camera> &client_id_to_camera,
                                        std::unordered_map<uint64_t, Mouse> &client_id_to_mouse);
    std::unordered_map<uint64_t, Client> connected_clients; // Mapping unique IDs to clients
  private:
    UniqueIDGenerator id_generator;
};

#endif // MWE_NETWORKING_SERVER_HPP
