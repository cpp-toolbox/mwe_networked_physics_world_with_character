#include <chrono>
#include <linux/input.h>
#include <stdexcept>
#include <stdio.h>
#include "client.hpp"
#include "expiring_data_container/expiring_data_container.hpp"
#include "networked_input_snapshot/networked_input_snapshot.hpp"
#include "networked_character_data/networked_character_data.hpp"

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

ClientNetwork::ClientNetwork(NetworkedInputSnapshot *input_snapshot) : input_snapshot(input_snapshot) {
    initialize_client_network();
}
ClientNetwork::~ClientNetwork() { disconnect_from_server(); }

void ClientNetwork::initialize_client_network() {
    if (enet_initialize() != 0) {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        throw std::runtime_error("An error occurred while initializing ENet.\n");
        // return EXIT_FAILURE;
    }

    client = {0};
    client = enet_host_create(NULL /* create a client host */, 1 /* only allow 1 outgoing connection */,
                              2 /* allow up 2 channels to be used, 0 and 1 */,
                              0 /* assume any amount of incoming bandwidth */,
                              0 /* assume any amount of outgoing bandwidth */);
    if (client == NULL) {
        fprintf(stderr, "An error occurred while trying to create an ENet client host.\n");
        throw std::runtime_error("An error occurred while trying to create an ENet client host.\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * \pre initialize_client_network was run successfully
 */
void ClientNetwork::attempt_to_connect_to_server() {

    ENetAddress address = {0};
    ENetEvent event = {static_cast<ENetEventType>(0)};
    server_connection = {0};
    /* Connect to some.server.net:1234. */
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 7777;
    /* Initiate the connection, allocating the two channels 0 and 1. */
    server_connection = enet_host_connect(client, &address, 2, 0);
    if (server_connection == NULL) {
        fprintf(stderr, "No available peers for initiating an ENet connection.\n");
        exit(EXIT_FAILURE);
    }
    /* Wait up to 5 seconds for the connection attempt to succeed. */
    if (enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        puts("Connection to some.server.net:1234 succeeded.");
        printf("peer id: %d\n", enet_peer_get_id(event.peer));
    } else {
        /* Either the 5 seconds are up or a disconnect event was */
        /* received. Reset the peer in the event the 5 seconds   */
        /* had run out without any significant event.            */
        enet_peer_reset(server_connection);
        puts("Connection to some.server.net:1234 failed.");
    }
}

int ClientNetwork::start_network_loop(int send_frequency_hz, Camera &camera, Mouse &mouse,
                                      std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data,
                                      ExpiringDataContainer<NetworkedInputSnapshot> &processed_input_snapshot_history) {
    ENetEvent event;

    bool first_iteration = true;
    std::chrono::steady_clock::time_point time_of_last_input_snapshot_send;
    float send_period_sec = 1.0 / send_frequency_hz;
    int send_period_ms = send_period_sec * 1000;

    while (true) {
        /* Wait for an event. (WARNING: blocking, which is ok, since this is in it's own thread) */
        if (enet_host_service(this->client, &event, send_period_ms) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: // is this even possible on the client?
                printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
                /* Store any relevant client information here. */
                event.peer->data = (void *)"Client information";
                break;

            case ENET_EVENT_TYPE_RECEIVE: {

                if (event.packet->dataLength == sizeof(uint64_t)) { // client id
                    uint64_t receivedID = *reinterpret_cast<const uint64_t *>(event.packet->data);
                    this->id = receivedID;
                    printf("Received unique ID from server: %lu\n", receivedID);
                }

                // printf("A packet of length %lu containing %s was received from %s on "
                //        "channel %u.\n",
                //        event.packet->dataLength, event.packet->data, event.peer->data, event.channelID);

                bool client_id_received_already = id != -1;
                if (client_id_received_already) { // everything after the client id is a game state update

                    NetworkedCharacterData *game_update =
                        reinterpret_cast<NetworkedCharacterData *>(event.packet->data);
                    size_t game_update_length = event.packet->dataLength / sizeof(NetworkedCharacterData);

                    for (size_t i = 0; i < game_update_length; ++i) {
                        NetworkedCharacterData player_data = game_update[i];
                        if (player_data.client_id == this->id) {

                            // server is authorative (reconcile)
                            camera.set_look_direction(
                                client_id_to_character_data[player_data.client_id].camera_yaw_angle,
                                client_id_to_character_data[player_data.client_id].camera_pitch_angle);

                            if (player_data.cihtems_of_last_server_processed_input_snapshot == -1) {
                                printf("got minus one BAD!\n");
                            }

                            auto ciht_of_last_server_processed_input_snapshot = std::chrono::steady_clock::duration(
                                player_data.cihtems_of_last_server_processed_input_snapshot);

                            // note that this has to match what we did in the expiring data container
                            std::chrono::steady_clock::time_point reconstructed_time_point(
                                ciht_of_last_server_processed_input_snapshot);

                            std::vector<NetworkedInputSnapshot> snapshots_to_be_reprocessed =
                                processed_input_snapshot_history.get_data_exceeding(reconstructed_time_point);

                            // re-apply inputs
                            printf("J~~~ about to re apply %zu snapshots out of %zu\n",
                                   snapshots_to_be_reprocessed.size(), processed_input_snapshot_history.size());
                            // processed_input_snapshot_history.print_state();
                            for (auto snapshot_to_be_reprocessed : snapshots_to_be_reprocessed) {
                                auto [change_in_yaw_angle, change_in_pitch_angle] =
                                    mouse.get_yaw_pitch_deltas(snapshot_to_be_reprocessed.mouse_position_x,
                                                               snapshot_to_be_reprocessed.mouse_position_y);
                                // TODO this probably needs to be locked with a mutex so that a network and local update
                                // can't occur at the same time for now I don't care.
                                camera.update_look_direction(change_in_yaw_angle, change_in_pitch_angle);
                            }

                            client_id_to_character_data[player_data.client_id] = player_data;
                            // apply new changes, we have to do this because render uses this structure
                            client_id_to_character_data[player_data.client_id].camera_yaw_angle = camera.yaw_angle;
                            client_id_to_character_data[player_data.client_id].camera_pitch_angle = camera.pitch_angle;

                        } else {
                            client_id_to_character_data[player_data.client_id] = player_data;
                        }

                        print_current_time();
                        printf("<~~~ received id: %lu is client: %b, pos: (%f, %f, %f) look: (%f, %f) \n",
                               player_data.client_id, player_data.client_id == this->id,
                               player_data.character_x_position, player_data.character_y_position,
                               player_data.character_z_position, player_data.camera_yaw_angle,
                               player_data.camera_pitch_angle);
                        // if (player_data.client_id == this->id) {
                        //     printf("got %f %f %f\n", player_data.character_x_position,
                        //     player_data.character_y_position,
                        //            player_data.character_z_position);
                        //     character_position->x = player_data.character_x_position;
                        //     character_position->y = player_data.character_y_position;
                        //     character_position->z = player_data.character_z_position;
                        //     camera->set_look_direction(player_data.camera_yaw_angle, player_data.camera_pitch_angle);
                        // }
                    }
                }
                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy(event.packet);
            } break;

            case ENET_EVENT_TYPE_DISCONNECT:
                printf("%s disconnected.\n", event.peer->data);
                /* Reset the peer's client information. */
                event.peer->data = NULL;
                break;

            case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                printf("%s disconnected due to timeout.\n", event.peer->data);
                /* Reset the peer's client information. */
                event.peer->data = NULL;
                break;

            case ENET_EVENT_TYPE_NONE:
                break;
            }
        }

        if (first_iteration) {
            time_of_last_input_snapshot_send = std::chrono::steady_clock::now();
            first_iteration = false;
        } else if (this->id != -1) {
            // if we 're not in the first iteration and we've received our client id from the server, then we can send
            // out data. otherwise keep waiting
            auto current_time = std::chrono::steady_clock::now();
            std::chrono::duration<double> time_since_last_input_snapshot_send_sec =
                current_time - time_of_last_input_snapshot_send;
            if (time_since_last_input_snapshot_send_sec.count() >= send_period_sec) {
                print_current_time();
                send_input_snapshot(processed_input_snapshot_history);
                time_of_last_input_snapshot_send = current_time;
            }
        }
    }

    enet_host_destroy(this->client);
    enet_deinitialize();
    return 0;
}

/*
 * \pre this->id != -1
 */
void ClientNetwork::send_input_snapshot(
    ExpiringDataContainer<NetworkedInputSnapshot> &processed_input_snapshot_history) {
    assert(this->id != -1);

    if (processed_input_snapshot_history.size() == 0) {
        return; // nothing to do because there is no data to send, this should never occur.
    }

    NetworkedInputSnapshot most_recently_added_processed_snapshot = processed_input_snapshot_history.get_most_recent();
    most_recently_added_processed_snapshot.client_id = this->id; // TODO this should be done somwhere else...
    ENetPacket *packet = enet_packet_create(&most_recently_added_processed_snapshot, sizeof(NetworkedInputSnapshot),
                                            0); // 0 indicates unreliable packet
    //

    printf("~~~> sending input snapshot, it has timestamp %lu\n",
           most_recently_added_processed_snapshot.client_input_history_insertion_time_epoch_ms);
    // printf("msx %f msy %f\n", this->input_snapshot->mouse_position_x,
    // this->input_snapshot->mouse_position_y);
    enet_peer_send(server_connection, 0, packet);
    enet_host_flush(client);
}

void ClientNetwork::disconnect_from_server() {

    // Disconnect
    enet_peer_disconnect(server_connection, 0);

    uint8_t disconnected = false;

    ENetEvent event = {static_cast<ENetEventType>(0)};
    /* Allow up to 3 seconds for the disconnect to succeed
     * and drop any packets received packets.
     */
    while (enet_host_service(client, &event, 3000) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_RECEIVE:
            enet_packet_destroy(event.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            puts("Disconnection succeeded.");
            disconnected = true;
            break;
        }
    }

    // Drop connection, since disconnection didn't successed
    if (!disconnected) {
        enet_peer_reset(server_connection);
    }

    enet_host_destroy(client);
    enet_deinitialize();
}
