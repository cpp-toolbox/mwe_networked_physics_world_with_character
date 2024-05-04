#include "server.hpp"
#include "interaction/physics/physics.hpp"
#include <functional>
#include <stdexcept>
#include <stdio.h>
#include <string>

using std::function;

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

int ServerNetwork::start_receive_loop(InputSnapshot *input_snapshot) {
    ENetEvent event;
    int wait_time_milliseconds = 100;

    while (true) {
        /* Wait for an event. (WARNING: blocking, which is ok, since this is in it's own thread) */
        while (enet_host_service(this->server, &event, wait_time_milliseconds) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
                /* Store any relevant client information here. */
                event.peer->data = (void *)"Client information";
                break;

            case ENET_EVENT_TYPE_RECEIVE: {
                printf("A packet of length %lu containing %s was received from %s on "
                       "channel %u.\n",
                       event.packet->dataLength, event.packet->data, event.peer->data, event.channelID);
                // not everything is going to be an input snapshot, but it works for now.
                bool packet_is_input_snapshot = true;
                if (packet_is_input_snapshot) {
                    unsigned int binary_input_snapshot = *event.packet->data;
                    write_binary_input_snapshot_to_input_snapshot(binary_input_snapshot, input_snapshot);
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
    }

    enet_host_destroy(server);
    enet_deinitialize();
    return 0;
}

/**
 * \note that this is run in a thread, but only uses read-only on the physics world
 */
std::function<void(double)> ServerNetwork::game_state_send_step_closure(Physics *physics) {
    return [physics, this](double time_since_last_update) {
        JPH::Vec3 character_position = physics->character->GetPosition();
        float sendable_character_pos[3] = {character_position.GetX(), character_position.GetY(),
                                           character_position.GetZ()};
        /* Create a reliable packet of size 7 containing "packet\0" */
        ENetPacket *packet =
            enet_packet_create(sendable_character_pos, sizeof(sendable_character_pos), ENET_PACKET_FLAG_RELIABLE);
        /* Send the packet to the peer over channel id 0. */
        /* One could also broadcast the packet by         */
        /* enet_host_broadcast (host, 0, packet);         */
        enet_host_broadcast(this->server, 0, packet);
        // enet_peer_send(peer, 0, packet);
        /* One could just use enet_host_service() instead. */
        // enet_host_flush(host);
    };
}
