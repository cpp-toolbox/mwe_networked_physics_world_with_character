#include <chrono>
#include <linux/input.h>
#include <stdexcept>
#include <stdio.h>
#include "client.hpp"
#include "expiring_data_container/expiring_data_container.hpp"
#include "networked_input_snapshot/networked_input_snapshot.hpp"
#include "networked_character_data/networked_character_data.hpp"
#include "character_update/character_update.hpp"
#include "spdlog/spdlog.h"
#include "formatting/formatting.hpp"

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
    // enet_address_set_host(&address, "142.67.250.4");
    // address.port = 9999;
    if (this->online_connection) {
        enet_address_set_host(&address, "104.131.10.102");
    } else {
        enet_address_set_host(&address, "127.0.0.1");
    }
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

/**
 * updates:            1    2    [3]    4     5
 * receive game state:   o     o     o     o    o
 * and reconcile
 *
 * when the newest game state is received, we see that it's accounted for input snapshot captured at 3
 * therefore we we will re-apply 4 and 5 to the authorative game state, we leave the regular timeline of the
 * client to do this, and we consider ourself back at 5 as if no time has passed.
 *
 *
 */
void ClientNetwork::reconcile_local_game_state_with_server_update(
    NetworkedCharacterData &networked_character_data, Physics &physics, Camera &camera, Mouse &mouse,
    std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data,
    ExpiringDataContainer<NetworkedInputSnapshot> &processed_input_snapshot_history, JPH::Vec3 &authorative_position,
    JPH::Vec3 &authorative_velocity

) {
    auto ciht_of_last_server_processed_input_snapshot =
        std::chrono::steady_clock::duration(networked_character_data.cihtems_of_last_server_processed_input_snapshot);

    // note that this has to match what we did in the expiring data container
    std::chrono::steady_clock::time_point reconstructed_time_point(ciht_of_last_server_processed_input_snapshot);

    std::vector<NetworkedInputSnapshot> snapshots_to_be_reprocessed =
        processed_input_snapshot_history.get_data_exceeding(reconstructed_time_point);

    JPH::Ref<JPH::CharacterVirtual> client_physics_character =
        physics.client_id_to_physics_character[networked_character_data.client_id];
    const float movement_acceleration = 15.0f;

    reconcile_mutex.lock();

    std::string reconciliation_history;
    reconciliation_history +=
        fmt::format("starting reconciliation, game state before reconciliation is: \n{}", physics);

    // bool first_time_rollback = true;
    // for (int i = 0; i < snapshots_to_be_reprocessed.size(); i++) {
    //     if (first_time_rollback) {
    //         first_time_rollback = false;
    //         continue;
    //     }
    //     reconciliation_history +=
    //         fmt::format("rolled back to state just after applying snapshot with cihtems: {} \n",
    //                     snapshots_to_be_reprocessed[i].client_input_history_insertion_time_epoch_ms);
    // }
    //
    // int num_physics_frames_to_step_back = snapshots_to_be_reprocessed.size() - 1;

    // PhysicsFrame physics_frame_before_predicted_inputs_applied =
    //     physics.physics_frames.get_nth_from_recent(num_physics_frames_to_step_back);

    // physics.physics_system.RestoreState(physics_frame_before_predicted_inputs_applied.physics_state);

    client_physics_character->SetPosition(authorative_position);
    client_physics_character->SetLinearVelocity(authorative_velocity);
    physics.refresh_contacts(client_physics_character);

    // reconciliation_history += fmt::format("after rolling back physics world state is: {}", physics);

    reconciliation_history += fmt::format("Authorative game state set current state: {}", physics);

    reconciliation_history +=
        fmt::format("starting reconciliation about to re apply {} snapshots out of {}\n",
                    snapshots_to_be_reprocessed.size() - 1, processed_input_snapshot_history.size());

    // spdlog::get("network")->info("starting reconciliation about to re apply {} snapshots out of {}",
    //                              snapshots_to_be_reprocessed.size(), processed_input_snapshot_history.size());
    bool first_time = true; // temp fix for some reason the reprocessed snapshots is getting the matchign tiem one but
                            // should be strict
    for (NetworkedInputSnapshot snapshot_to_be_reprocessed : snapshots_to_be_reprocessed) {
        if (first_time) {
            first_time = false;
            continue;
        }
        reconciliation_history += fmt::format("re-applying the following snapshot: \n{}", snapshot_to_be_reprocessed);
        // TODO this probably needs to be locked with a mutex so that a network and local update
        // can't occur at the same time for now I don't care.
        update_player_camera_and_velocity(client_physics_character, camera, mouse, snapshot_to_be_reprocessed,
                                          movement_acceleration,
                                          snapshot_to_be_reprocessed.time_delta_used_for_client_side_processing_ms,
                                          physics.physics_system.GetGravity());

        physics.update_characters_only(snapshot_to_be_reprocessed.time_delta_used_for_client_side_processing_ms);
        // this being commented out makes it smoother because physics call rate doesn't increase, without it it would
        // physics.update(
        //     snapshot_to_be_reprocessed
        //         .time_delta_used_for_client_side_processing_ms); // update in the for loop because the other
        //         characters
        //                                                          // that have velocity will be simulated as well
        JPH::Vec3 reconciliation_position = client_physics_character->GetPosition();
        JPH::Vec3 reconciliation_velocity = client_physics_character->GetLinearVelocity();
        reconciliation_history +=
            fmt::format("after applying that update that players new state is: \n position: {} velocity: {}\n",
                        reconciliation_position, reconciliation_velocity);

        // reconciliation_stream << "just processed cihtems: "
        //                       << snapshot_to_be_reprocessed.client_input_history_insertion_time_epoch_ms << "|||,"
        //                       << reconciliation_position << "|||,";
    }

    // client_id_to_character_data[networked_character_data.client_id] = networked_character_data;
    // apply new changes, we have to do this because render uses this structure
    // uint64_t client_id = networked_character_data.client_id;
    // client_id_to_character_data[client_id].camera_yaw_angle = camera.yaw_angle;
    // client_id_to_character_data[client_id].camera_pitch_angle = camera.pitch_angle;
    // client_id_to_character_data[client_id].character_x_position = client_physics_character->GetPosition().GetX();
    // client_id_to_character_data[client_id].character_y_position = client_physics_character->GetPosition().GetY();
    // client_id_to_character_data[client_id].character_z_position = client_physics_character->GetPosition().GetZ();
    //

    // JPH::Vec3 position_after_reconciliation = client_physics_character->GetPosition();
    // JPH::Vec3 velocity_after_reconciliation = client_physics_character->GetLinearVelocity();
    reconciliation_history += fmt::format("after reconciliation the game state is: \n{}", physics);

    reconcile_mutex.unlock();

    // std::ostringstream x;
    // x << position_after_reconciliation;
    // std::string pos_str = x.str();
    //
    // std::ostringstream y;
    // y << velocity_after_reconciliation;
    // std::string vel_str = y.str();

    // std::string reconciliation_positions = reconciliation_stream.str();
    //

    spdlog::get("network")->info(reconciliation_history);
}

