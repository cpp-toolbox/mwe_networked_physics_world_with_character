#include "server.hpp"
#include "interaction/physics/physics.hpp"
#include "thread_safe_queue.hpp"
#include <functional>
#include <stdexcept>
#include <stdio.h>
#include <string>

using std::function;

uint64_t UniqueIDGenerator::generate() { return counter.fetch_add(1, std::memory_order_relaxed); };

void set_inputs_to_false(InputSnapshot *input_snapshot) {
    input_snapshot->left_pressed = false;
    input_snapshot->right_pressed = false;
    input_snapshot->forward_pressed = false;
    input_snapshot->backward_pressed = false;
    input_snapshot->jump_pressed = false;
}

void write_binary_input_snapshot_to_input_snapshot(unsigned int binary_input_snapshot, InputSnapshot *input_snapshot) {
    std::string temp[5] = {"l", "r", "f", "b", "j"};

    set_inputs_to_false(input_snapshot);

    for (int i = 0; i < 5; i++) {
        if (binary_input_snapshot & (1 << i)) {
            std::string message = temp[i] + " pressed\n";
            printf("%s", message.c_str());
            switch (i) {
            case 0:
                input_snapshot->left_pressed = true;
                break;
            case 1:
                input_snapshot->right_pressed = true;
                break;
            case 2:
                input_snapshot->forward_pressed = true;
                break;
            case 3:
                input_snapshot->backward_pressed = true;
                break;
            case 4:
                input_snapshot->jump_pressed = true;
                break;
            }
        }
    }
}

ServerNetwork::ServerNetwork() {
    if (enet_initialize() != 0) {
        printf("An error occurred while initializing ENet.\n");
        std::runtime_error("An error occurred while initializing ENet.\n");
        // return 1;
    }

    ENetAddress address = {0};

    address.host = ENET_HOST_ANY; /* Bind the server to the default localhost. */
    address.port = this->port;    /* Bind the server to port 7777. */

    int MAX_CLIENTS = 32;

    /* create a server */
    ENetHost *server = enet_host_create(&address, MAX_CLIENTS, 2, 0, 0);

    if (server == NULL) {
        printf("An error occurred while trying to create an ENet server host.\n");
        std::runtime_error("An error occurred while trying to create an ENet server host.\n");
        // return 1;
    }

    this->server = server;

    printf("Server has been initalized...\n");
}

void initialize_enet() {}

void ServerNetwork::remove_client_data_from_engine(ENetEvent disconnect_event, Physics *physics,
                                                   std::unordered_map<uint64_t, Camera> &client_id_to_camera,
                                                   std::unordered_map<uint64_t, Mouse> &client_id_to_mouse) {
    // the for loop has to happen because we need to figure out what client disconnected.
    for (auto it = connected_clients.begin(); it != connected_clients.end(); ++it) {
        if (it->second.peer == disconnect_event.peer) {
            Client matching_client = it->second;
            uint64_t id_of_disconnected_client = matching_client.uniqueID;
            std::cout << "Client with ID " << id_of_disconnected_client << " disconnected." << std::endl;

            physics->delete_character(id_of_disconnected_client);
            client_id_to_mouse.erase(id_of_disconnected_client);
            client_id_to_camera.erase(id_of_disconnected_client);
            connected_clients.erase(id_of_disconnected_client);

            break;
        }
    }
}

int ServerNetwork::start_receive_loop(InputSnapshot *input_snapshot, Physics *physics,
                                      std::unordered_map<uint64_t, Camera> &client_id_to_camera,
                                      std::unordered_map<uint64_t, Mouse> &client_id_to_mouse,
                                      ThreadSafeQueue<InputSnapshot> &input_snapshot_queue) {
    ENetEvent event;
    int wait_time_milliseconds = 100;

    while (true) {
        /* Wait for an event. (WARNING: blocking, which is ok, since this is in it's own thread) */
        while (enet_host_service(this->server, &event, wait_time_milliseconds) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
                uint64_t new_id = id_generator.generate();
                ENetPacket *packet = enet_packet_create(reinterpret_cast<const void *>(&new_id), sizeof(new_id),
                                                        ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(event.peer, 0, packet);

                connected_clients[new_id] = {event.peer, new_id};
                Camera camera;
                client_id_to_camera[new_id] = camera;
                Mouse mouse;
                client_id_to_mouse[new_id] = mouse;
                physics->create_character(new_id);

                /* Store any relevant client information here. */
                // event.peer->data = (void *)"Client information";
                //
                // printf("peer id: %d\n", enet_peer_get_id(event.peer));
            } break;

            case ENET_EVENT_TYPE_RECEIVE: {
                // printf("A packet of length %lu containing %s was received from %s on "
                //        "channel %u.\n",
                //        event.packet->dataLength, event.packet->data, event.peer->data, event.channelID);
                // not everything is going to be an input snapshot, but it works for now.
                bool packet_is_input_snapshot = true;
                if (packet_is_input_snapshot) {
                    InputSnapshot received_input_snapshot = *reinterpret_cast<InputSnapshot *>(event.packet->data);
                    input_snapshot_queue.push(received_input_snapshot);
                }
                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy(event.packet);
            } break;

            case ENET_EVENT_TYPE_DISCONNECT:
                printf("%s disconnected.\n", event.peer->data);
                /* Reset the peer's client information. */
                remove_client_data_from_engine(event, physics, client_id_to_camera, client_id_to_mouse);
                event.peer->data = NULL;
                break;

            case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                // printf("%s disconnected due to timeout.\n", event.peer->data);
                /* Reset the peer's client information. */
                remove_client_data_from_engine(event, physics, client_id_to_camera, client_id_to_mouse);
                event.peer->data = NULL;
                break;

            case ENET_EVENT_TYPE_NONE:
                break;
            }
        }
    }

    enet_host_destroy(server);
    enet_deinitialize();
    return 0;
}

/**
 * \note that this is run in a thread, but only uses read-only on the physics world
 */
std::function<void(double)>
ServerNetwork::game_state_send_step_closure(Physics *physics,
                                            std::unordered_map<uint64_t, Camera> &client_id_to_camera) {
    return [physics, &client_id_to_camera, this](double time_since_last_update) {
        std::vector<PlayerData> game_update;
        for (const auto &pair : physics->client_id_to_physics_character) {
            uint64_t client_id = pair.first;
            JPH::Ref<JPH::CharacterVirtual> character = pair.second;
            Camera camera = client_id_to_camera[client_id];
            JPH::Vec3 character_position = character->GetPosition();
            PlayerData player_data = {
                client_id,        character_position.GetX(), character_position.GetY(), character_position.GetZ(),
                camera.yaw_angle, camera.pitch_angle};
            game_update.push_back(player_data);
        }

        // Convert the vector to raw data
        size_t game_update_size = game_update.size() * sizeof(PlayerData);
        char *raw_data = new char[game_update_size];
        std::memcpy(raw_data, game_update.data(), game_update_size);
        ENetPacket *packet = enet_packet_create(raw_data, game_update_size, 0);
        enet_host_broadcast(this->server, 0, packet);
        // enet_host_flush(this->server);
        delete[] raw_data;
    };
}
