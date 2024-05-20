#include "server.hpp"
#include "thread_safe_queue.hpp"
#include "networked_character_data/networked_character_data.hpp"
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <stdio.h>
#include <string>

uint64_t UniqueIDGenerator::generate() { return counter.fetch_add(1, std::memory_order_relaxed); };

void print_current_time() {
    // Get the current time point
    auto currentTime = std::chrono::high_resolution_clock::now();

    // Extract minutes, seconds, and milliseconds
    auto currentTime_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(currentTime);
    auto timeSinceEpoch = currentTime_ms.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceEpoch) % 1000;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timeSinceEpoch) % 60;
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(timeSinceEpoch) % 60;

    // Print the extracted time
    printf("Current time: %lld minutes, %lld seconds, %lld milliseconds\n", minutes.count(), seconds.count(),
           milliseconds.count());
}

void set_inputs_to_false(NetworkedInputSnapshot *input_snapshot) {
    input_snapshot->left_pressed = false;
    input_snapshot->right_pressed = false;
    input_snapshot->forward_pressed = false;
    input_snapshot->backward_pressed = false;
    input_snapshot->jump_pressed = false;
}

void write_binary_input_snapshot_to_input_snapshot(unsigned int binary_input_snapshot,
                                                   NetworkedInputSnapshot *input_snapshot) {
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

int ServerNetwork::start_network_loop(
    int send_frequency_hz, NetworkedInputSnapshot *input_snapshot, Physics *physics,
    std::unordered_map<uint64_t, Camera> &client_id_to_camera, std::unordered_map<uint64_t, Mouse> &client_id_to_mouse,
    std::unordered_map<uint64_t, uint64_t> &client_id_to_cihtems_of_last_server_processed_input_snapshot,
    ThreadSafeQueue<NetworkedInputSnapshot> &input_snapshot_queue) {

    ENetEvent event;

    bool first_iteration = true;
    std::chrono::high_resolution_clock::time_point time_of_last_game_update_send;
    float send_period_sec = 1.0 / send_frequency_hz;
    int send_period_ms = send_period_sec * 1000;

    while (true) {
        /* Wait for an event. (WARNING: blocking, which is ok, since this is in it's own thread) */
        print_current_time();
        printf("starting host_service\n");
        if (enet_host_service(this->server, &event, send_period_ms) > 0) {
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
                bool packet_is_input_snapshot = true;
                if (packet_is_input_snapshot) {
                    NetworkedInputSnapshot received_input_snapshot =
                        *reinterpret_cast<NetworkedInputSnapshot *>(event.packet->data);
                    print_current_time();
                    printf("<~~~ received id: %lu l: %b r: %b f: %b b: %b, j: %b, msx: %f, msy: %f \n",
                           received_input_snapshot.client_id, received_input_snapshot.left_pressed,
                           received_input_snapshot.right_pressed, received_input_snapshot.forward_pressed,
                           received_input_snapshot.backward_pressed, received_input_snapshot.jump_pressed,
                           received_input_snapshot.mouse_position_x, received_input_snapshot.mouse_position_y);
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
        printf("ending host_service, about to send game state\n");

        if (first_iteration) {
            time_of_last_game_update_send = std::chrono::high_resolution_clock::now();
            first_iteration = false;
        } else {
            auto current_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> time_since_last_game_update_send_sec =
                current_time - time_of_last_game_update_send;
            printf("elapsed time %f requires at least %f\n", time_since_last_game_update_send_sec.count(),
                   send_period_sec);
            if (time_since_last_game_update_send_sec.count() >= send_period_sec) {
                print_current_time();
                printf("~~~> sending game state\n");
                send_game_state(physics, client_id_to_camera,
                                client_id_to_cihtems_of_last_server_processed_input_snapshot);
                time_of_last_game_update_send = current_time;
            } else {
                printf("not enough time elapsed yet\n");
            }
        }
    }

    enet_host_destroy(server);
    enet_deinitialize();
    return 0;
}

/**
 * \note that this is run in a thread, but only uses read-only on the physics world
 * \todo this function should not get run until every character id in any mapping has processed
 * at least one input snapshot, no because if it hasn't processed an input snapshot, it may just be as the player is
 * falling in after spawning for the first ttime, on the client if they haven't processed any yet, simply just don't do
 * reconciliation
 */
void ServerNetwork::send_game_state(
    Physics *physics, std::unordered_map<uint64_t, Camera> &client_id_to_camera,
    std::unordered_map<uint64_t, uint64_t> &client_id_to_cihtems_of_last_server_processed_input_snapshot) {

    std::vector<NetworkedCharacterData> game_update;
    for (const auto &pair : physics->client_id_to_physics_character) {
        uint64_t client_id = pair.first;
        JPH::Ref<JPH::CharacterVirtual> character = pair.second;
        Camera camera = client_id_to_camera[client_id];
        uint64_t cihtems_of_last_server_processed_input_snapshot =
            client_id_to_cihtems_of_last_server_processed_input_snapshot[client_id];
        JPH::Vec3 character_position = character->GetPosition();
        NetworkedCharacterData player_data = {client_id,
                                              cihtems_of_last_server_processed_input_snapshot,
                                              character_position.GetX(),
                                              character_position.GetY(),
                                              character_position.GetZ(),
                                              camera.yaw_angle,
                                              camera.pitch_angle};
        game_update.push_back(player_data);
    }

    // Convert the vector to raw data
    size_t game_update_size = game_update.size() * sizeof(NetworkedCharacterData);
    char *raw_data = new char[game_update_size];
    std::memcpy(raw_data, game_update.data(), game_update_size);
    ENetPacket *packet = enet_packet_create(raw_data, game_update_size, 0);
    enet_host_broadcast(this->server, 0, packet);
    // enet_host_flush(this->server);
    delete[] raw_data;
}