std::function<void(double)>
ClientNetwork::network_step_closure(int service_period_ms, Physics &physics, Camera &camera, Mouse &mouse,
                                    std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data,
                                    ExpiringDataContainer<NetworkedInputSnapshot> &processed_input_snapshot_history) {

    return
        [this, &service_period_ms, &physics, &client_id_to_character_data, &camera, &mouse,
         &processed_input_snapshot_history](double service_period_ms_temp) { // temp because usually this is delta time
            ENetEvent event;

            while (enet_host_service(this->client, &event, 0) > 0) {
                handle_network_event(event, physics, camera, mouse, client_id_to_character_data,
                                     processed_input_snapshot_history);
            }

            if (enet_host_service(this->client, &event, service_period_ms_temp) > //
                0) { // note this sleeps the thread for the period specified
                handle_network_event(event, physics, camera, mouse, client_id_to_character_data,
                                     processed_input_snapshot_history);
            }

        };
}

void ClientNetwork::handle_network_event(
    ENetEvent event, Physics &physics, Camera &camera, Mouse &mouse,
    std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data,
    ExpiringDataContainer<NetworkedInputSnapshot> &processed_input_snapshot_history) {

    switch (event.type) {
    case ENET_EVENT_TYPE_CONNECT: // is this even possible on the client?
        // spdlog::get("network")
        //     ->info("A new client connected from {}:{}", event.peer->address.host,
        //     event.peer->address.port);

        printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
        /* Store any relevant client information here. */
        event.peer->data = (void *)"Client information";
        break;

    case ENET_EVENT_TYPE_RECEIVE: {

        if (event.packet->dataLength == sizeof(uint64_t)) { // client id
            uint64_t receivedID = *reinterpret_cast<const uint64_t *>(event.packet->data);
            this->id = receivedID;
            physics.create_character(receivedID);

            spdlog::get("network")->info("Received unique ID from server: {}", receivedID);
            // printf("Received unique ID from server: %lu\n", receivedID);
        } else {

            bool client_id_received_already = id != -1;
            if (client_id_received_already) { // everything after the client id is a game state update
                NetworkedCharacterData *game_update = reinterpret_cast<NetworkedCharacterData *>(event.packet->data);
                size_t game_update_length = event.packet->dataLength / sizeof(NetworkedCharacterData);
                process_game_state_update(game_update, game_update_length, physics, camera, mouse,
                                          client_id_to_character_data, processed_input_snapshot_history);
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

void ClientNetwork::process_game_state_update(
    NetworkedCharacterData *game_update, int game_update_length, Physics &physics, Camera &camera, Mouse &mouse,
    std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data,
    ExpiringDataContainer<NetworkedInputSnapshot> &processed_input_snapshot_history) {

    std::ostringstream first_look;
    for (size_t i = 0; i < game_update_length; ++i) {
        NetworkedCharacterData networked_character_data = game_update[i];
        first_look << networked_character_data;
    }

    spdlog::get("network")->info(
        "Just received a game update containing: \n {} individual character updates with data: \n {}",
        game_update_length, first_look.str());

    for (size_t i = 0; i < game_update_length; ++i) {
        NetworkedCharacterData networked_character_data = game_update[i];

        spdlog::get("network")->info("Iterating through the {}th character update it has data: \n {} ", i,
                                     networked_character_data);

        if (networked_character_data.client_id == this->id) {

            if (networked_character_data.cihtems_of_last_server_processed_input_snapshot == -1) {
                printf("got minus one BAD!\n");
            }

            // server is authorative blindly apply the update.
            client_id_to_character_data[networked_character_data.client_id] = networked_character_data;
            JPH::Ref<JPH::CharacterVirtual> client_physics_character =
                physics.client_id_to_physics_character[networked_character_data.client_id];

            JPH::Vec3 authoriative_position = {networked_character_data.character_x_position,
                                               networked_character_data.character_y_position,
                                               networked_character_data.character_z_position};

            JPH::Vec3 authoriative_velocity = {networked_character_data.character_x_velocity,
                                               networked_character_data.character_y_velocity,
                                               networked_character_data.character_z_velocity};

            JPH::Vec3 position_with_prediction = client_physics_character->GetPosition();
            JPH::Vec3 velocity_with_prediction = client_physics_character->GetLinearVelocity();

            spdlog::get("network")->info("Before reconciliation game state was \n position: {}, velocity: {}",
                                         position_with_prediction, velocity_with_prediction);

            // camera.set_look_direction(networked_character_data.camera_yaw_angle,
            // networked_character_data.camera_pitch_angle);

            // JPH::Ref<JPH::CharacterVirtual> client_physics_character =
            //     physics.client_id_to_physics_character[networked_character_data.client_id];

            // client_physics_character.SetPosition();

            // now account for the local updates that have ocurred since then
            reconcile_local_game_state_with_server_update(networked_character_data, physics, camera, mouse,
                                                          client_id_to_character_data, processed_input_snapshot_history,
                                                          authoriative_position, authoriative_velocity);

            JPH::Vec3 position_after_reconciliation = client_physics_character->GetPosition();
            JPH::Vec3 velocity_after_reconciliation = client_physics_character->GetLinearVelocity();

            JPH::Vec3 predicted_position_diff = position_with_prediction - position_after_reconciliation;
            JPH::Vec3 predicted_velocity_diff = velocity_with_prediction - velocity_after_reconciliation;

            spdlog::get("network")->info("prediction deltas, pos: {}, vel: {}\n poslen: {}, vellen: {}",
                                         predicted_position_diff, predicted_velocity_diff,
                                         predicted_position_diff.Length(), predicted_velocity_diff.Length());

        } else { // this is not the client player, just slam that data in.
            client_id_to_character_data[networked_character_data.client_id] = networked_character_data;
        }
    }
}

int ClientNetwork::start_network_loop(int send_frequency_hz, Physics &physics, Camera &camera, Mouse &mouse,
                                      std::unordered_map<uint64_t, NetworkedCharacterData> &client_id_to_character_data,
                                      ExpiringDataContainer<NetworkedInputSnapshot> &processed_input_snapshot_history) {
    // ENetEvent event;
    //
    // bool first_iteration = true;
    // std::chrono::steady_clock::time_point time_of_last_input_snapshot_send;
    // float send_period_sec = 1.0 / send_frequency_hz;
    // int send_period_ms = send_period_sec * 1000;
    //
    // while (true) {
    //     /* Wait for an event. (WARNING: blocking, which is ok, since this is in it's own thread) */
    //     if (enet_host_service(this->client, &event, send_period_ms) > 0) {
    //         switch (event.type) {
    //         case ENET_EVENT_TYPE_CONNECT: // is this even possible on the client?
    //             // spdlog::get("network")
    //             //     ->info("A new client connected from {}:{}", event.peer->address.host,
    //             event.peer->address.port);
    //
    //             printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
    //             /* Store any relevant client information here. */
    //             event.peer->data = (void *)"Client information";
    //             break;
    //
    //         case ENET_EVENT_TYPE_RECEIVE: {
    //
    //             if (event.packet->dataLength == sizeof(uint64_t)) { // client id
    //                 uint64_t receivedID = *reinterpret_cast<const uint64_t *>(event.packet->data);
    //                 this->id = receivedID;
    //                 physics.create_character(receivedID);
    //
    //                 spdlog::get("network")->info("Received unique ID from server: {}", receivedID);
    //                 // printf("Received unique ID from server: %lu\n", receivedID);
    //             }
    //
    //             bool client_id_received_already = id != -1;
    //             if (client_id_received_already) { // everything after the client id is a game state update
    //                 NetworkedCharacterData *game_update =
    //                     reinterpret_cast<NetworkedCharacterData *>(event.packet->data);
    //                 size_t game_update_length = event.packet->dataLength / sizeof(NetworkedCharacterData);
    //
    //                 std::ostringstream first_look;
    //                 for (size_t i = 0; i < game_update_length; ++i) {
    //                     NetworkedCharacterData networked_character_data = game_update[i];
    //                     first_look << networked_character_data;
    //                 }
    //
    //                 spdlog::get("network")->info(
    //                     "Just received a game update containing {} individual character updates with data {}",
    //                     game_update_length, first_look.str());
    //
    //                 for (size_t i = 0; i < game_update_length; ++i) {
    //                     NetworkedCharacterData networked_character_data = game_update[i];
    //
    //                     std::ostringstream oss;
    //                     oss << networked_character_data;
    //                     std::string struct_string = oss.str();
    //                     spdlog::get("network")->info("Iterating through the {}th character update it has data: {} ",
    //                     i,
    //                                                  struct_string);
    //
    //                     // bool client_has_given_character = physics.
    //                     //
    //                     // if (networked_character_data.client_id
    //                     //
    //
    //                     if (networked_character_data.client_id == this->id) {
    //
    //                         if (networked_character_data.cihtems_of_last_server_processed_input_snapshot == -1) {
    //                             printf("got minus one BAD!\n");
    //                         }
    //
    //                         // server is authorative blindly apply the update.
    //                         client_id_to_character_data[networked_character_data.client_id] =
    //                         networked_character_data; JPH::Ref<JPH::CharacterVirtual> client_physics_character =
    //                             physics.client_id_to_physics_character[networked_character_data.client_id];
    //
    //                         JPH::Vec3 authoriative_position = {networked_character_data.character_x_position,
    //                                                            networked_character_data.character_y_position,
    //                                                            networked_character_data.character_z_position};
    //
    //                         JPH::Vec3 authoriative_velocity = {networked_character_data.character_x_velocity,
    //                                                            networked_character_data.character_y_velocity,
    //                                                            networked_character_data.character_z_velocity};
    //
    //                         JPH::Vec3 position_with_prediction = client_physics_character->GetPosition();
    //                         JPH::Vec3 velocity_with_prediction = client_physics_character->GetLinearVelocity();
    //
    //                         std::ostringstream x;
    //                         x << position_with_prediction;
    //                         std::string pos_str = x.str();
    //
    //                         std::ostringstream y;
    //                         y << velocity_with_prediction;
    //                         std::string vel_str = y.str();
    //
    //                         spdlog::get("network")->info("Before reconciliation pos was {} vel was {}", pos_str,
    //                                                      vel_str);
    //
    //                         client_physics_character->SetPosition(authoriative_position);
    //                         client_physics_character->SetLinearVelocity(authoriative_velocity);
    //
    //                         // physics.update_characters_only(0.00001); // to move the player
    //
    //                         std::ostringstream x1;
    //                         x1 << authoriative_position;
    //                         pos_str = x1.str();
    //
    //                         std::ostringstream y1;
    //                         y1 << authoriative_velocity;
    //                         vel_str = y1.str();
    //
    //                         spdlog::get("network")->info("Authorative pos to {} vel {}", pos_str, vel_str);
    //
    //                         // camera.set_look_direction(networked_character_data.camera_yaw_angle,
    //                         // networked_character_data.camera_pitch_angle);
    //
    //                         // JPH::Ref<JPH::CharacterVirtual> client_physics_character =
    //                         //     physics.client_id_to_physics_character[networked_character_data.client_id];
    //
    //                         // client_physics_character.SetPosition();
    //
    //                         // now account for the local updates that have ocurred since then
    //                         reconcile_local_game_state_with_server_update(networked_character_data, physics, camera,
    //                                                                       mouse, client_id_to_character_data,
    //                                                                       processed_input_snapshot_history);
    //
    //                         JPH::Vec3 position_after_reconciliation = client_physics_character->GetPosition();
    //                         JPH::Vec3 velocity_after_reconciliation = client_physics_character->GetLinearVelocity();
    //
    //                         physics.update_characters_only(0.001);
    //
    //                         JPH::Vec3 predicted_position_diff =
    //                             position_with_prediction - position_after_reconciliation;
    //                         JPH::Vec3 predicted_velocity_diff =
    //                             velocity_with_prediction - velocity_after_reconciliation;
    //
    //                         std::ostringstream x2;
    //                         x2 << predicted_position_diff;
    //                         pos_str = x2.str();
    //
    //                         std::ostringstream y2;
    //                         y2 << predicted_velocity_diff;
    //                         vel_str = y2.str();
    //
    //                         spdlog::get("network")->info("prediction deltas, pos: {}, vel: {}, poslen: {}, vellen:
    //                         {}",
    //                                                      pos_str, vel_str, predicted_position_diff.Length(),
    //                                                      predicted_velocity_diff.Length());
    //
    //                     } else { // this is not the client player, just slam that data in.
    //                         client_id_to_character_data[networked_character_data.client_id] =
    //                         networked_character_data;
    //                     }
    //
    //                     // print_current_time();
    //                     // printf("<~~~ received id: %lu is client: %b, pos: (%f, %f, %f) look: (%f, %f) \n",
    //                     //        networked_character_data.client_id, networked_character_data.client_id == this->id,
    //                     //        networked_character_data.character_x_position,
    //                     //        networked_character_data.character_y_position,
    //                     //        networked_character_data.character_z_position,
    //                     //        networked_character_data.camera_yaw_angle,
    //                     //        networked_character_data.camera_pitch_angle);
    //                     // if (player_data.client_id == this->id) {
    //                     //     printf("got %f %f %f\n", player_data.character_x_position,
    //                     //     player_data.character_y_position,
    //                     //            player_data.character_z_position);
    //                     //     character_position->x = player_data.character_x_position;
    //                     //     character_position->y = player_data.character_y_position;
    //                     //     character_position->z = player_data.character_z_position;
    //                     //     camera->set_look_direction(player_data.camera_yaw_angle,
    //                     //     player_data.camera_pitch_angle);
    //                     // }
    //                 }
    //             }
    //             /* Clean up the packet now that we're done using it. */
    //             enet_packet_destroy(event.packet);
    //         } break;
    //
    //         case ENET_EVENT_TYPE_DISCONNECT:
    //             printf("%s disconnected.\n", event.peer->data);
    //             /* Reset the peer's client information. */
    //             event.peer->data = NULL;
    //             break;
    //
    //         case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
    //             printf("%s disconnected due to timeout.\n", event.peer->data);
    //             /* Reset the peer's client information. */
    //             event.peer->data = NULL;
    //             break;
    //
    //         case ENET_EVENT_TYPE_NONE:
    //             break;
    //         }
    //     }
    //
    //     if (first_iteration) {
    //         time_of_last_input_snapshot_send = std::chrono::steady_clock::now();
    //         first_iteration = false;
    //     } else if (this->id != -1) {
    //         // if we 're not in the first iteration and we've received our client id from the server, then we can
    //         // send out data. otherwise keep waiting
    //         auto current_time = std::chrono::steady_clock::now();
    //         std::chrono::duration<double> time_since_last_input_snapshot_send_sec =
    //             current_time - time_of_last_input_snapshot_send;
    //         if (time_since_last_input_snapshot_send_sec.count() >= send_period_sec) {
    //             // print_current_time();
    //             send_input_snapshot(processed_input_snapshot_history);
    //             time_of_last_input_snapshot_send = current_time;
    //         }
    //     }
    // }

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

    spdlog::get("network")->info("~~~> sending input snapshot, {}", most_recently_added_processed_snapshot);
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
